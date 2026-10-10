#include "spi1.h"

#include "stm32h563xx.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>

enum {
    SPI_AF = 5,
    SPI_SCK_PIN = 5,
    SPI_MISO_PIN = 6,
    SPI_MOSI_PIN = 7,
    SPI_CS_PIN = 14
};

static drats_spi1_counters_t counters;
static uint32_t cycles_per_us;
static bool initialized;
static volatile bool dma_active;
static volatile drats_status_t dma_result;
static TaskHandle_t dma_waiter;

enum {
    GPDMA_SPI1_RX_REQUEST = 6,
    GPDMA_SPI1_TX_REQUEST = 7
};

static const uint32_t dma_clearable = DMA_CFCR_TCF | DMA_CFCR_HTF |
                                      DMA_CFCR_DTEF | DMA_CFCR_ULEF |
                                      DMA_CFCR_USEF | DMA_CFCR_SUSPF |
                                      DMA_CFCR_TOF;

static uint32_t elapsed_cycles(uint32_t start) {
    return DWT->CYCCNT - start;
}

static bool timed_out(uint32_t start, uint32_t timeout_us) {
    const uint64_t limit = (uint64_t)timeout_us * cycles_per_us;
    return (uint64_t)elapsed_cycles(start) >= limit;
}

static void chip_select(bool selected) {
    GPIOD->BSRR = selected ? (UINT32_C(1) << (SPI_CS_PIN + 16U))
                           : (UINT32_C(1) << SPI_CS_PIN);
}

static uint32_t baud_divider_encoding(uint32_t peripheral_clock_hz,
                                      uint32_t maximum_bus_hz) {
    uint32_t encoding = 0U;
    uint32_t divisor = 2U;
    while (((peripheral_clock_hz / divisor) > maximum_bus_hz) &&
           (encoding < 7U)) {
        divisor <<= 1U;
        ++encoding;
    }
    return encoding;
}

drats_status_t drats_spi1_init(uint32_t peripheral_clock_hz,
                               uint32_t maximum_bus_hz) {
    if ((peripheral_clock_hz == 0U) || (maximum_bus_hz == 0U) ||
        ((peripheral_clock_hz / 256U) > maximum_bus_hz)) {
        return DRATS_E_ARGUMENT;
    }
    counters = (drats_spi1_counters_t){0};
    cycles_per_us = (SystemCoreClock + UINT32_C(999999)) / UINT32_C(1000000);
    if (cycles_per_us == 0U) {
        return DRATS_E_STATE;
    }
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIODEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPDMA1EN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;

    const uint32_t spi_mode_mask = (UINT32_C(3) << (2U * SPI_SCK_PIN)) |
                                   (UINT32_C(3) << (2U * SPI_MISO_PIN)) |
                                   (UINT32_C(3) << (2U * SPI_MOSI_PIN));
    GPIOA->MODER = (GPIOA->MODER & ~spi_mode_mask) |
                   (UINT32_C(2) << (2U * SPI_SCK_PIN)) |
                   (UINT32_C(2) << (2U * SPI_MISO_PIN)) |
                   (UINT32_C(2) << (2U * SPI_MOSI_PIN));
    GPIOA->OTYPER &= ~((UINT32_C(1) << SPI_SCK_PIN) |
                       (UINT32_C(1) << SPI_MISO_PIN) |
                       (UINT32_C(1) << SPI_MOSI_PIN));
    GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~spi_mode_mask) | spi_mode_mask;
    GPIOA->PUPDR &= ~spi_mode_mask;
    const uint32_t af_mask = UINT32_C(0x0fff) << (4U * SPI_SCK_PIN);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~af_mask) |
                    (SPI_AF << (4U * SPI_SCK_PIN)) |
                    (SPI_AF << (4U * SPI_MISO_PIN)) |
                    (SPI_AF << (4U * SPI_MOSI_PIN));

    const uint32_t cs_shift = 2U * SPI_CS_PIN;
    GPIOD->MODER &= ~(UINT32_C(3) << cs_shift);
    GPIOD->OTYPER &= ~(UINT32_C(1) << SPI_CS_PIN);
    GPIOD->OSPEEDR = (GPIOD->OSPEEDR & ~(UINT32_C(3) << cs_shift)) |
                     (UINT32_C(2) << cs_shift);
    GPIOD->PUPDR &= ~(UINT32_C(3) << cs_shift);
    chip_select(false);

    SPI1->CR1 = 0U;
    SPI1->CFG1 = (UINT32_C(7) << SPI_CFG1_DSIZE_Pos) |
                 (baud_divider_encoding(peripheral_clock_hz, maximum_bus_hz)
                  << SPI_CFG1_MBR_Pos);
    SPI1->CFG2 = SPI_CFG2_MASTER | SPI_CFG2_SSM;
    SPI1->IFCR = UINT32_MAX;
    SPI1->CR1 = SPI_CR1_SSI | SPI_CR1_SPE;
    GPDMA1_Channel0->CCR = DMA_CCR_RESET;
    GPDMA1_Channel1->CCR = DMA_CCR_RESET;
    dma_active = false;
    dma_waiter = NULL;
    NVIC_SetPriority(GPDMA1_Channel0_IRQn, 6U);
    NVIC_SetPriority(GPDMA1_Channel1_IRQn, 6U);
    NVIC_ClearPendingIRQ(GPDMA1_Channel0_IRQn);
    NVIC_ClearPendingIRQ(GPDMA1_Channel1_IRQn);
    NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
    NVIC_EnableIRQ(GPDMA1_Channel1_IRQn);
    initialized = true;
    return DRATS_OK;
}

