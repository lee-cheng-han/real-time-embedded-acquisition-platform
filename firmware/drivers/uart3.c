#include "uart3.h"

#include "stm32h563xx.h"

#include <stdbool.h>
#include <stdatomic.h>

#define UART_TX_CAPACITY 2048U
#define UART_ERROR_MASK (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)

static uint8_t tx_storage[UART_TX_CAPACITY];
static atomic_size_t tx_head;
static atomic_size_t tx_tail;
static drats_uart3_counters_t counters;
static bool initialized;

static size_t used_space(size_t head, size_t tail) {
    return (head + UART_TX_CAPACITY - tail) % UART_TX_CAPACITY;
}

drats_status_t drats_uart3_init(uint32_t peripheral_clock_hz,
                                uint32_t baud_rate) {
    if ((peripheral_clock_hz == 0U) || (baud_rate == 0U) ||
        (baud_rate > peripheral_clock_hz)) {
        return DRATS_E_ARGUMENT;
    }
    atomic_init(&tx_head, 0U);
    atomic_init(&tx_tail, 0U);
    counters = (drats_uart3_counters_t){0};

    RCC->AHB2ENR |= RCC_AHB2ENR_GPIODEN;
    RCC->APB1LENR |= RCC_APB1LENR_USART3EN;
    (void)RCC->APB1LENR;

    const uint32_t pin8_shift = 8U * 2U;
    const uint32_t pin9_shift = 9U * 2U;
    GPIOD->MODER = (GPIOD->MODER &
                    ~((UINT32_C(3) << pin8_shift) |
                      (UINT32_C(3) << pin9_shift))) |
                   (UINT32_C(2) << pin8_shift) |
                   (UINT32_C(2) << pin9_shift);
    GPIOD->OTYPER &= ~((UINT32_C(1) << 8U) | (UINT32_C(1) << 9U));
    GPIOD->OSPEEDR = (GPIOD->OSPEEDR &
                      ~((UINT32_C(3) << pin8_shift) |
                        (UINT32_C(3) << pin9_shift))) |
                     (UINT32_C(2) << pin8_shift) |
                     (UINT32_C(2) << pin9_shift);
    GPIOD->PUPDR = (GPIOD->PUPDR &
                    ~((UINT32_C(3) << pin8_shift) |
                      (UINT32_C(3) << pin9_shift))) |
                   (UINT32_C(1) << pin9_shift);
    GPIOD->AFR[1] = (GPIOD->AFR[1] & ~UINT32_C(0x000000ff)) |
                    UINT32_C(0x00000077);

    USART3->CR1 = 0U;
    USART3->CR2 = 0U;
    USART3->CR3 = 0U;
    USART3->PRESC = 0U;
    USART3->BRR = (peripheral_clock_hz + (baud_rate / 2U)) / baud_rate;
    USART3->ICR = UINT32_MAX;
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    NVIC_SetPriority(USART3_IRQn, 7U);
    NVIC_ClearPendingIRQ(USART3_IRQn);
    NVIC_EnableIRQ(USART3_IRQn);
    initialized = true;
    return DRATS_OK;
}

void drats_uart3_shutdown(void) {
    NVIC_DisableIRQ(USART3_IRQn);
    USART3->CR1 = 0U;
    initialized = false;
}

size_t drats_uart3_available(void) {
    const size_t head = atomic_load_explicit(&tx_head, memory_order_acquire);
    const size_t tail = atomic_load_explicit(&tx_tail, memory_order_acquire);
    return (UART_TX_CAPACITY - 1U) - used_space(head, tail);
}

drats_status_t drats_uart3_write(const uint8_t *data, size_t length) {
    if ((data == NULL) || (length == 0U) || !initialized) {
        return DRATS_E_ARGUMENT;
    }
    size_t head = atomic_load_explicit(&tx_head, memory_order_relaxed);
    const size_t tail = atomic_load_explicit(&tx_tail, memory_order_acquire);
    if (length > ((UART_TX_CAPACITY - 1U) - used_space(head, tail))) {
        ++counters.submissions_rejected;
        return DRATS_E_FULL;
    }
    for (size_t i = 0; i < length; ++i) {
        tx_storage[head] = data[i];
        head = (head + 1U) % UART_TX_CAPACITY;
    }
    atomic_store_explicit(&tx_head, head, memory_order_release);
    counters.bytes_accepted += (uint32_t)length;
    USART3->CR1 |= USART_CR1_TXEIE_TXFNFIE;
    return DRATS_OK;
}

void drats_uart3_irq(void) {
    const uint32_t status = USART3->ISR;
    if ((status & UART_ERROR_MASK) != 0U) {
        USART3->ICR = USART_ICR_PECF | USART_ICR_FECF |
                      USART_ICR_NECF | USART_ICR_ORECF;
        ++counters.hardware_errors;
    }
    if ((status & USART_ISR_TXE_TXFNF) != 0U) {
        size_t tail = atomic_load_explicit(&tx_tail, memory_order_relaxed);
        const size_t head = atomic_load_explicit(&tx_head, memory_order_acquire);
        if (tail != head) {
            USART3->TDR = tx_storage[tail];
            tail = (tail + 1U) % UART_TX_CAPACITY;
            atomic_store_explicit(&tx_tail, tail, memory_order_release);
            ++counters.bytes_transmitted;
        } else {
            USART3->CR1 &= ~USART_CR1_TXEIE_TXFNFIE;
        }
    }
}

const drats_uart3_counters_t *drats_uart3_counters(void) {
    return &counters;
}
