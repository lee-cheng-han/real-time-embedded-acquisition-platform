#ifndef DRATS_INTERRUPTS_H
#define DRATS_INTERRUPTS_H

#include "FreeRTOS.h"
#include "task.h"

void drats_irq_set_acquisition_task(TaskHandle_t task);

#endif