void drats_spi1_shutdown(void) {
    if (initialized) {
        NVIC_DisableIRQ(GPDMA1_Channel0_IRQn);
        NVIC_DisableIRQ(GPDMA1_Channel1_IRQn);
        GPDMA1_Channel0->CCR = DMA_CCR_RESET;
        GPDMA1_Channel1->CCR = DMA_CCR_RESET;
        chip_select(false);
        SPI1->CR1 = 0U;
        RCC->APB2ENR &= ~RCC_APB2ENR_SPI1EN;
        initialized = false;
    }
}

static void dma_abort(void) {
    GPDMA1_Channel0->CCR = DMA_CCR_RESET;
    GPDMA1_Channel1->CCR = DMA_CCR_RESET;
    SPI1->CFG1 &= ~(SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
    chip_select(false);
    dma_active = false;
}

drats_status_t drats_spi1_transfer_dma(void *context,
                                       const uint8_t *tx,
                                       uint8_t *rx,
                                       size_t length,
                                       uint32_t timeout_us) {
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return drats_spi1_transfer(context, tx, rx, length, timeout_us);
    }
    if (!initialized || (tx == NULL) || (rx == NULL) || (length == 0U) ||
        (length > UINT16_MAX) || (timeout_us == 0U) || dma_active) {
        return DRATS_E_ARGUMENT;
    }
    (void)ulTaskNotifyTakeIndexed(1U, pdTRUE, 0U);
    dma_waiter = xTaskGetCurrentTaskHandle();
    dma_result = DRATS_E_TIMEOUT;
    dma_active = true;

    GPDMA1_Channel0->CCR = DMA_CCR_RESET;
    GPDMA1_Channel1->CCR = DMA_CCR_RESET;
    GPDMA1_Channel0->CFCR = dma_clearable;
    GPDMA1_Channel1->CFCR = dma_clearable;
    GPDMA1_Channel0->CTR1 = DMA_CTR1_DINC;
    GPDMA1_Channel0->CTR2 = GPDMA_SPI1_RX_REQUEST;
    GPDMA1_Channel0->CBR1 = (uint32_t)length;
    GPDMA1_Channel0->CSAR = (uint32_t)(uintptr_t)&SPI1->RXDR;
    GPDMA1_Channel0->CDAR = (uint32_t)(uintptr_t)rx;
    GPDMA1_Channel1->CTR1 = DMA_CTR1_SINC;
    GPDMA1_Channel1->CTR2 = GPDMA_SPI1_TX_REQUEST | DMA_CTR2_DREQ;
    GPDMA1_Channel1->CBR1 = (uint32_t)length;
    GPDMA1_Channel1->CSAR = (uint32_t)(uintptr_t)tx;
    GPDMA1_Channel1->CDAR = (uint32_t)(uintptr_t)&SPI1->TXDR;

    SPI1->IFCR = UINT32_MAX;
    SPI1->CR2 = (uint32_t)length;
    SPI1->CFG1 |= SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN;
    chip_select(true);
    __DMB();
    GPDMA1_Channel0->CCR = DMA_CCR_TCIE | DMA_CCR_DTEIE |
                           DMA_CCR_ULEIE | DMA_CCR_USEIE | DMA_CCR_EN;
    GPDMA1_Channel1->CCR = DMA_CCR_DTEIE | DMA_CCR_ULEIE |
                           DMA_CCR_USEIE | DMA_CCR_EN;
    SPI1->CR1 |= SPI_CR1_CSTART;

    const TickType_t wait_ticks =
        pdMS_TO_TICKS((timeout_us + 999U) / 1000U) + 1U;
    if (ulTaskNotifyTakeIndexed(1U, pdTRUE, wait_ticks) == 0U) {
        taskENTER_CRITICAL();
        if (dma_active) {
            dma_abort();
        }
        taskEXIT_CRITICAL();
        ++counters.timeouts;
        return DRATS_E_TIMEOUT;
    }
    if (dma_result == DRATS_OK) {
        ++counters.transfers;
        counters.bytes += (uint32_t)length;
    } else {
        ++counters.hardware_errors;
    }
    return dma_result;
}

