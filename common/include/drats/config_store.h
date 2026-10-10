#ifndef DRATS_CONFIG_STORE_H
#define DRATS_CONFIG_STORE_H

#include "drats/status.h"

#include <stddef.h>
#include <stdint.h>

#define DRATS_CONFIG_MAGIC UINT32_C(0x43464744)
#define DRATS_CONFIG_FORMAT_VERSION 1U
#define DRATS_CONFIG_COMMITTED UINT32_C(0xA55A3CC3)
#define DRATS_CONFIG_RECORD_SIZE 32U

typedef struct {
    uint32_t sample_rate_hz;
    uint16_t samples_per_block;
    uint8_t overflow_policy;
    uint8_t telemetry_enabled;
} drats_persistent_config_t;

typedef struct {
    uint32_t generation;
    drats_persistent_config_t config;
} drats_config_selection_t;

drats_status_t drats_config_record_encode(
    const drats_persistent_config_t *config,
    uint32_t generation,
    uint8_t output[DRATS_CONFIG_RECORD_SIZE]);
drats_status_t drats_config_record_commit(
    uint8_t record[DRATS_CONFIG_RECORD_SIZE]);
drats_status_t drats_config_record_decode(
    const uint8_t *record,
    size_t record_length,
    drats_config_selection_t *selection);
drats_status_t drats_config_select_newest(
    const uint8_t *slot_a,
    size_t slot_a_length,
    const uint8_t *slot_b,
    size_t slot_b_length,
    drats_config_selection_t *selection,
    unsigned *selected_slot);

#endif
