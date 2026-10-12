#include "sample_timer.h"

#include "stm32h563xx.h"
#include "timing_trace.h"

#include <stdbool.h>

static TaskHandle_t target_task;
static bool initialized;

drats_status_t drats_sample_timer_init(uint32_t timer_clock_hz,
                                       uint32_t sample_rate_hz) {
    if ((timer_clock_hz < UINT32_C(1000000)) ||
        ((timer_clock_hz % UINT32_C(1000000)) != 0U) ||
        (sample_rate_hz == 0U) ||
        ((UINT32_C(1000000) % sample_rate_hz) != 0U)) {
        return DRATS_E_ARGUMENT;
    }
    const uint32_t prescaler = timer_clock_hz / UINT32_C(1000000);
    const uint32_t period = UINT32_C(1000000) / sample_rate_hz;
    if ((prescaler > UINT16_MAX) || (period > UINT16_MAX)) {
        return DRATS_E_ARGUMENT;
    }
    RCC->APB1LENR |= RCC_APB1LENR_TIM3EN;
    (void)RCC->APB1LENR;
    TIM3->CR1 = 0U;
    TIM3->PSC = prescaler - 1U;
    TIM3->ARR = period - 1U;
    TIM3->CNT = 0U;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->SR = 0U;
    TIM3->DIER = TIM_DIER_UIE;
    target_task = NULL;
    NVIC_SetPriority(TIM3_IRQn, 6U);
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    NVIC_EnableIRQ(TIM3_IRQn);
    initialized = true;
    return DRATS_OK;
}

drats_status_t drats_sample_timer_start(TaskHandle_t acquisition_task) {
    if (!initialized || (acquisition_task == NULL)) {
        return DRATS_E_STATE;
    }
    target_task = acquisition_task;
    TIM3->CNT = 0U;
    TIM3->SR = 0U;
    TIM3->CR1 = TIM_CR1_CEN;
    return DRATS_OK;
}

void drats_sample_timer_stop(void) {
    TIM3->CR1 = 0U;
    target_task = NULL;
}

void drats_sample_timer_irq(void) {
    if ((TIM3->SR & TIM_SR_UIF) == 0U) {
        return;
    }
    TIM3->SR &= ~TIM_SR_UIF;
    drats_timing_trace_record(DRATS_TRACE_SAMPLE_TRIGGER, 0U);
    BaseType_t higher_priority_woken = pdFALSE;
    if (target_task != NULL) {
        vTaskNotifyGiveIndexedFromISR(target_task, 0U,
                                      &higher_priority_woken);
    }
    portYIELD_FROM_ISR(higher_priority_woken);
}
