#include "drats/acquisition.h"
#include "drats/protocol.h"
#include "test_support.h"

#include <stdint.h>
#include <string.h>

static void put_u32(uint8_t *p, uint32_t value) {
    for (unsigned i = 0; i < 4U; ++i) {
        p[i] = (uint8_t)(value >> (8U * i));
    }
}

int main(void) {
    drats_block_pool_t pool;
    drats_handle_queue_t ready;
    drats_block_pool_init(&pool);
    drats_handle_queue_init(&ready);
    const drats_acq_config_t config = {
        .sample_rate_hz = 10000U,
        .samples_per_block = DRATS_SAMPLES_PER_BLOCK,
        .overflow_policy = DRATS_OVERFLOW_STOP
    };
    drats_acquisition_t acq;
    CHECK(drats_acq_init(&acq, &pool, &ready, &config) == DRATS_OK);
    CHECK(drats_acq_start(&acq) == DRATS_OK);

    uint32_t expected_sequence = 0U;
    for (uint32_t iteration = 0; iteration < 3125U; ++iteration) {
        drats_sample_block_t *dma_block = NULL;
        CHECK(drats_acq_arm_next(&acq, &dma_block) == DRATS_OK);
        for (size_t sample = 0; sample < DRATS_SAMPLES_PER_BLOCK; ++sample) {
            for (size_t channel = 0; channel < DRATS_CHANNEL_COUNT; ++channel) {
                dma_block->samples[sample][channel] =
                    (int16_t)((iteration + sample + channel) & 0x7fffU);
            }
        }
        CHECK(drats_acq_complete_dma(&acq,
                                     (uint64_t)iteration * UINT64_C(3200000),
                                     DRATS_SAMPLES_PER_BLOCK) == DRATS_OK);
        drats_block_handle_t handle;
        CHECK(drats_handle_queue_pop(&ready, &handle) == DRATS_OK);
        CHECK(drats_block_transition(&pool, handle, DRATS_BLOCK_READY,
                                     DRATS_BLOCK_PROCESSING) == DRATS_OK);
        drats_sample_block_t *processing = NULL;
        CHECK(drats_block_get(&pool, handle, DRATS_BLOCK_PROCESSING,
                              &processing) == DRATS_OK);
        CHECK(processing->sequence_first == expected_sequence);
        expected_sequence += DRATS_SAMPLES_PER_BLOCK;
        CHECK(drats_block_transition(&pool, handle, DRATS_BLOCK_PROCESSING,
                                     DRATS_BLOCK_TELEMETRY) == DRATS_OK);

        uint8_t payload[8];
        put_u32(payload, processing->sequence_first);
        put_u32(&payload[4], processing->sample_period_ns);
        drats_frame_header_t header = {
            .version = DRATS_PROTOCOL_VERSION,
            .type = DRATS_MSG_SAMPLE_BLOCK,
            .sequence = iteration,
            .timestamp_ns = processing->timestamp_first_ns,
            .payload_length = sizeof(payload)
        };
        uint8_t frame[64];
        size_t frame_length = 0U;
        CHECK(drats_frame_encode(&header, payload, frame, sizeof(frame),
                                 &frame_length) == DRATS_OK);
        drats_frame_header_t decoded;
        const uint8_t *decoded_payload = NULL;
        CHECK(drats_frame_decode(frame, frame_length, &decoded,
                                 &decoded_payload) == DRATS_OK);
        CHECK(decoded.sequence == iteration);
        CHECK(memcmp(payload, decoded_payload, sizeof(payload)) == 0);
        CHECK(drats_block_transition(&pool, handle, DRATS_BLOCK_TELEMETRY,
                                     DRATS_BLOCK_FREE) == DRATS_OK);
    }
    CHECK(acq.counters.samples_completed == UINT64_C(100000));
    CHECK(acq.counters.dropped_blocks == 0U);
    CHECK(drats_block_count_owner(&pool, DRATS_BLOCK_FREE) == DRATS_POOL_CAPACITY);
    return 0;
}
