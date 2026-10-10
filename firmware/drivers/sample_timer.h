#ifndef DRATS_SAMPLE_TIMER_H
#define DRATS_SAMPLE_TIMER_H

#include "drats/status.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>

drats_status_t drats_sample_timer_init(uint32_t timer_clock_hz,
                                       uint32_t sample_rate_hz);
drats_status_t drats_sample_timer_start(TaskHandle_t acquisition_task);
void drats_sample_timer_stop(void);
void drats_sample_timer_irq(void);

#endif
