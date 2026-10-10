#include "drats/config_protocol.h"

static void put_u16(uint8_t *destination, uint16_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *destination, uint32_t value) {
    for (unsigned i = 0; i < 4U; ++i) {
        destination[i] = (uint8_t)(value >> (8U * i));
    }
}

static uint16_t get_u16(const uint8_t *source) {
    return (uint16_t)((uint16_t)source[0] | ((uint16_t)source[1] << 8U));
}

static uint32_t get_u32(const uint8_t *source) {
    uint32_t value = 0U;
    for (unsigned i = 0; i < 4U; ++i) {
        value |= (uint32_t)source[i] << (8U * i);
    }
    return value;
}

drats_status_t drats_config_command_decode(
    const uint8_t *payload,
    size_t length,
    drats_config_command_t *command) {
    if ((payload == NULL) || (command == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    if (length != DRATS_CONFIG_COMMAND_SIZE) {
        return DRATS_E_SIZE;
    }
    drats_config_command_t decoded = {
        .request_id = get_u32(&payload[0]),
        .config = {
            .sample_rate_hz = get_u32(&payload[4]),
            .samples_per_block = get_u16(&payload[8]),
            .overflow_policy = (drats_overflow_policy_t)payload[10]
        },
        .telemetry_enabled = payload[11] != 0U
    };
    if ((payload[11] > 1U) ||
        (drats_acq_config_validate(&decoded.config) != DRATS_OK)) {
        return DRATS_E_ARGUMENT;
    }
    *command = decoded;
    return DRATS_OK;
}

drats_status_t drats_config_ack_encode(
    const drats_config_ack_t *ack,
    uint8_t output[DRATS_CONFIG_ACK_SIZE]) {
    if ((ack == NULL) || (output == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    put_u32(&output[0], ack->request_id);
    put_u32(&output[4], (uint32_t)ack->status);
    put_u32(&output[8], ack->config.sample_rate_hz);
    put_u16(&output[12], ack->config.samples_per_block);
    output[14] = (uint8_t)ack->config.overflow_policy;
    output[15] = ack->telemetry_enabled ? 1U : 0U;
    return DRATS_OK;
}

drats_status_t drats_config_ack_decode(
    const uint8_t *payload,
    size_t length,
    drats_config_ack_t *ack) {
    if ((payload == NULL) || (ack == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    if (length != DRATS_CONFIG_ACK_SIZE) {
        return DRATS_E_SIZE;
    }
    ack->request_id = get_u32(&payload[0]);
    ack->status = (int32_t)get_u32(&payload[4]);
    ack->config.sample_rate_hz = get_u32(&payload[8]);
    ack->config.samples_per_block = get_u16(&payload[12]);
    ack->config.overflow_policy = (drats_overflow_policy_t)payload[14];
    ack->telemetry_enabled = payload[15] != 0U;
    if ((payload[15] > 1U) ||
        ((ack->status == DRATS_OK) &&
         (drats_acq_config_validate(&ack->config) != DRATS_OK))) {
        return DRATS_E_FORMAT;
    }
    return DRATS_OK;
}
