#include "drats/config_store.h"
#include "drats/config_protocol.h"
#include "drats/image.h"
#include "drats/supervisor.h"
#include "drats/update_manager.h"
#include "test_support.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static bool verifier(const drats_image_metadata_t *metadata, void *context) {
    const bool expected = *(const bool *)context;
    return expected && (metadata->digest[0] == 0x42U);
}

static int test_config_power_loss(void) {
    const drats_persistent_config_t old_config = {
        .sample_rate_hz = 10000U, .samples_per_block = 32U,
        .overflow_policy = 0U, .telemetry_enabled = 1U
    };
    const drats_persistent_config_t new_config = {
        .sample_rate_hz = 20000U, .samples_per_block = 16U,
        .overflow_policy = 1U, .telemetry_enabled = 1U
    };
    uint8_t slot_a[DRATS_CONFIG_RECORD_SIZE];
    uint8_t slot_b[DRATS_CONFIG_RECORD_SIZE];
    CHECK(drats_config_record_encode(&old_config, 7U, slot_a) == DRATS_OK);
    CHECK(drats_config_record_commit(slot_a) == DRATS_OK);
    CHECK(drats_config_record_encode(&new_config, 8U, slot_b) == DRATS_OK);
    drats_config_selection_t selected;
    unsigned slot = 99U;
    CHECK(drats_config_select_newest(slot_a, sizeof(slot_a), slot_b,
                                     sizeof(slot_b), &selected, &slot) == DRATS_OK);
    CHECK(slot == 0U);
    CHECK(selected.config.sample_rate_hz == old_config.sample_rate_hz);
    CHECK(drats_config_record_commit(slot_b) == DRATS_OK);
    CHECK(drats_config_select_newest(slot_a, sizeof(slot_a), slot_b,
                                     sizeof(slot_b), &selected, &slot) == DRATS_OK);
    CHECK(slot == 1U);
    CHECK(selected.generation == 8U);
    slot_b[12] ^= 1U;
    CHECK(drats_config_select_newest(slot_a, sizeof(slot_a), slot_b,
                                     sizeof(slot_b), &selected, &slot) == DRATS_OK);
    CHECK(slot == 0U);
    return 0;
}

static int test_image_and_update(void) {
    drats_image_metadata_t metadata;
    memset(&metadata, 0, sizeof(metadata));
    metadata.magic = DRATS_IMAGE_MAGIC;
    metadata.format_version = DRATS_IMAGE_FORMAT_VERSION;
    metadata.header_size = (uint16_t)sizeof(metadata);
    metadata.image_size = 123456U;
    metadata.security_counter = 9U;
    metadata.signature_algorithm = DRATS_SIGNATURE_ECDSA_P256_SHA256;
    metadata.signature_length = 64U;
    metadata.digest[0] = 0x42U;
    bool accepts = true;
    CHECK(drats_image_metadata_validate(&metadata, 768U * 1024U, 8U,
                                        verifier, &accepts) == DRATS_OK);
    CHECK(drats_image_metadata_validate(&metadata, 100U, 8U,
                                        verifier, &accepts) == DRATS_E_SIZE);
    CHECK(drats_image_metadata_validate(&metadata, 768U * 1024U, 10U,
                                        verifier, &accepts) == DRATS_E_VERSION);
    accepts = false;
    CHECK(drats_image_metadata_validate(&metadata, 768U * 1024U, 8U,
                                        verifier, &accepts) == DRATS_E_AUTH);

    drats_update_manager_t update;
    drats_update_init(&update);
    CHECK(drats_update_begin(&update, 1024U) == DRATS_OK);
    CHECK(drats_update_record_durable_bytes(&update, 512U) == DRATS_OK);
    CHECK(drats_update_verification_result(&update, true) == DRATS_E_STATE);
    CHECK(drats_update_record_durable_bytes(&update, 1024U) == DRATS_OK);
    CHECK(drats_update_verification_result(&update, true) == DRATS_OK);
    CHECK(drats_update_mark_boot_pending(&update) == DRATS_OK);
    CHECK(drats_update_confirm(&update) == DRATS_E_STATE);
    CHECK(drats_update_record_boot(&update, 2U) == DRATS_OK);
    CHECK(drats_update_confirm(&update) == DRATS_OK);

    drats_update_init(&update);
    CHECK(drats_update_begin(&update, 1U) == DRATS_OK);
    CHECK(drats_update_record_durable_bytes(&update, 1U) == DRATS_OK);
    CHECK(drats_update_verification_result(&update, true) == DRATS_OK);
    CHECK(drats_update_mark_boot_pending(&update) == DRATS_OK);
    CHECK(drats_update_record_boot(&update, 1U) == DRATS_OK);
    CHECK(drats_update_record_boot(&update, 1U) == DRATS_E_STATE);
    CHECK(update.state == DRATS_UPDATE_REJECTED);
    return 0;
}

