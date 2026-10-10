#ifndef DRATS_BLOCK_POOL_H
#define DRATS_BLOCK_POOL_H

#include "drats/status.h"

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#define DRATS_POOL_CAPACITY 8U
#define DRATS_SAMPLES_PER_BLOCK 32U
#define DRATS_CHANNEL_COUNT 6U

typedef enum {
    DRATS_BLOCK_FREE = 0,
    DRATS_BLOCK_DMA,
    DRATS_BLOCK_READY,
    DRATS_BLOCK_PROCESSING,
    DRATS_BLOCK_TELEMETRY
} drats_block_owner_t;

typedef struct {
    uint8_t index;
    uint32_t generation;
} drats_block_handle_t;

typedef struct {
    atomic_uchar owner;
    atomic_uint_least32_t generation;
    uint32_t sequence_first;
    uint64_t timestamp_first_ns;
    uint32_t sample_period_ns;
    uint16_t sample_count;
    uint16_t flags;
    int16_t samples[DRATS_SAMPLES_PER_BLOCK][DRATS_CHANNEL_COUNT];
} drats_sample_block_t;

typedef struct {
    drats_sample_block_t blocks[DRATS_POOL_CAPACITY];
} drats_block_pool_t;

void drats_block_pool_init(drats_block_pool_t *pool);
drats_status_t drats_block_acquire_dma(drats_block_pool_t *pool,
                                       drats_block_handle_t *handle,
                                       drats_sample_block_t **block);
drats_status_t drats_block_transition(drats_block_pool_t *pool,
                                      drats_block_handle_t handle,
                                      drats_block_owner_t expected,
                                      drats_block_owner_t desired);
drats_status_t drats_block_get(drats_block_pool_t *pool,
                               drats_block_handle_t handle,
                               drats_block_owner_t expected,
                               drats_sample_block_t **block);
size_t drats_block_count_owner(const drats_block_pool_t *pool,
                               drats_block_owner_t owner);

#endif
