#include "drats/acquisition.h"
#include "drats/protocol.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_PAYLOAD_PREFIX 12U
#define SAMPLE_BYTES (DRATS_SAMPLES_PER_BLOCK * DRATS_CHANNEL_COUNT * 2U)

static void put_u16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *p, uint32_t value) {
    for (unsigned i = 0; i < 4U; ++i) {
        p[i] = (uint8_t)(value >> (8U * i));
    }
}

static int emit_block(uint32_t frame_sequence,
                      drats_sample_block_t *block,
                      FILE *output) {
    uint8_t payload[SAMPLE_PAYLOAD_PREFIX + SAMPLE_BYTES];
    put_u32(&payload[0], block->sequence_first);
    put_u32(&payload[4], block->sample_period_ns);
    put_u16(&payload[8], block->sample_count);
    put_u16(&payload[10], DRATS_CHANNEL_COUNT);
    size_t offset = SAMPLE_PAYLOAD_PREFIX;
    for (size_t sample = 0; sample < block->sample_count; ++sample) {
        for (size_t channel = 0; channel < DRATS_CHANNEL_COUNT; ++channel) {
            put_u16(&payload[offset], (uint16_t)block->samples[sample][channel]);
            offset += 2U;
        }
    }
    const drats_frame_header_t header = {
        .version = DRATS_PROTOCOL_VERSION,
        .type = DRATS_MSG_SAMPLE_BLOCK,
        .flags = block->flags,
        .sequence = frame_sequence,
        .timestamp_ns = block->timestamp_first_ns,
        .payload_length = (uint16_t)offset
    };
    uint8_t frame[DRATS_PROTOCOL_MAX_FRAME];
    size_t frame_length = 0U;
    if (drats_frame_encode(&header, payload, frame, sizeof(frame),
                           &frame_length) != DRATS_OK) {
        return 1;
    }
    return fwrite(frame, 1U, frame_length, output) == frame_length ? 0 : 1;
}

int main(int argc, char **argv) {
    unsigned long requested_blocks = 100U;
    if (argc > 2) {
        fprintf(stderr, "usage: %s [block-count]\n", argv[0]);
        return 2;
    }
    if (argc == 2) {
        char *end = NULL;
        errno = 0;
        requested_blocks = strtoul(argv[1], &end, 10);
        if ((errno != 0) || (end == argv[1]) || (*end != '\0') ||
            (requested_blocks == 0U) || (requested_blocks > UINT32_MAX)) {
            fprintf(stderr, "invalid block count\n");
            return 2;
        }
    }

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
    if ((drats_acq_init(&acq, &pool, &ready, &config) != DRATS_OK) ||
        (drats_acq_start(&acq) != DRATS_OK)) {
        return 1;
    }

    for (uint32_t iteration = 0; iteration < (uint32_t)requested_blocks;
         ++iteration) {
        drats_sample_block_t *block = NULL;
        if (drats_acq_arm_next(&acq, &block) != DRATS_OK) {
            return 1;
        }
        for (size_t sample = 0; sample < DRATS_SAMPLES_PER_BLOCK; ++sample) {
            for (size_t channel = 0; channel < DRATS_CHANNEL_COUNT; ++channel) {
                const uint32_t value = (iteration * DRATS_SAMPLES_PER_BLOCK) +
                                       (uint32_t)sample + (uint32_t)channel;
                block->samples[sample][channel] = (int16_t)(value & 0x7fffU);
            }
        }
        if (drats_acq_complete_dma(&acq,
                                   (uint64_t)iteration * UINT64_C(3200000),
                                   DRATS_SAMPLES_PER_BLOCK) != DRATS_OK) {
            return 1;
        }
        drats_block_handle_t handle;
        if ((drats_handle_queue_pop(&ready, &handle) != DRATS_OK) ||
            (drats_block_transition(&pool, handle, DRATS_BLOCK_READY,
                                    DRATS_BLOCK_PROCESSING) != DRATS_OK)) {
            return 1;
        }
        drats_sample_block_t *processing = NULL;
        if ((drats_block_get(&pool, handle, DRATS_BLOCK_PROCESSING,
                             &processing) != DRATS_OK) ||
            (drats_block_transition(&pool, handle, DRATS_BLOCK_PROCESSING,
                                    DRATS_BLOCK_TELEMETRY) != DRATS_OK) ||
            (emit_block(iteration, processing, stdout) != 0) ||
            (drats_block_transition(&pool, handle, DRATS_BLOCK_TELEMETRY,
                                    DRATS_BLOCK_FREE) != DRATS_OK)) {
            return 1;
        }
    }
    return fflush(stdout) == 0 ? 0 : 1;
}
