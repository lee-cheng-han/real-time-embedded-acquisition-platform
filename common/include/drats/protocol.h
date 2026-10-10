#ifndef DRATS_PROTOCOL_H
#define DRATS_PROTOCOL_H

#include "drats/status.h"

#include <stddef.h>
#include <stdint.h>

#define DRATS_PROTOCOL_VERSION 1U
#define DRATS_PROTOCOL_HEADER_SIZE 24U
#define DRATS_PROTOCOL_CRC_SIZE 4U
#define DRATS_PROTOCOL_MAX_PAYLOAD 1024U
#define DRATS_PROTOCOL_MAX_FRAME (DRATS_PROTOCOL_HEADER_SIZE + DRATS_PROTOCOL_MAX_PAYLOAD + DRATS_PROTOCOL_CRC_SIZE)

typedef enum {
    DRATS_MSG_SAMPLE_BLOCK = 1,
    DRATS_MSG_HEALTH = 2,
    DRATS_MSG_CONFIG_SET = 16,
    DRATS_MSG_CONFIG_ACK = 17,
    DRATS_MSG_ERROR = 127
} drats_message_type_t;

typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t flags;
    uint32_t sequence;
    uint64_t timestamp_ns;
    uint16_t payload_length;
} drats_frame_header_t;

drats_status_t drats_frame_encode(const drats_frame_header_t *header,
                                  const uint8_t *payload,
                                  uint8_t *output,
                                  size_t capacity,
                                  size_t *encoded_length);
drats_status_t drats_frame_decode(const uint8_t *frame,
                                  size_t frame_length,
                                  drats_frame_header_t *header,
                                  const uint8_t **payload);

#endif