static int test_supervisor(void) {
    const uint32_t deadlines[DRATS_SUPERVISOR_TASK_COUNT] = {
        10U, 20U, 50U, 1000U, 100U
    };
    const uint32_t required = (UINT32_C(1) << DRATS_TASK_ACQUISITION) |
                              (UINT32_C(1) << DRATS_TASK_PROCESSING) |
                              (UINT32_C(1) << DRATS_TASK_TELEMETRY);
    drats_supervisor_t supervisor;
    CHECK(drats_supervisor_init(&supervisor, deadlines, required) == DRATS_OK);
    CHECK(!drats_supervisor_watchdog_eligible(&supervisor, 0U));
    CHECK(drats_supervisor_heartbeat(&supervisor, DRATS_TASK_ACQUISITION, 5U) == DRATS_OK);
    CHECK(drats_supervisor_heartbeat(&supervisor, DRATS_TASK_PROCESSING, 5U) == DRATS_OK);
    CHECK(drats_supervisor_heartbeat(&supervisor, DRATS_TASK_TELEMETRY, 5U) == DRATS_OK);
    CHECK(drats_supervisor_watchdog_eligible(&supervisor, 10U));
    CHECK(!drats_supervisor_watchdog_eligible(&supervisor, 16U));
    CHECK((drats_supervisor_missing(&supervisor, 16U) &
           (UINT32_C(1) << DRATS_TASK_ACQUISITION)) != 0U);
    return 0;
}

static int test_config_protocol(void) {
    const uint8_t command_payload[DRATS_CONFIG_COMMAND_SIZE] = {
        9U, 0U, 0U, 0U,
        0x20U, 0x4eU, 0U, 0U,
        16U, 0U, 1U, 1U
    };
    drats_config_command_t command;
    CHECK(drats_config_command_decode(command_payload, sizeof(command_payload),
                                      &command) == DRATS_OK);
    CHECK(command.request_id == 9U);
    CHECK(command.config.sample_rate_hz == 20000U);
    CHECK(command.config.samples_per_block == 16U);
    CHECK(command.config.overflow_policy == DRATS_OVERFLOW_DROP_NEWEST);
    CHECK(command.telemetry_enabled);

    drats_config_ack_t ack = {
        .request_id = command.request_id,
        .status = DRATS_E_FULL,
        .config = command.config,
        .telemetry_enabled = command.telemetry_enabled
    };
    uint8_t encoded[DRATS_CONFIG_ACK_SIZE];
    CHECK(drats_config_ack_encode(&ack, encoded) == DRATS_OK);
    drats_config_ack_t decoded;
    CHECK(drats_config_ack_decode(encoded, sizeof(encoded), &decoded) == DRATS_OK);
    CHECK(decoded.request_id == ack.request_id);
    CHECK(decoded.status == DRATS_E_FULL);
    CHECK(decoded.config.sample_rate_hz == 20000U);
    const drats_config_ack_t rejected = {
        .request_id = 5U,
        .status = DRATS_E_ARGUMENT,
        .config = {0},
        .telemetry_enabled = false
    };
    CHECK(drats_config_ack_encode(&rejected, encoded) == DRATS_OK);
    CHECK(drats_config_ack_decode(encoded, sizeof(encoded), &decoded) == DRATS_OK);
    CHECK(decoded.status == DRATS_E_ARGUMENT);
    uint8_t invalid[DRATS_CONFIG_COMMAND_SIZE];
    memcpy(invalid, command_payload, sizeof(invalid));
    invalid[11] = 2U;
    CHECK(drats_config_command_decode(invalid, sizeof(invalid), &command) ==
          DRATS_E_ARGUMENT);
    return 0;
}

int main(void) {
    CHECK(test_config_power_loss() == 0);
    CHECK(test_image_and_update() == 0);
    CHECK(test_supervisor() == 0);
    CHECK(test_config_protocol() == 0);
    return 0;
}
