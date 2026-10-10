#include "drats/icm42688.h"
#include "drats/protocol.h"
#include "drats/stream_decoder.h"
#include "test_support.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t registers[128];
    uint32_t delay_total_us;
    bool fail_next;
} fake_sensor_t;

static drats_status_t fake_transfer(void *context,
                                    const uint8_t *tx,
                                    uint8_t *rx,
                                    size_t length,
                                    uint32_t timeout_us) {
    fake_sensor_t *fake = context;
    if ((timeout_us == 0U) || (length < 2U)) {
        return DRATS_E_ARGUMENT;
    }
    if (fake->fail_next) {
        fake->fail_next = false;
        return DRATS_E_TIMEOUT;
    }
    memset(rx, 0, length);
    const uint8_t address = tx[0] & 0x7fU;
    if ((tx[0] & 0x80U) != 0U) {
        for (size_t i = 1U; i < length; ++i) {
            rx[i] = fake->registers[(uint8_t)(address + i - 1U)];
        }
    } else {
        fake->registers[address] = tx[1];
    }
    return DRATS_OK;
}

static void fake_delay(void *context, uint32_t delay_us) {
    fake_sensor_t *fake = context;
    fake->delay_total_us += delay_us;
}

typedef struct {
    uint32_t count;
    uint32_t last_sequence;
    bool payload_valid;
} decoded_frames_t;

static void on_frame(const drats_frame_header_t *header,
                     const uint8_t *payload,
                     void *context) {
    decoded_frames_t *decoded = context;
    ++decoded->count;
    decoded->last_sequence = header->sequence;
    decoded->payload_valid = (header->payload_length == 0U) || (payload != NULL);
}

static int test_stream(void) {
    const uint8_t payload[] = {1U, 2U, 3U};
    drats_frame_header_t header = {
        .version = DRATS_PROTOCOL_VERSION,
        .type = DRATS_MSG_HEALTH,
        .sequence = 91U,
        .payload_length = sizeof(payload)
    };
    uint8_t frame[64];
    size_t frame_length = 0U;
    CHECK(drats_frame_encode(&header, payload, frame, sizeof(frame),
                             &frame_length) == DRATS_OK);
    drats_stream_decoder_t decoder;
    drats_stream_decoder_init(&decoder);
    decoded_frames_t decoded = {0};
    const uint8_t garbage[] = {0xffU, 'D', 'R', 0U};
    drats_stream_decoder_feed(&decoder, garbage, sizeof(garbage),
                              on_frame, &decoded);
    for (size_t i = 0; i < frame_length; ++i) {
        drats_stream_decoder_feed(&decoder, &frame[i], 1U,
                                  on_frame, &decoded);
    }
    CHECK(decoded.count == 1U);
    CHECK(decoded.last_sequence == 91U);
    CHECK(decoded.payload_valid);
    CHECK(decoder.bytes_discarded >= sizeof(garbage));
    frame[10] ^= 1U;
    drats_stream_decoder_feed(&decoder, frame, frame_length,
                              on_frame, &decoded);
    CHECK(decoded.count == 1U);
    CHECK(decoder.corrupt_candidates >= 1U);
    return 0;
}

static int test_sensor(void) {
    fake_sensor_t fake = {0};
    fake.registers[0x75] = DRATS_ICM42688_WHO_AM_I_VALUE;
    for (size_t i = 0; i < 12U; ++i) {
        fake.registers[0x1fU + i] = (uint8_t)(i + 1U);
    }
    const drats_icm42688_bus_t bus = {
        .transfer = fake_transfer,
        .delay_us = fake_delay,
        .context = &fake,
        .timeout_us = 1000U
    };
    drats_icm42688_t sensor;
    CHECK(drats_icm42688_init(&sensor, &bus) == DRATS_OK);
    CHECK(sensor.initialized);
    CHECK(fake.delay_total_us == 52000U);
    CHECK(fake.registers[0x4e] == 0x0fU);
    CHECK(fake.registers[0x4f] == 0x06U);
    int16_t axes[6];
    CHECK(drats_icm42688_read_axes(&sensor, axes) == DRATS_OK);
    CHECK(axes[0] == (int16_t)0x0102);
    CHECK(axes[5] == (int16_t)0x0b0c);
    fake.fail_next = true;
    CHECK(drats_icm42688_read_axes(&sensor, axes) == DRATS_E_TIMEOUT);
    CHECK(sensor.errors == 1U);
    drats_icm42688_shutdown(&sensor);
    CHECK(!sensor.initialized);

    fake.registers[0x75] = 0U;
    CHECK(drats_icm42688_init(&sensor, &bus) == DRATS_E_IO);
    return 0;
}

int main(void) {
    CHECK(test_stream() == 0);
    CHECK(test_sensor() == 0);
    return 0;
}
