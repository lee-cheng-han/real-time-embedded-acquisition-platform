#include "drats/update_manager.h"

#include <string.h>

void drats_update_init(drats_update_manager_t *manager) {
    if (manager != NULL) {
        memset(manager, 0, sizeof(*manager));
        manager->state = DRATS_UPDATE_EMPTY;
    }
}

drats_status_t drats_update_begin(drats_update_manager_t *manager,
                                  uint32_t expected_size) {
    if ((manager == NULL) || (expected_size == 0U) ||
        ((manager->state != DRATS_UPDATE_EMPTY) &&
         (manager->state != DRATS_UPDATE_REJECTED))) {
        return DRATS_E_STATE;
    }
    manager->expected_size = expected_size;
    manager->durable_bytes = 0U;
    manager->boot_attempts = 0U;
    manager->state = DRATS_UPDATE_DOWNLOADING;
    return DRATS_OK;
}

drats_status_t drats_update_record_durable_bytes(drats_update_manager_t *manager,
                                                 uint32_t durable_bytes) {
    if ((manager == NULL) || (manager->state != DRATS_UPDATE_DOWNLOADING) ||
        (durable_bytes < manager->durable_bytes) ||
        (durable_bytes > manager->expected_size)) {
        return DRATS_E_STATE;
    }
    manager->durable_bytes = durable_bytes;
    return DRATS_OK;
}

drats_status_t drats_update_verification_result(drats_update_manager_t *manager,
                                                bool verified) {
    if ((manager == NULL) || (manager->state != DRATS_UPDATE_DOWNLOADING) ||
        (manager->durable_bytes != manager->expected_size)) {
        return DRATS_E_STATE;
    }
    manager->state = verified ? DRATS_UPDATE_VERIFIED : DRATS_UPDATE_REJECTED;
    return verified ? DRATS_OK : DRATS_E_CRC;
}

drats_status_t drats_update_mark_boot_pending(drats_update_manager_t *manager) {
    if ((manager == NULL) || (manager->state != DRATS_UPDATE_VERIFIED)) {
        return DRATS_E_STATE;
    }
    manager->state = DRATS_UPDATE_PENDING;
    return DRATS_OK;
}

drats_status_t drats_update_record_boot(drats_update_manager_t *manager,
                                        uint32_t maximum_attempts) {
    if ((manager == NULL) || (manager->state != DRATS_UPDATE_PENDING) ||
        (maximum_attempts == 0U)) {
        return DRATS_E_STATE;
    }
    ++manager->boot_attempts;
    if (manager->boot_attempts > maximum_attempts) {
        manager->state = DRATS_UPDATE_REJECTED;
        return DRATS_E_STATE;
    }
    return DRATS_OK;
}

drats_status_t drats_update_confirm(drats_update_manager_t *manager) {
    if ((manager == NULL) || (manager->state != DRATS_UPDATE_PENDING) ||
        (manager->boot_attempts == 0U)) {
        return DRATS_E_STATE;
    }
    manager->state = DRATS_UPDATE_CONFIRMED;
    return DRATS_OK;
}
