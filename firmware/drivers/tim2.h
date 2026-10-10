#ifndef DRATS_TIM2_H
#define DRATS_TIM2_H

#include "drats/status.h"

#include <stdint.h>

drats_status_t drats_tim2_init(uint32_t timer_clock_hz);
void drats_tim2_shutdown(void);
uint64_t drats_tim2_now_us(void);
void drats_tim2_irq(void);

#endif
