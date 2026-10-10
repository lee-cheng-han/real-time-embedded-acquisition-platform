#ifndef DRATS_SUPERVISOR_H
#define DRATS_SUPERVISOR_H

#include "drats/status.h"

#include <stdbool.h>
#include <stdint.h>

#define DRATS_SUPERVISOR_TASK_COUNT 5U

typedef enum {
    DRATS_TASK_ACQUISITION = 0,
    DRATS_TASK_PROCESSING,
    DRATS_TASK_TELEMETRY,
    DRATS_TASK_DIAGNOSTICS,
    DRATS_TASK_SUPERVISOR
} drats_task_id_t;

typedef struct {
    uint64_t last_heartbeat_ms[DRATS_SUPERVISOR_TASK_COUNT];
    uint32_t deadline_ms[DRATS_SUPERVISOR_TASK_COUNT];
    uint32_t seen_mask;
    uint32_t required_mask;
    uint32_t inactive_events;
} drats_supervisor_t;

drats_status_t drats_supervisor_init(
    drats_supervisor_t *supervisor,
    const uint32_t deadline_ms[DRATS_SUPERVISOR_TASK_COUNT],
    uint32_t required_mask);
drats_status_t drats_supervisor_heartbeat(drats_supervisor_t *supervisor,
                                          drats_task_id_t task,
                                          uint64_t now_ms);
uint32_t drats_supervisor_missing(const drats_supervisor_t *supervisor,
                                  uint64_t now_ms);
bool drats_supervisor_watchdog_eligible(drats_supervisor_t *supervisor,
                                        uint64_t now_ms);

#endif
