#include "drats/acquisition.h"
#include "test_support.h"

#include <stdint.h>

int main(void) {
    drats_block_pool_t pool;
    drats_handle_queue_t queue;
    drats_block_pool_init(&pool);
    drats_handle_queue_init(&queue);
    const drats_acq_config_t config = {
        .sample_rate_hz = 10000U,
        .samples_per_block = DRATS_SAMPLES_PER_BLOCK,
        .overflow_policy = DRATS_OVERFLOW_STOP
    };
    drats_acquisition_t acq;
    CHECK(drats_acq_init(&acq, &pool, &queue, &config) == DRATS_OK);
    CHECK(drats_acq_start(&acq) == DRATS_OK);

    uint32_t successful_blocks = 0U;
    uint32_t injected_timeouts = 0U;
    for (uint32_t iteration = 0; iteration < 31250U; ++iteration) {
        drats_sample_block_t *block = NULL;
        CHECK(drats_acq_arm_next(&acq, &block) == DRATS_OK);
        block->samples[0][0] = (int16_t)(iteration & 0x7fffU);
        if ((iteration != 0U) && ((iteration % 997U) == 0U)) {
            CHECK(drats_acq_fail_dma(&acq, true) == DRATS_OK);
            CHECK(drats_acq_recovery_result(&acq, true) == DRATS_OK);
            ++injected_timeouts;
            continue;
        }
        CHECK(drats_acq_complete_dma(&acq,
                                     (uint64_t)iteration * UINT64_C(3200000),
                                     DRATS_SAMPLES_PER_BLOCK) == DRATS_OK);
        drats_block_handle_t handle;
        CHECK(drats_handle_queue_pop(&queue, &handle) == DRATS_OK);
        CHECK(drats_block_transition(&pool, handle, DRATS_BLOCK_READY,
                                     DRATS_BLOCK_PROCESSING) == DRATS_OK);
        CHECK(drats_block_transition(&pool, handle, DRATS_BLOCK_PROCESSING,
                                     DRATS_BLOCK_TELEMETRY) == DRATS_OK);
        CHECK(drats_block_transition(&pool, handle, DRATS_BLOCK_TELEMETRY,
                                     DRATS_BLOCK_FREE) == DRATS_OK);
        ++successful_blocks;
    }
    CHECK(successful_blocks == 31250U - injected_timeouts);
    CHECK(acq.counters.dma_timeouts == injected_timeouts);
    CHECK(acq.counters.samples_completed ==
          (uint64_t)successful_blocks * DRATS_SAMPLES_PER_BLOCK);
    CHECK(acq.counters.dropped_blocks == 0U);
    CHECK(drats_block_count_owner(&pool, DRATS_BLOCK_FREE) == DRATS_POOL_CAPACITY);
    return 0;
}