void drats_spi1_dma_irq(void) {
    const uint32_t rx_status = GPDMA1_Channel0->CSR;
    const uint32_t tx_status = GPDMA1_Channel1->CSR;
    GPDMA1_Channel0->CFCR = rx_status & dma_clearable;
    GPDMA1_Channel1->CFCR = tx_status & dma_clearable;
    const uint32_t errors = DMA_CSR_DTEF | DMA_CSR_ULEF | DMA_CSR_USEF;
    if (!dma_active || ((((rx_status | tx_status) & errors) == 0U) &&
                        ((rx_status & DMA_CSR_TCF) == 0U))) {
        return;
    }
    dma_result = (((rx_status | tx_status) & errors) != 0U) ?
                 DRATS_E_IO : DRATS_OK;
    dma_abort();
    __DMB();
    __DSB();
    BaseType_t higher_priority_woken = pdFALSE;
    if (dma_waiter != NULL) {
        vTaskNotifyGiveIndexedFromISR(dma_waiter, 1U, &higher_priority_woken);
    }
    portYIELD_FROM_ISR(higher_priority_woken);
}

drats_status_t drats_spi1_transfer(void *context,
                                   const uint8_t *tx,
                                   uint8_t *rx,
                                   size_t length,
                                   uint32_t timeout_us) {
    (void)context;
    if (!initialized || (tx == NULL) || (rx == NULL) || (length == 0U) ||
        (length > UINT16_MAX) || (timeout_us == 0U)) {
        return DRATS_E_ARGUMENT;
    }
    SPI1->IFCR = UINT32_MAX;
    SPI1->CR2 = (uint32_t)length;
    chip_select(true);
    SPI1->CR1 |= SPI_CR1_CSTART;

    size_t sent = 0U;
    size_t received = 0U;
    const uint32_t start = DWT->CYCCNT;
    while (received < length) {
        const uint32_t status = SPI1->SR;
        if ((sent < length) && ((status & SPI_SR_TXP) != 0U)) {
            *(volatile uint8_t *)&SPI1->TXDR = tx[sent++];
        }
        if ((status & SPI_SR_RXP) != 0U) {
            rx[received++] = *(volatile uint8_t *)&SPI1->RXDR;
        }
        if ((status & (SPI_SR_OVR | SPI_SR_UDR)) != 0U) {
            ++counters.hardware_errors;
            SPI1->IFCR = SPI_IFCR_OVRC | SPI_IFCR_UDRC;
            chip_select(false);
            return DRATS_E_IO;
        }
        if (timed_out(start, timeout_us)) {
            ++counters.timeouts;
            chip_select(false);
            SPI1->CR1 &= ~SPI_CR1_SPE;
            SPI1->CR1 |= SPI_CR1_SSI | SPI_CR1_SPE;
            return DRATS_E_TIMEOUT;
        }
    }
    while ((SPI1->SR & SPI_SR_EOT) == 0U) {
        if (timed_out(start, timeout_us)) {
            ++counters.timeouts;
            chip_select(false);
            SPI1->CR1 &= ~SPI_CR1_SPE;
            SPI1->CR1 |= SPI_CR1_SSI | SPI_CR1_SPE;
            return DRATS_E_TIMEOUT;
        }
    }
    SPI1->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC;
    chip_select(false);
    ++counters.transfers;
    counters.bytes += (uint32_t)length;
    return DRATS_OK;
}

void drats_spi1_delay_us(void *context, uint32_t delay_us) {
    (void)context;
    const uint32_t start = DWT->CYCCNT;
    while ((uint64_t)elapsed_cycles(start) <
           ((uint64_t)delay_us * cycles_per_us)) {
        __NOP();
    }
}

const drats_spi1_counters_t *drats_spi1_counters(void) {
    return &counters;
}
