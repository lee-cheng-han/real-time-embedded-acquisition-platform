#include "drats/acquisition.h"
#include "drats/fault_manager.h"
#include "test_support.h"

int main(void) {
    drats_fault_manager_t manager;
    drats_fault_manager_init(&manager);
    CHECK(drats_fault_record(&manager, DRATS_FAULT_DMA_TIMEOUT) ==
          DRATS_ACTION_RESTART_PERIPHERAL);
    CHECK(manager.health == DRATS_HEALTH_RECOVERING);
    CHECK(!drats_fault_recovery_result(&manager, DRATS_FAULT_DMA_TIMEOUT, false));
    CHECK(manager.health == DRATS_HEALTH_DEGRADED);
    CHECK(drats_fault_recovery_result(&manager, DRATS_FAULT_DMA_TIMEOUT, true));
    CHECK(manager.health == DRATS_HEALTH_OK);
    CHECK(drats_fault_record(&manager, DRATS_FAULT_BUFFER_OVERFLOW) ==
          DRATS_ACTION_STOP_ACQUISITION);
    CHECK(manager.health == DRATS_HEALTH_DEGRADED);
    CHECK(drats_fault_record(&manager, DRATS_FAULT_TASK_INACTIVE) ==
          DRATS_ACTION_REQUEST_RESET);

    drats_block_pool_t pool;
    drats_handle_queue_t queue;
    drats_block_pool_init(&pool);
    drats_handle_queue_init(&queue);
    const drats_acq_config_t config = {
        .sample_rate_hz = 1000U,
        .samples_per_block = 8U,
        .overflow_policy = DRATS_OVERFLOW_STOP
    };
    drats_acquisition_t acq;
    drats_sample_block_t *block = NULL;
    CHECK(drats_acq_init(&acq, &pool, &queue, &config) == DRATS_OK);
    CHECK(drats_acq_start(&acq) == DRATS_OK);
    CHECK(drats_acq_arm_next(&acq, &block) == DRATS_OK);
    CHECK(drats_acq_fail_dma(&acq, true) == DRATS_OK);
    CHECK(acq.state == DRATS_ACQ_RECOVERING);
    CHECK(acq.counters.dma_timeouts == 1U);
    CHECK(drats_block_count_owner(&pool, DRATS_BLOCK_FREE) == DRATS_POOL_CAPACITY);
    return 0;
}
