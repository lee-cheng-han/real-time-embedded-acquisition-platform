#include "drats/config_store.h"
#include "drats/crc32c.h"

#include <stdbool.h>
#include <string.h>

enum {
    OFFSET_MAGIC = 0,
    OFFSET_VERSION = 4,
    OFFSET_LENGTH = 6,
    OFFSET_GENERATION = 8,
    OFFSET_SAMPLE_RATE = 12,
    OFFSET_SAMPLES_PER_BLOCK = 16,
    OFFSET_OVERFLOW_POLICY = 18,
    OFFSET_TELEMETRY_ENABLED = 19,
    OFFSET_CRC = 24,
    OFFSET_COMMIT = 28
};

static void put_u16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *p, uint32_t value) {
    for (unsigned i = 0; i < 4U; ++i) {
        p[i] = (uint8_t)(value >> (8U * i));
    }
}

static uint16_t get_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8U));
}

static uint32_t get_u32(const uint8_t *p) {
    uint32_t value = 0U;
    for (unsigned i = 0; i < 4U; ++i) {
        value |= (uint32_t)p[i] << (8U * i);
    }
    return value;
}

static bool generation_is_newer(uint32_t candidate, uint32_t reference) {
    return (int32_t)(candidate - reference) > 0;
}

static bool config_valid(const drats_persistent_config_t *config) {
    return (config->sample_rate_hz >= 1U) &&
           (config->sample_rate_hz <= 50000U) &&
           (config->samples_per_block >= 1U) &&
           (config->samples_per_block <= 32U) &&
           (config->overflow_policy <= 1U) &&
           (config->telemetry_enabled <= 1U);
}

drats_status_t drats_config_record_encode(
    const drats_persistent_config_t *config,
    uint32_t generation,
    uint8_t output[DRATS_CONFIG_RECORD_SIZE]) {
    if ((config == NULL) || (output == NULL) || !config_valid(config)) {
        return DRATS_E_ARGUMENT;
    }
    memset(output, 0xff, DRATS_CONFIG_RECORD_SIZE);
    put_u32(&output[OFFSET_MAGIC], DRATS_CONFIG_MAGIC);
    put_u16(&output[OFFSET_VERSION], DRATS_CONFIG_FORMAT_VERSION);
    put_u16(&output[OFFSET_LENGTH], DRATS_CONFIG_RECORD_SIZE);
    put_u32(&output[OFFSET_GENERATION], generation);
    put_u32(&output[OFFSET_SAMPLE_RATE], config->sample_rate_hz);
    put_u16(&output[OFFSET_SAMPLES_PER_BLOCK], config->samples_per_block);
    output[OFFSET_OVERFLOW_POLICY] = config->overflow_policy;
    output[OFFSET_TELEMETRY_ENABLED] = config->telemetry_enabled;
    put_u32(&output[OFFSET_CRC],
            drats_crc32c(0U, output, OFFSET_CRC));
    return DRATS_OK;
}

drats_status_t drats_config_record_commit(
    uint8_t record[DRATS_CONFIG_RECORD_SIZE]) {
    if (record == NULL) {
        return DRATS_E_ARGUMENT;
    }
    if ((get_u32(&record[OFFSET_MAGIC]) != DRATS_CONFIG_MAGIC) ||
        (get_u32(&record[OFFSET_CRC]) !=
         drats_crc32c(0U, record, OFFSET_CRC))) {
        return DRATS_E_CRC;
    }
    put_u32(&record[OFFSET_COMMIT], DRATS_CONFIG_COMMITTED);
    return DRATS_OK;
}

drats_status_t drats_config_record_decode(
    const uint8_t *record,
    size_t record_length,
    drats_config_selection_t *selection) {
    if ((record == NULL) || (selection == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    if (record_length != DRATS_CONFIG_RECORD_SIZE) {
        return DRATS_E_SIZE;
    }
    if ((get_u32(&record[OFFSET_MAGIC]) != DRATS_CONFIG_MAGIC) ||
        (get_u16(&record[OFFSET_VERSION]) != DRATS_CONFIG_FORMAT_VERSION) ||
        (get_u16(&record[OFFSET_LENGTH]) != DRATS_CONFIG_RECORD_SIZE) ||
        (get_u32(&record[OFFSET_COMMIT]) != DRATS_CONFIG_COMMITTED)) {
        return DRATS_E_FORMAT;
    }
    if (get_u32(&record[OFFSET_CRC]) !=
        drats_crc32c(0U, record, OFFSET_CRC)) {
        return DRATS_E_CRC;
    }
    drats_persistent_config_t decoded = {
        .sample_rate_hz = get_u32(&record[OFFSET_SAMPLE_RATE]),
        .samples_per_block = get_u16(&record[OFFSET_SAMPLES_PER_BLOCK]),
        .overflow_policy = record[OFFSET_OVERFLOW_POLICY],
        .telemetry_enabled = record[OFFSET_TELEMETRY_ENABLED]
    };
    if (!config_valid(&decoded)) {
        return DRATS_E_FORMAT;
    }
    selection->generation = get_u32(&record[OFFSET_GENERATION]);
    selection->config = decoded;
    return DRATS_OK;
}

drats_status_t drats_config_select_newest(
    const uint8_t *slot_a,
    size_t slot_a_length,
    const uint8_t *slot_b,
    size_t slot_b_length,
    drats_config_selection_t *selection,
    unsigned *selected_slot) {
    if ((selection == NULL) || (selected_slot == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    drats_config_selection_t a;
    drats_config_selection_t b;
    const bool a_valid = drats_config_record_decode(slot_a, slot_a_length, &a) == DRATS_OK;
    const bool b_valid = drats_config_record_decode(slot_b, slot_b_length, &b) == DRATS_OK;
    if (!a_valid && !b_valid) {
        return DRATS_E_FORMAT;
    }
    if (a_valid && (!b_valid || !generation_is_newer(b.generation, a.generation))) {
        *selection = a;
        *selected_slot = 0U;
    } else {
        *selection = b;
        *selected_slot = 1U;
    }
    return DRATS_OK;
}
