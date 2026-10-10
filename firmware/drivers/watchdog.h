#ifndef DRATS_WATCHDOG_H
#define DRATS_WATCHDOG_H

#include "drats/status.h"

#include <stdbool.h>
#include <stdint.h>

drats_status_t drats_watchdog_start(uint32_t timeout_ms);
void drats_watchdog_kick(void);
uint32_t drats_reset_reason_capture(void);
bool drats_reset_was_watchdog(uint32_t reset_flags);

#endif
