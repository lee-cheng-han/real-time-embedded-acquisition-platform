#ifndef DRATS_BOOT_RECORD_H
#define DRATS_BOOT_RECORD_H

#include "drats/status.h"
#include "drats/update_manager.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DRATS_BOOT_RECORD_SIZE 32U
#define DRATS_BOOT_SLOT_NONE 0xffU

typedef struct {
    uint32_t generation;
    drats_update_manager_t update;
    uint32_t security_counter;
    uint8_t active_slot;
    uint8_t pending_slot;
} drats_boot_record_t;

drats_status_t drats_boot_record_encode(
    const drats_boot_record_t *record,
    uint8_t output[DRATS_BOOT_RECORD_SIZE]);
drats_status_t drats_boot_record_commit(
    uint8_t record[DRATS_BOOT_RECORD_SIZE]);
drats_status_t drats_boot_record_decode(
    const uint8_t *data,
    size_t length,
    drats_boot_record_t *record);
drats_status_t drats_boot_record_select(
    const uint8_t *slot_a,
    size_t slot_a_length,
    const uint8_t *slot_b,
    size_t slot_b_length,
    drats_boot_record_t *record,
    unsigned *selected_slot);
drats_status_t drats_boot_decide(drats_boot_record_t *record,
                                 const bool slot_valid[2],
                                 uint32_t maximum_attempts,
                                 unsigned *boot_slot);

#endif
