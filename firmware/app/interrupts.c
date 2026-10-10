#include "FreeRTOS.h"
#include "task.h"
#include "stm32h563xx.h"
#include "uart3.h"
#include "tim2.h"
#include "spi1.h"
#include "sample_timer.h"

#include <stdint.h>

static volatile uint32_t unexpected_irq_count;

void GPDMA1_Channel0_IRQHandler(void) {
    drats_spi1_dma_irq();
}

void GPDMA1_Channel1_IRQHandler(void) {
    drats_spi1_dma_irq();
}

void SPI1_IRQHandler(void) {
    ++unexpected_irq_count;
}

void TIM2_IRQHandler(void) {
    drats_tim2_irq();
}

void TIM3_IRQHandler(void) {
    drats_sample_timer_irq();
}

void USART3_IRQHandler(void) {
    drats_uart3_irq();
}
