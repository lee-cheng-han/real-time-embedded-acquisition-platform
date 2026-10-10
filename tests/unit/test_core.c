#include "drats/acquisition.h"
#include "drats/block_pool.h"
#include "drats/crc32c.h"
#include "drats/handle_queue.h"
#include "drats/protocol.h"
#include "test_support.h"

#include <stdint.h>
#include <string.h>

static int test_crc(void) {
    static const char vector[] = "123456789";
    CHECK(drats_crc32c(0U, vector, 9U) == UINT32_C(0xE3069283));
    return 0;
}

static int test_protocol(void) {
    const uint8_t body[] = {0x00, 0x7f, 0x80, 0xff};
    const drats_frame_header_t input = {
        .version = DRATS_PROTOCOL_VERSION,
        .type = DRATS_MSG_SAMPLE_BLOCK,
        .flags = 0x102U,
        .sequence = UINT32_C(0x12345678),
        .timestamp_ns = UINT64_C(0x0102030405060708),
        .payload_length = sizeof(body)
    };
    uint8_t frame[DRATS_PROTOCOL_MAX_FRAME];
    size_t length = 0U;
    CHECK(drats_frame_encode(&input, body, frame, sizeof(frame), &length) == DRATS_OK);
    CHECK(length == DRATS_PROTOCOL_HEADER_SIZE + sizeof(body) + DRATS_PROTOCOL_CRC_SIZE);
    drats_frame_header_t output = {0};
    const uint8_t *decoded = NULL;
    CHECK(drats_frame_decode(frame, length, &output, &decoded) == DRATS_OK);
    CHECK(output.sequence == input.sequence);
    CHECK(output.timestamp_ns == input.timestamp_ns);
    CHECK(output.payload_length == sizeof(body));
    CHECK(memcmp(decoded, body, sizeof(body)) == 0);
    frame[22] = 1U;
    CHECK(drats_frame_decode(frame, length, &output, &decoded) == DRATS_E_FORMAT);
    frame[22] = 0U;
    frame[DRATS_PROTOCOL_HEADER_SIZE + 1U] ^= 1U;
    CHECK(drats_frame_decode(frame, length, &output, &decoded) == DRATS_E_CRC);
    return 0;
}

static int test_pool_and_queue(void) {
    drats_block_pool_t pool;
    drats_block_pool_init(&pool);
    drats_block_handle_t handles[DRATS_POOL_CAPACITY];
    for (size_t i = 0; i < DRATS_POOL_CAPACITY; ++i) {
        drats_sample_block_t *block = NULL;
        CHECK(drats_block_acquire_dma(&pool, &handles[i], &block) == DRATS_OK);
        CHECK(block != NULL);
    }
    drats_block_handle_t extra;
    drats_sample_block_t *unused = NULL;
    CHECK(drats_block_acquire_dma(&pool, &extra, &unused) == DRATS_E_FULL);
    CHECK(drats_block_transition(&pool, handles[0], DRATS_BLOCK_DMA,
                                 DRATS_BLOCK_READY) == DRATS_OK);
    CHECK(drats_block_transition(&pool, handles[0], DRATS_BLOCK_DMA,
                                 DRATS_BLOCK_READY) == DRATS_E_STATE);
    CHECK(drats_block_transition(&pool, handles[0], DRATS_BLOCK_READY,
                                 DRATS_BLOCK_FREE) == DRATS_OK);
    drats_block_handle_t replacement;
    CHECK(drats_block_acquire_dma(&pool, &replacement, &unused) == DRATS_OK);
    CHECK(drats_block_transition(&pool, handles[0], DRATS_BLOCK_DMA,
                                 DRATS_BLOCK_READY) == DRATS_E_STALE);

    drats_handle_queue_t queue;
    drats_handle_queue_init(&queue);
    for (size_t i = 0; i < DRATS_QUEUE_CAPACITY - 1U; ++i) {
        CHECK(drats_handle_queue_push(&queue, handles[i]) == DRATS_OK);
    }
    CHECK(drats_handle_queue_push(&queue, extra) == DRATS_E_FULL);
    CHECK(drats_handle_queue_size(&queue) == DRATS_QUEUE_CAPACITY - 1U);
    for (size_t i = 0; i < DRATS_QUEUE_CAPACITY - 1U; ++i) {
        drats_block_handle_t value;
        CHECK(drats_handle_queue_pop(&queue, &value) == DRATS_OK);
        CHECK(value.index == handles[i].index);
    }
    CHECK(drats_handle_queue_pop(&queue, &extra) == DRATS_E_EMPTY);
    return 0;
}

static int test_configuration(void) {
    drats_acq_config_t config = {
        .sample_rate_hz = 10000U,
        .samples_per_block = DRATS_SAMPLES_PER_BLOCK,
        .overflow_policy = DRATS_OVERFLOW_STOP
    };
    CHECK(drats_acq_config_validate(&config) == DRATS_OK);
    config.sample_rate_hz = 0U;
    CHECK(drats_acq_config_validate(&config) == DRATS_E_ARGUMENT);
    config.sample_rate_hz = 50001U;
    CHECK(drats_acq_config_validate(&config) == DRATS_E_ARGUMENT);
    return 0;
}

int main(void) {
    CHECK(test_crc() == 0);
    CHECK(test_protocol() == 0);
    CHECK(test_pool_and_queue() == 0);
    CHECK(test_configuration() == 0);
    return 0;
}
