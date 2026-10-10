#include "tim2.h"

#include "stm32h563xx.h"

#include <stdbool.h>

static volatile uint32_t overflow_count;
static bool initialized;

drats_status_t drats_tim2_init(uint32_t timer_clock_hz) {
    if ((timer_clock_hz < UINT32_C(1000000)) ||
        ((timer_clock_hz % UINT32_C(1000000)) != 0U)) {
        return DRATS_E_ARGUMENT;
    }
    const uint32_t divider = timer_clock_hz / UINT32_C(1000000);
    if (divider > UINT16_MAX) {
        return DRATS_E_ARGUMENT;
    }
    RCC->APB1LENR |= RCC_APB1LENR_TIM2EN;
    (void)RCC->APB1LENR;
    TIM2->CR1 = 0U;
    TIM2->PSC = divider - 1U;
    TIM2->ARR = UINT32_MAX;
    TIM2->CNT = 0U;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;
    TIM2->DIER = TIM_DIER_UIE;
    overflow_count = 0U;
    NVIC_SetPriority(TIM2_IRQn, 6U);
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;
    initialized = true;
    return DRATS_OK;
}

void drats_tim2_shutdown(void) {
    NVIC_DisableIRQ(TIM2_IRQn);
    TIM2->CR1 = 0U;
    TIM2->DIER = 0U;
    RCC->APB1LENR &= ~RCC_APB1LENR_TIM2EN;
    initialized = false;
}

uint64_t drats_tim2_now_us(void) {
    if (!initialized) {
        return 0U;
    }
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t high = overflow_count;
    uint32_t low = TIM2->CNT;
    if ((TIM2->SR & TIM_SR_UIF) != 0U) {
        ++high;
        low = TIM2->CNT;
    }
    __set_PRIMASK(primask);
    return ((uint64_t)high << 32U) | low;
}

void drats_tim2_irq(void) {
    if ((TIM2->SR & TIM_SR_UIF) != 0U) {
        TIM2->SR &= ~TIM_SR_UIF;
        ++overflow_count;
    }
}
