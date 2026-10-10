#include "drats/block_pool.h"

#include <string.h>

void drats_block_pool_init(drats_block_pool_t *pool) {
    if (pool == NULL) {
        return;
    }
    memset(pool, 0, sizeof(*pool));
    for (size_t i = 0; i < DRATS_POOL_CAPACITY; ++i) {
        atomic_init(&pool->blocks[i].owner, (unsigned char)DRATS_BLOCK_FREE);
        atomic_init(&pool->blocks[i].generation, 0U);
    }
}

drats_status_t drats_block_acquire_dma(drats_block_pool_t *pool,
                                       drats_block_handle_t *handle,
                                       drats_sample_block_t **block) {
    if ((pool == NULL) || (handle == NULL) || (block == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    for (size_t i = 0; i < DRATS_POOL_CAPACITY; ++i) {
        unsigned char expected = (unsigned char)DRATS_BLOCK_FREE;
        if (atomic_compare_exchange_strong_explicit(
                &pool->blocks[i].owner, &expected,
                (unsigned char)DRATS_BLOCK_DMA,
                memory_order_acq_rel, memory_order_relaxed)) {
            uint32_t generation = atomic_fetch_add_explicit(
                &pool->blocks[i].generation, 1U, memory_order_relaxed) + 1U;
            handle->index = (uint8_t)i;
            handle->generation = generation;
            *block = &pool->blocks[i];
            return DRATS_OK;
        }
    }
    return DRATS_E_FULL;
}

drats_status_t drats_block_get(drats_block_pool_t *pool,
                               drats_block_handle_t handle,
                               drats_block_owner_t expected,
                               drats_sample_block_t **block) {
    if ((pool == NULL) || (block == NULL) ||
        (handle.index >= DRATS_POOL_CAPACITY)) {
        return DRATS_E_ARGUMENT;
    }
    drats_sample_block_t *candidate = &pool->blocks[handle.index];
    if (atomic_load_explicit(&candidate->generation, memory_order_acquire) !=
        handle.generation) {
        return DRATS_E_STALE;
    }
    if (atomic_load_explicit(&candidate->owner, memory_order_acquire) !=
        (unsigned char)expected) {
        return DRATS_E_STATE;
    }
    *block = candidate;
    return DRATS_OK;
}

drats_status_t drats_block_transition(drats_block_pool_t *pool,
                                      drats_block_handle_t handle,
                                      drats_block_owner_t expected,
                                      drats_block_owner_t desired) {
    drats_sample_block_t *block = NULL;
    drats_status_t status = drats_block_get(pool, handle, expected, &block);
    if (status != DRATS_OK) {
        return status;
    }
    unsigned char expected_value = (unsigned char)expected;
    if (!atomic_compare_exchange_strong_explicit(
            &block->owner, &expected_value, (unsigned char)desired,
            memory_order_acq_rel, memory_order_acquire)) {
        return DRATS_E_STATE;
    }
    return DRATS_OK;
}

size_t drats_block_count_owner(const drats_block_pool_t *pool,
                               drats_block_owner_t owner) {
    if (pool == NULL) {
        return 0U;
    }
    size_t count = 0U;
    for (size_t i = 0; i < DRATS_POOL_CAPACITY; ++i) {
        if (atomic_load_explicit(&pool->blocks[i].owner, memory_order_acquire) ==
            (unsigned char)owner) {
            ++count;
        }
    }
    return count;
}
