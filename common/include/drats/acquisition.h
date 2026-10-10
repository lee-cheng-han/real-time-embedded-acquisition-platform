#ifndef DRATS_ACQUISITION_H
#define DRATS_ACQUISITION_H

#include "drats/block_pool.h"
#include "drats/handle_queue.h"
#include "drats/status.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DRATS_ACQ_STOPPED = 0,
    DRATS_ACQ_ARMED,
    DRATS_ACQ_RUNNING,
    DRATS_ACQ_RECOVERING,
    DRATS_ACQ_DEGRADED
} drats_acq_state_t;

typedef enum {
    DRATS_OVERFLOW_STOP = 0,
    DRATS_OVERFLOW_DROP_NEWEST
} drats_overflow_policy_t;

typedef struct {
    uint32_t sample_rate_hz;
    uint16_t samples_per_block;
    drats_overflow_policy_t overflow_policy;
} drats_acq_config_t;

typedef struct {
    uint64_t blocks_completed;
    uint64_t samples_completed;
    uint64_t dropped_blocks;
    uint64_t queue_overflows;
    uint64_t dma_errors;
    uint64_t dma_timeouts;
    uint64_t sequence_gaps;
} drats_acq_counters_t;

typedef struct {
    drats_acq_state_t state;
    drats_acq_config_t config;
    drats_acq_counters_t counters;
    uint32_t next_sequence;
    bool has_active_dma;
    drats_block_handle_t active_dma;
    drats_block_pool_t *pool;
    drats_handle_queue_t *ready_queue;
} drats_acquisition_t;

drats_status_t drats_acq_config_validate(const drats_acq_config_t *config);
drats_status_t drats_acq_init(drats_acquisition_t *acq,
                              drats_block_pool_t *pool,
                              drats_handle_queue_t *ready_queue,
                              const drats_acq_config_t *config);
drats_status_t drats_acq_start(drats_acquisition_t *acq);
drats_status_t drats_acq_arm_next(drats_acquisition_t *acq,
                                  drats_sample_block_t **block);
drats_status_t drats_acq_complete_dma(drats_acquisition_t *acq,
                                      uint64_t timestamp_ns,
                                      uint16_t sample_count);
drats_status_t drats_acq_fail_dma(drats_acquisition_t *acq, bool timed_out);
drats_status_t drats_acq_recovery_result(drats_acquisition_t *acq,
                                         bool revalidated);
drats_status_t drats_acq_stop(drats_acquisition_t *acq);

#endif
