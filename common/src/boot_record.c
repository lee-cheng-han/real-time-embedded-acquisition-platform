#include "drats/boot_record.h"

#include "drats/crc32c.h"

#include <string.h>

enum {
    OFFSET_MAGIC = 0,
    OFFSET_GENERATION = 4,
    OFFSET_STATE = 8,
    OFFSET_ACTIVE_SLOT = 9,
    OFFSET_PENDING_SLOT = 10,
    OFFSET_BOOT_ATTEMPTS = 11,
    OFFSET_EXPECTED_SIZE = 12,
    OFFSET_DURABLE_BYTES = 16,
    OFFSET_SECURITY_COUNTER = 20,
    OFFSET_CRC = 24,
    OFFSET_COMMIT = 28
};

static const uint32_t record_magic = UINT32_C(0x54445055);
static const uint32_t commit_magic = UINT32_C(0xc04dc0de);

static void put_u32(uint8_t *destination, uint32_t value) {
    for (unsigned i = 0U; i < 4U; ++i) {
        destination[i] = (uint8_t)(value >> (8U * i));
    }
}

static uint32_t get_u32(const uint8_t *source) {
    uint32_t value = 0U;
    for (unsigned i = 0U; i < 4U; ++i) {
        value |= (uint32_t)source[i] << (8U * i);
    }
    return value;
}

static bool slot_id_valid(uint8_t slot, bool allow_none) {
    return (slot < 2U) || (allow_none && (slot == DRATS_BOOT_SLOT_NONE));
}

static bool generation_newer(uint32_t candidate, uint32_t reference) {
    return (int32_t)(candidate - reference) > 0;
}

drats_status_t drats_boot_record_encode(
    const drats_boot_record_t *record,
    uint8_t output[DRATS_BOOT_RECORD_SIZE]) {
    if ((record == NULL) || (output == NULL) ||
        (record->update.state > DRATS_UPDATE_REJECTED) ||
        (record->update.boot_attempts > UINT8_MAX) ||
        !slot_id_valid(record->active_slot, true) ||
        !slot_id_valid(record->pending_slot, true) ||
        (record->update.durable_bytes > record->update.expected_size)) {
        return DRATS_E_ARGUMENT;
    }
    memset(output, 0xff, DRATS_BOOT_RECORD_SIZE);
    put_u32(&output[OFFSET_MAGIC], record_magic);
    put_u32(&output[OFFSET_GENERATION], record->generation);
    output[OFFSET_STATE] = (uint8_t)record->update.state;
    output[OFFSET_ACTIVE_SLOT] = record->active_slot;
    output[OFFSET_PENDING_SLOT] = record->pending_slot;
    output[OFFSET_BOOT_ATTEMPTS] = (uint8_t)record->update.boot_attempts;
    put_u32(&output[OFFSET_EXPECTED_SIZE], record->update.expected_size);
    put_u32(&output[OFFSET_DURABLE_BYTES], record->update.durable_bytes);
    put_u32(&output[OFFSET_SECURITY_COUNTER], record->security_counter);
    put_u32(&output[OFFSET_CRC], drats_crc32c(0U, output, OFFSET_CRC));
    return DRATS_OK;
}

drats_status_t drats_boot_record_commit(
    uint8_t record[DRATS_BOOT_RECORD_SIZE]) {
    if (record == NULL) {
        return DRATS_E_ARGUMENT;
    }
    put_u32(&record[OFFSET_COMMIT], commit_magic);
    return DRATS_OK;
}

drats_status_t drats_boot_record_decode(const uint8_t *data,
                                        size_t length,
                                        drats_boot_record_t *record) {
    if ((data == NULL) || (record == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    if (length != DRATS_BOOT_RECORD_SIZE) {
        return DRATS_E_SIZE;
    }
    if ((get_u32(&data[OFFSET_MAGIC]) != record_magic) ||
        (get_u32(&data[OFFSET_COMMIT]) != commit_magic) ||
        (get_u32(&data[OFFSET_CRC]) !=
         drats_crc32c(0U, data, OFFSET_CRC))) {
        return DRATS_E_CRC;
    }
    drats_boot_record_t decoded = {
        .generation = get_u32(&data[OFFSET_GENERATION]),
        .update = {
            .state = (drats_update_state_t)data[OFFSET_STATE],
            .expected_size = get_u32(&data[OFFSET_EXPECTED_SIZE]),
            .durable_bytes = get_u32(&data[OFFSET_DURABLE_BYTES]),
            .boot_attempts = data[OFFSET_BOOT_ATTEMPTS]
        },
        .security_counter = get_u32(&data[OFFSET_SECURITY_COUNTER]),
        .active_slot = data[OFFSET_ACTIVE_SLOT],
        .pending_slot = data[OFFSET_PENDING_SLOT]
    };
    if ((decoded.update.state > DRATS_UPDATE_REJECTED) ||
        !slot_id_valid(decoded.active_slot, true) ||
        !slot_id_valid(decoded.pending_slot, true) ||
        (decoded.update.durable_bytes > decoded.update.expected_size)) {
        return DRATS_E_FORMAT;
    }
    *record = decoded;
    return DRATS_OK;
}

drats_status_t drats_boot_record_select(
    const uint8_t *slot_a,
    size_t slot_a_length,
    const uint8_t *slot_b,
    size_t slot_b_length,
    drats_boot_record_t *record,
    unsigned *selected_slot) {
    if ((record == NULL) || (selected_slot == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    drats_boot_record_t a;
    drats_boot_record_t b;
    const bool a_valid = drats_boot_record_decode(
        slot_a, slot_a_length, &a) == DRATS_OK;
    const bool b_valid = drats_boot_record_decode(
        slot_b, slot_b_length, &b) == DRATS_OK;
    if (!a_valid && !b_valid) {
        return DRATS_E_EMPTY;
    }
    if (b_valid && (!a_valid || generation_newer(b.generation, a.generation))) {
        *record = b;
        *selected_slot = 1U;
    } else {
        *record = a;
        *selected_slot = 0U;
    }
    return DRATS_OK;
}

drats_status_t drats_boot_decide(drats_boot_record_t *record,
                                 const bool slot_valid[2],
                                 uint32_t maximum_attempts,
                                 unsigned *boot_slot) {
    if ((record == NULL) || (slot_valid == NULL) || (boot_slot == NULL) ||
        (maximum_attempts == 0U)) {
        return DRATS_E_ARGUMENT;
    }
    if (record->update.state == DRATS_UPDATE_PENDING) {
        if ((record->pending_slot < 2U) && slot_valid[record->pending_slot] &&
            (record->update.boot_attempts < maximum_attempts)) {
            ++record->update.boot_attempts;
            ++record->generation;
            *boot_slot = record->pending_slot;
            return DRATS_OK;
        }
        record->update.state = DRATS_UPDATE_REJECTED;
        record->pending_slot = DRATS_BOOT_SLOT_NONE;
        ++record->generation;
    }
    if ((record->active_slot < 2U) && slot_valid[record->active_slot]) {
        *boot_slot = record->active_slot;
        return DRATS_OK;
    }
    return DRATS_E_AUTH;
}
