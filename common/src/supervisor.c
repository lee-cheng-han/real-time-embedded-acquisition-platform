#include "drats/supervisor.h"

#include <stddef.h>
#include <string.h>

drats_status_t drats_supervisor_init(
    drats_supervisor_t *supervisor,
    const uint32_t deadline_ms[DRATS_SUPERVISOR_TASK_COUNT],
    uint32_t required_mask) {
    const uint32_t valid_mask = (UINT32_C(1) << DRATS_SUPERVISOR_TASK_COUNT) - 1U;
    if ((supervisor == NULL) || (deadline_ms == NULL) ||
        ((required_mask & ~valid_mask) != 0U)) {
        return DRATS_E_ARGUMENT;
    }
    memset(supervisor, 0, sizeof(*supervisor));
    supervisor->required_mask = required_mask;
    for (size_t i = 0; i < DRATS_SUPERVISOR_TASK_COUNT; ++i) {
        if (((required_mask & (UINT32_C(1) << i)) != 0U) &&
            (deadline_ms[i] == 0U)) {
            return DRATS_E_ARGUMENT;
        }
        supervisor->deadline_ms[i] = deadline_ms[i];
    }
    return DRATS_OK;
}

drats_status_t drats_supervisor_heartbeat(drats_supervisor_t *supervisor,
                                          drats_task_id_t task,
                                          uint64_t now_ms) {
    if ((supervisor == NULL) || ((unsigned)task >= DRATS_SUPERVISOR_TASK_COUNT)) {
        return DRATS_E_ARGUMENT;
    }
    supervisor->last_heartbeat_ms[task] = now_ms;
    supervisor->seen_mask |= UINT32_C(1) << (unsigned)task;
    return DRATS_OK;
}

uint32_t drats_supervisor_missing(const drats_supervisor_t *supervisor,
                                  uint64_t now_ms) {
    if (supervisor == NULL) {
        return UINT32_MAX;
    }
    uint32_t missing = supervisor->required_mask & ~supervisor->seen_mask;
    for (size_t i = 0; i < DRATS_SUPERVISOR_TASK_COUNT; ++i) {
        const uint32_t bit = UINT32_C(1) << i;
        if (((supervisor->required_mask & bit) != 0U) &&
            ((supervisor->seen_mask & bit) != 0U) &&
            ((now_ms - supervisor->last_heartbeat_ms[i]) >
             supervisor->deadline_ms[i])) {
            missing |= bit;
        }
    }
    return missing;
}

bool drats_supervisor_watchdog_eligible(drats_supervisor_t *supervisor,
                                        uint64_t now_ms) {
    if (supervisor == NULL) {
        return false;
    }
    const uint32_t missing = drats_supervisor_missing(supervisor, now_ms);
    if (missing != 0U) {
        ++supervisor->inactive_events;
        return false;
    }
    return true;
}
