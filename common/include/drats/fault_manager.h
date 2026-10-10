#ifndef DRATS_FAULT_MANAGER_H
#define DRATS_FAULT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DRATS_FAULT_SPI_TIMEOUT = 0,
    DRATS_FAULT_DMA_TIMEOUT,
    DRATS_FAULT_DMA_ERROR,
    DRATS_FAULT_BUFFER_OVERFLOW,
    DRATS_FAULT_TELEMETRY_CONGESTION,
    DRATS_FAULT_SENSOR_DISCONNECTED,
    DRATS_FAULT_CORRUPT_FRAME,
    DRATS_FAULT_TASK_INACTIVE,
    DRATS_FAULT_COUNT
} drats_fault_t;

typedef enum {
    DRATS_HEALTH_OK = 0,
    DRATS_HEALTH_RECOVERING,
    DRATS_HEALTH_DEGRADED,
    DRATS_HEALTH_RESET_REQUIRED
} drats_health_state_t;

typedef enum {
    DRATS_ACTION_NONE = 0,
    DRATS_ACTION_RESTART_PERIPHERAL,
    DRATS_ACTION_REPROBE_SENSOR,
    DRATS_ACTION_THROTTLE_TELEMETRY,
    DRATS_ACTION_STOP_ACQUISITION,
    DRATS_ACTION_REQUEST_RESET
} drats_recovery_action_t;

typedef struct {
    uint32_t count[DRATS_FAULT_COUNT];
    uint8_t consecutive[DRATS_FAULT_COUNT];
    drats_health_state_t health;
    drats_fault_t active_fault;
    bool recovery_pending;
} drats_fault_manager_t;

void drats_fault_manager_init(drats_fault_manager_t *manager);
drats_recovery_action_t drats_fault_record(drats_fault_manager_t *manager,
                                           drats_fault_t fault);
bool drats_fault_recovery_result(drats_fault_manager_t *manager,
                                 drats_fault_t fault,
                                 bool revalidated);

#endif
