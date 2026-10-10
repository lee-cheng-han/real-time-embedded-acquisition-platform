#ifndef DRATS_CONFIG_PROTOCOL_H
#define DRATS_CONFIG_PROTOCOL_H

#include "drats/acquisition.h"
#include "drats/status.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DRATS_CONFIG_COMMAND_SIZE 12U
#define DRATS_CONFIG_ACK_SIZE 16U

typedef struct {
    uint32_t request_id;
    drats_acq_config_t config;
    bool telemetry_enabled;
} drats_config_command_t;

typedef struct {
    uint32_t request_id;
    int32_t status;
    drats_acq_config_t config;
    bool telemetry_enabled;
} drats_config_ack_t;

drats_status_t drats_config_command_decode(
    const uint8_t *payload,
    size_t length,
    drats_config_command_t *command);
drats_status_t drats_config_ack_encode(
    const drats_config_ack_t *ack,
    uint8_t output[DRATS_CONFIG_ACK_SIZE]);
drats_status_t drats_config_ack_decode(
    const uint8_t *payload,
    size_t length,
    drats_config_ack_t *ack);

#endif
