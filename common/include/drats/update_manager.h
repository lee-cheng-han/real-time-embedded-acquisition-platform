#ifndef DRATS_UPDATE_MANAGER_H
#define DRATS_UPDATE_MANAGER_H

#include "drats/status.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DRATS_UPDATE_EMPTY = 0,
    DRATS_UPDATE_DOWNLOADING,
    DRATS_UPDATE_VERIFIED,
    DRATS_UPDATE_PENDING,
    DRATS_UPDATE_CONFIRMED,
    DRATS_UPDATE_REJECTED
} drats_update_state_t;

typedef struct {
    drats_update_state_t state;
    uint32_t expected_size;
    uint32_t durable_bytes;
    uint32_t boot_attempts;
} drats_update_manager_t;

void drats_update_init(drats_update_manager_t *manager);
drats_status_t drats_update_begin(drats_update_manager_t *manager,
                                  uint32_t expected_size);
drats_status_t drats_update_record_durable_bytes(drats_update_manager_t *manager,
                                                 uint32_t durable_bytes);
drats_status_t drats_update_verification_result(drats_update_manager_t *manager,
                                                bool verified);
drats_status_t drats_update_mark_boot_pending(drats_update_manager_t *manager);
drats_status_t drats_update_record_boot(drats_update_manager_t *manager,
                                        uint32_t maximum_attempts);
drats_status_t drats_update_confirm(drats_update_manager_t *manager);

#endif
