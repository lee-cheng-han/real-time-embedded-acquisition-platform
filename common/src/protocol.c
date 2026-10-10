#include "drats/protocol.h"
#include "drats/crc32c.h"

#include <string.h>

static const uint8_t magic[4] = {'D', 'R', 'A', 'T'};

static void put_u16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *p, uint32_t value) {
    for (unsigned i = 0; i < 4U; ++i) {
        p[i] = (uint8_t)(value >> (8U * i));
    }
}

static void put_u64(uint8_t *p, uint64_t value) {
    for (unsigned i = 0; i < 8U; ++i) {
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

static uint64_t get_u64(const uint8_t *p) {
    uint64_t value = 0U;
    for (unsigned i = 0; i < 8U; ++i) {
        value |= (uint64_t)p[i] << (8U * i);
    }
    return value;
}

drats_status_t drats_frame_encode(const drats_frame_header_t *header,
                                  const uint8_t *payload,
                                  uint8_t *output,
                                  size_t capacity,
                                  size_t *encoded_length) {
    if ((header == NULL) || (output == NULL) || (encoded_length == NULL) ||
        ((header->payload_length != 0U) && (payload == NULL))) {
        return DRATS_E_ARGUMENT;
    }
    if (header->payload_length > DRATS_PROTOCOL_MAX_PAYLOAD) {
        return DRATS_E_SIZE;
    }
    const size_t total = DRATS_PROTOCOL_HEADER_SIZE + header->payload_length +
                         DRATS_PROTOCOL_CRC_SIZE;
    if (capacity < total) {
        return DRATS_E_SIZE;
    }
    memcpy(output, magic, sizeof(magic));
    output[4] = header->version;
    output[5] = header->type;
    put_u16(&output[6], header->flags);
    put_u32(&output[8], header->sequence);
    put_u64(&output[12], header->timestamp_ns);
    put_u16(&output[20], header->payload_length);
    put_u16(&output[22], 0U);
    if (header->payload_length != 0U) {
        memcpy(&output[DRATS_PROTOCOL_HEADER_SIZE], payload,
               header->payload_length);
    }
    put_u32(&output[total - DRATS_PROTOCOL_CRC_SIZE],
            drats_crc32c(0U, output, total - DRATS_PROTOCOL_CRC_SIZE));
    *encoded_length = total;
    return DRATS_OK;
}

drats_status_t drats_frame_decode(const uint8_t *frame,
                                  size_t frame_length,
                                  drats_frame_header_t *header,
                                  const uint8_t **payload) {
    if ((frame == NULL) || (header == NULL) || (payload == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    if (frame_length < DRATS_PROTOCOL_HEADER_SIZE + DRATS_PROTOCOL_CRC_SIZE) {
        return DRATS_E_SIZE;
    }
    if (memcmp(frame, magic, sizeof(magic)) != 0) {
        return DRATS_E_FORMAT;
    }
    if (frame[4] != DRATS_PROTOCOL_VERSION) {
        return DRATS_E_VERSION;
    }
    if (get_u16(&frame[22]) != 0U) {
        return DRATS_E_FORMAT;
    }
    const uint16_t payload_length = get_u16(&frame[20]);
    const size_t expected = DRATS_PROTOCOL_HEADER_SIZE + payload_length +
                            DRATS_PROTOCOL_CRC_SIZE;
    if ((payload_length > DRATS_PROTOCOL_MAX_PAYLOAD) ||
        (frame_length != expected)) {
        return DRATS_E_SIZE;
    }
    const uint32_t expected_crc = get_u32(&frame[expected - DRATS_PROTOCOL_CRC_SIZE]);
    const uint32_t actual_crc = drats_crc32c(0U, frame,
                                             expected - DRATS_PROTOCOL_CRC_SIZE);
    if (expected_crc != actual_crc) {
        return DRATS_E_CRC;
    }
    header->version = frame[4];
    header->type = frame[5];
    header->flags = get_u16(&frame[6]);
    header->sequence = get_u32(&frame[8]);
    header->timestamp_ns = get_u64(&frame[12]);
    header->payload_length = payload_length;
    *payload = &frame[DRATS_PROTOCOL_HEADER_SIZE];
    return DRATS_OK;
}
