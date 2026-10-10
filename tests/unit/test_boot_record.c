#include "drats/boot_record.h"
#include "drats/boot_vector.h"
#include "test_support.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static drats_boot_record_t confirmed_record(uint32_t generation) {
    const drats_boot_record_t record = {
        .generation = generation,
        .update = {
            .state = DRATS_UPDATE_CONFIRMED,
            .expected_size = 4096U,
            .durable_bytes = 4096U,
            .boot_attempts = 1U
        },
        .security_counter = 4U,
        .active_slot = 0U,
        .pending_slot = DRATS_BOOT_SLOT_NONE
    };
    return record;
}

static int test_redundant_power_loss(void) {
    drats_boot_record_t old = confirmed_record(10U);
    drats_boot_record_t pending = {
        .generation = 11U,
        .update = {
            .state = DRATS_UPDATE_PENDING,
            .expected_size = 8192U,
            .durable_bytes = 8192U,
            .boot_attempts = 0U
        },
        .security_counter = 5U,
        .active_slot = 0U,
        .pending_slot = 1U
    };
    uint8_t old_bytes[DRATS_BOOT_RECORD_SIZE];
    uint8_t pending_uncommitted[DRATS_BOOT_RECORD_SIZE];
    CHECK(drats_boot_record_encode(&old, old_bytes) == DRATS_OK);
    CHECK(drats_boot_record_commit(old_bytes) == DRATS_OK);
    CHECK(drats_boot_record_encode(&pending, pending_uncommitted) == DRATS_OK);

    for (size_t cut = 0U; cut <= DRATS_BOOT_RECORD_SIZE - 4U; ++cut) {
        uint8_t interrupted[DRATS_BOOT_RECORD_SIZE];
        memset(interrupted, 0xff, sizeof(interrupted));
        memcpy(interrupted, pending_uncommitted, cut);
        drats_boot_record_t selected;
        unsigned selected_slot = 99U;
        CHECK(drats_boot_record_select(old_bytes, sizeof(old_bytes),
                                       interrupted, sizeof(interrupted),
                                       &selected, &selected_slot) == DRATS_OK);
        CHECK(selected_slot == 0U && selected.generation == 10U);
    }

    uint8_t committed[DRATS_BOOT_RECORD_SIZE];
    memcpy(committed, pending_uncommitted, sizeof(committed));
    const uint8_t commit_bytes[4] = {0xdeU, 0xc0U, 0x4dU, 0xc0U};
    for (size_t written = 0U; written < sizeof(commit_bytes); ++written) {
        memcpy(&committed[DRATS_BOOT_RECORD_SIZE - 4U], commit_bytes, written);
        drats_boot_record_t selected;
        unsigned selected_slot = 99U;
        CHECK(drats_boot_record_select(old_bytes, sizeof(old_bytes),
                                       committed, sizeof(committed),
                                       &selected, &selected_slot) == DRATS_OK);
        CHECK(selected_slot == 0U);
    }
    CHECK(drats_boot_record_commit(committed) == DRATS_OK);
    drats_boot_record_t selected;
    unsigned selected_slot = 99U;
    CHECK(drats_boot_record_select(old_bytes, sizeof(old_bytes),
                                   committed, sizeof(committed),
                                   &selected, &selected_slot) == DRATS_OK);
    CHECK(selected_slot == 1U && selected.generation == 11U);
    return 0;
}

static int test_boot_attempt_and_rollback(void) {
    drats_boot_record_t record = confirmed_record(20U);
    record.update.state = DRATS_UPDATE_PENDING;
    record.update.boot_attempts = 0U;
    record.pending_slot = 1U;
    const bool both_valid[2] = {true, true};
    unsigned boot_slot = 99U;
    CHECK(drats_boot_decide(&record, both_valid, 2U, &boot_slot) == DRATS_OK);
    CHECK(boot_slot == 1U && record.update.boot_attempts == 1U);
    CHECK(drats_boot_decide(&record, both_valid, 2U, &boot_slot) == DRATS_OK);
    CHECK(boot_slot == 1U && record.update.boot_attempts == 2U);
    CHECK(drats_boot_decide(&record, both_valid, 2U, &boot_slot) == DRATS_OK);
    CHECK(boot_slot == 0U);
    CHECK(record.update.state == DRATS_UPDATE_REJECTED);
    CHECK(record.pending_slot == DRATS_BOOT_SLOT_NONE);

    record = confirmed_record(UINT32_MAX);
    uint8_t older[DRATS_BOOT_RECORD_SIZE];
    uint8_t wrapped[DRATS_BOOT_RECORD_SIZE];
    CHECK(drats_boot_record_encode(&record, older) == DRATS_OK);
    CHECK(drats_boot_record_commit(older) == DRATS_OK);
    record.generation = 0U;
    CHECK(drats_boot_record_encode(&record, wrapped) == DRATS_OK);
    CHECK(drats_boot_record_commit(wrapped) == DRATS_OK);
    unsigned record_slot = 99U;
    CHECK(drats_boot_record_select(older, sizeof(older), wrapped,
                                   sizeof(wrapped), &record,
                                   &record_slot) == DRATS_OK);
    CHECK(record_slot == 1U && record.generation == 0U);

    const bool pending_invalid[2] = {true, false};
    record = confirmed_record(30U);
    record.update.state = DRATS_UPDATE_PENDING;
    record.pending_slot = 1U;
    CHECK(drats_boot_decide(&record, pending_invalid, 2U, &boot_slot) == DRATS_OK);
    CHECK(boot_slot == 0U && record.update.state == DRATS_UPDATE_REJECTED);
    return 0;
}

int main(void) {
    CHECK(test_redundant_power_loss() == 0);
    CHECK(test_boot_attempt_and_rollback() == 0);
    CHECK(drats_boot_vector_validate(0x200a0000U, 0x08020101U,
                                     0x08020000U, 0x000c0000U,
                                     0x20000000U, 0x000a0000U) == DRATS_OK);
    CHECK(drats_boot_vector_validate(0x200a0004U, 0x08020101U,
                                     0x08020000U, 0x000c0000U,
                                     0x20000000U, 0x000a0000U) == DRATS_E_FORMAT);
    CHECK(drats_boot_vector_validate(0x200a0000U, 0x08020100U,
                                     0x08020000U, 0x000c0000U,
                                     0x20000000U, 0x000a0000U) == DRATS_E_FORMAT);
    return 0;
}
