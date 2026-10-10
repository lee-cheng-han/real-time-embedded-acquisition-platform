#include "FreeRTOS.h"
#include "task.h"
#include "stm32h563xx.h"
#include "uart3.h"

#include <stdint.h>

static TaskHandle_t acquisition_task_handle;
static volatile uint32_t unexpected_irq_count;

void drats_irq_set_acquisition_task(TaskHandle_t task) {
    acquisition_task_handle = task;
}

void GPDMA1_Channel0_IRQHandler(void) {
    const uint32_t status = GPDMA1_Channel0->CSR;
    const uint32_t clearable = DMA_CFCR_TCF | DMA_CFCR_HTF |
                               DMA_CFCR_DTEF | DMA_CFCR_ULEF |
                               DMA_CFCR_USEF | DMA_CFCR_SUSPF |
                               DMA_CFCR_TOF;
    GPDMA1_Channel0->CFCR = status & clearable;
    __DSB();

    BaseType_t higher_priority_woken = pdFALSE;
    if (((status & DMA_CSR_TCF) != 0U) &&
        (acquisition_task_handle != NULL)) {
        vTaskNotifyGiveFromISR(acquisition_task_handle, &higher_priority_woken);
    }
    if ((status & (DMA_CSR_DTEF | DMA_CSR_ULEF | DMA_CSR_USEF)) != 0U) {
        ++unexpected_irq_count;
    }
    portYIELD_FROM_ISR(higher_priority_woken);
}

void SPI1_IRQHandler(void) {
    ++unexpected_irq_count;
}

void TIM2_IRQHandler(void) {
    ++unexpected_irq_count;
}

void USART3_IRQHandler(void) {
    drats_uart3_irq();
}
