#include "drats/acquisition.h"

#include <string.h>

drats_status_t drats_acq_config_validate(const drats_acq_config_t *config) {
    if (config == NULL) {
        return DRATS_E_ARGUMENT;
    }
    if ((config->sample_rate_hz < 1U) || (config->sample_rate_hz > 50000U) ||
        (config->samples_per_block < 1U) ||
        (config->samples_per_block > DRATS_SAMPLES_PER_BLOCK) ||
        (config->overflow_policy > DRATS_OVERFLOW_DROP_NEWEST)) {
        return DRATS_E_ARGUMENT;
    }
    return DRATS_OK;
}

drats_status_t drats_acq_init(drats_acquisition_t *acq,
                              drats_block_pool_t *pool,
                              drats_handle_queue_t *ready_queue,
                              const drats_acq_config_t *config) {
    if ((acq == NULL) || (pool == NULL) || (ready_queue == NULL) ||
        (drats_acq_config_validate(config) != DRATS_OK)) {
        return DRATS_E_ARGUMENT;
    }
    memset(acq, 0, sizeof(*acq));
    acq->config = *config;
    acq->pool = pool;
    acq->ready_queue = ready_queue;
    acq->state = DRATS_ACQ_STOPPED;
    return DRATS_OK;
}

drats_status_t drats_acq_start(drats_acquisition_t *acq) {
    if ((acq == NULL) || (acq->state != DRATS_ACQ_STOPPED)) {
        return DRATS_E_STATE;
    }
    acq->state = DRATS_ACQ_ARMED;
    return DRATS_OK;
}

drats_status_t drats_acq_reconfigure(drats_acquisition_t *acq,
                                     const drats_acq_config_t *config) {
    if ((acq == NULL) || (acq->state != DRATS_ACQ_STOPPED) ||
        acq->has_active_dma ||
        (drats_acq_config_validate(config) != DRATS_OK)) {
        return DRATS_E_STATE;
    }
    acq->config = *config;
    return DRATS_OK;
}

drats_status_t drats_acq_arm_next(drats_acquisition_t *acq,
                                  drats_sample_block_t **block) {
    if ((acq == NULL) || (block == NULL) ||
        ((acq->state != DRATS_ACQ_ARMED) &&
         (acq->state != DRATS_ACQ_RUNNING))) {
        return DRATS_E_STATE;
    }
    if (acq->has_active_dma) {
        return DRATS_E_STATE;
    }
    drats_status_t status = drats_block_acquire_dma(
        acq->pool, &acq->active_dma, block);
    if (status == DRATS_E_FULL) {
        ++acq->counters.queue_overflows;
        ++acq->counters.dropped_blocks;
        if (acq->config.overflow_policy == DRATS_OVERFLOW_STOP) {
            acq->state = DRATS_ACQ_DEGRADED;
        }
        return status;
    }
    if (status == DRATS_OK) {
        acq->has_active_dma = true;
        acq->state = DRATS_ACQ_RUNNING;
    }
    return status;
}

drats_status_t drats_acq_complete_dma(drats_acquisition_t *acq,
                                      uint64_t timestamp_ns,
                                      uint16_t sample_count) {
    if ((acq == NULL) || !acq->has_active_dma ||
        (acq->state != DRATS_ACQ_RUNNING) || (sample_count == 0U) ||
        (sample_count > acq->config.samples_per_block)) {
        return DRATS_E_STATE;
    }
    drats_sample_block_t *block = NULL;
    drats_status_t status = drats_block_get(acq->pool, acq->active_dma,
                                             DRATS_BLOCK_DMA, &block);
    if (status != DRATS_OK) {
        return status;
    }
    block->sequence_first = acq->next_sequence;
    block->timestamp_first_ns = timestamp_ns;
    block->sample_period_ns = UINT32_C(1000000000) / acq->config.sample_rate_hz;
    block->sample_count = sample_count;
    block->flags = 0U;
    status = drats_block_transition(acq->pool, acq->active_dma,
                                    DRATS_BLOCK_DMA, DRATS_BLOCK_READY);
    if (status != DRATS_OK) {
        return status;
    }
    status = drats_handle_queue_push(acq->ready_queue, acq->active_dma);
    if (status != DRATS_OK) {
        (void)drats_block_transition(acq->pool, acq->active_dma,
                                     DRATS_BLOCK_READY, DRATS_BLOCK_FREE);
        ++acq->counters.queue_overflows;
        ++acq->counters.dropped_blocks;
        if (acq->config.overflow_policy == DRATS_OVERFLOW_STOP) {
            acq->state = DRATS_ACQ_DEGRADED;
        }
    } else {
        ++acq->counters.blocks_completed;
        acq->counters.samples_completed += sample_count;
        acq->next_sequence += sample_count;
    }
    acq->has_active_dma = false;
    return status;
}

drats_status_t drats_acq_fail_dma(drats_acquisition_t *acq, bool timed_out) {
    if ((acq == NULL) || !acq->has_active_dma) {
        return DRATS_E_STATE;
    }
    drats_status_t status = drats_block_transition(
        acq->pool, acq->active_dma, DRATS_BLOCK_DMA, DRATS_BLOCK_FREE);
    if (status != DRATS_OK) {
        return status;
    }
    if (timed_out) {
        ++acq->counters.dma_timeouts;
    } else {
        ++acq->counters.dma_errors;
    }
    acq->has_active_dma = false;
    acq->state = DRATS_ACQ_RECOVERING;
    return DRATS_OK;
}

drats_status_t drats_acq_recovery_result(drats_acquisition_t *acq,
                                         bool revalidated) {
    if ((acq == NULL) || (acq->state != DRATS_ACQ_RECOVERING) ||
        acq->has_active_dma) {
        return DRATS_E_STATE;
    }
    acq->state = revalidated ? DRATS_ACQ_ARMED : DRATS_ACQ_DEGRADED;
    return revalidated ? DRATS_OK : DRATS_E_STATE;
}

drats_status_t drats_acq_stop(drats_acquisition_t *acq) {
    if (acq == NULL) {
        return DRATS_E_ARGUMENT;
    }
    if (acq->has_active_dma) {
        drats_status_t status = drats_block_transition(
            acq->pool, acq->active_dma, DRATS_BLOCK_DMA, DRATS_BLOCK_FREE);
        if (status != DRATS_OK) {
            return status;
        }
        acq->has_active_dma = false;
    }
    acq->state = DRATS_ACQ_STOPPED;
    return DRATS_OK;
}
