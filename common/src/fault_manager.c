#include "drats/fault_manager.h"

#include <string.h>

void drats_fault_manager_init(drats_fault_manager_t *manager) {
    if (manager == NULL) {
        return;
    }
    memset(manager, 0, sizeof(*manager));
    manager->health = DRATS_HEALTH_OK;
}

drats_recovery_action_t drats_fault_record(drats_fault_manager_t *manager,
                                           drats_fault_t fault) {
    if ((manager == NULL) || (fault >= DRATS_FAULT_COUNT)) {
        return DRATS_ACTION_NONE;
    }
    ++manager->count[fault];
    if (manager->consecutive[fault] < UINT8_MAX) {
        ++manager->consecutive[fault];
    }
    manager->active_fault = fault;
    manager->recovery_pending = true;
    manager->health = DRATS_HEALTH_RECOVERING;

    if ((fault == DRATS_FAULT_TASK_INACTIVE) ||
        (manager->consecutive[fault] >= 5U)) {
        manager->health = DRATS_HEALTH_RESET_REQUIRED;
        return DRATS_ACTION_REQUEST_RESET;
    }
    if (fault == DRATS_FAULT_BUFFER_OVERFLOW) {
        manager->health = DRATS_HEALTH_DEGRADED;
        return DRATS_ACTION_STOP_ACQUISITION;
    }
    if (fault == DRATS_FAULT_TELEMETRY_CONGESTION) {
        return DRATS_ACTION_THROTTLE_TELEMETRY;
    }
    if (fault == DRATS_FAULT_SENSOR_DISCONNECTED) {
        return DRATS_ACTION_REPROBE_SENSOR;
    }
    if ((fault == DRATS_FAULT_SPI_TIMEOUT) ||
        (fault == DRATS_FAULT_DMA_TIMEOUT) ||
        (fault == DRATS_FAULT_DMA_ERROR)) {
        return DRATS_ACTION_RESTART_PERIPHERAL;
    }
    manager->recovery_pending = false;
    manager->health = DRATS_HEALTH_OK;
    manager->consecutive[fault] = 0U;
    return DRATS_ACTION_NONE;
}

bool drats_fault_recovery_result(drats_fault_manager_t *manager,
                                 drats_fault_t fault,
                                 bool revalidated) {
    if ((manager == NULL) || (fault >= DRATS_FAULT_COUNT) ||
        !manager->recovery_pending || (manager->active_fault != fault)) {
        return false;
    }
    if (!revalidated) {
        manager->health = DRATS_HEALTH_DEGRADED;
        return false;
    }
    manager->consecutive[fault] = 0U;
    manager->recovery_pending = false;
    manager->health = DRATS_HEALTH_OK;
    return true;
}
