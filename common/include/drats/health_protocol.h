#ifndef DRATS_HEALTH_PROTOCOL_H
#define DRATS_HEALTH_PROTOCOL_H

#include "drats/status.h"

#include <stddef.h>
#include <stdint.h>

#define DRATS_HEALTH_PAYLOAD_SIZE 40U

typedef struct {
    uint32_t uptime_ms;
    uint32_t reset_flags;
    uint32_t acquisition_state;
    uint32_t blocks_completed;
    uint32_t samples_completed;
    uint32_t dropped_blocks;
    uint32_t queue_overflows;
    uint32_t dma_errors;
    uint32_t dma_timeouts;
    uint32_t sequence_gaps;
} drats_health_report_t;

drats_status_t drats_health_encode(
    const drats_health_report_t *report,
    uint8_t output[DRATS_HEALTH_PAYLOAD_SIZE]);
drats_status_t drats_health_decode(const uint8_t *payload,
                                   size_t length,
                                   drats_health_report_t *report);

#endif
