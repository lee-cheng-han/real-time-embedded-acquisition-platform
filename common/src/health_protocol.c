#include "drats/health_protocol.h"

static void put_u32(uint8_t *destination, uint32_t value) {
    for (unsigned i = 0U; i < 4U; ++i) {
        destination[i] = (uint8_t)(value >> (8U * i));
    }
}

static uint32_t get_u32(const uint8_t *source) {
    uint32_t value = 0U;
    for (unsigned i = 0U; i < 4U; ++i) {
        value |= (uint32_t)source[i] << (8U * i);
    }
    return value;
}

drats_status_t drats_health_encode(
    const drats_health_report_t *report,
    uint8_t output[DRATS_HEALTH_PAYLOAD_SIZE]) {
    if ((report == NULL) || (output == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    const uint32_t fields[10] = {
        report->uptime_ms, report->reset_flags, report->acquisition_state,
        report->blocks_completed, report->samples_completed,
        report->dropped_blocks, report->queue_overflows, report->dma_errors,
        report->dma_timeouts, report->sequence_gaps
    };
    for (size_t i = 0U; i < 10U; ++i) {
        put_u32(&output[i * 4U], fields[i]);
    }
    return DRATS_OK;
}

drats_status_t drats_health_decode(const uint8_t *payload,
                                   size_t length,
                                   drats_health_report_t *report) {
    if ((payload == NULL) || (report == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    if (length != DRATS_HEALTH_PAYLOAD_SIZE) {
        return DRATS_E_SIZE;
    }
    uint32_t fields[10];
    for (size_t i = 0U; i < 10U; ++i) {
        fields[i] = get_u32(&payload[i * 4U]);
    }
    *report = (drats_health_report_t){
        .uptime_ms = fields[0],
        .reset_flags = fields[1],
        .acquisition_state = fields[2],
        .blocks_completed = fields[3],
        .samples_completed = fields[4],
        .dropped_blocks = fields[5],
        .queue_overflows = fields[6],
        .dma_errors = fields[7],
        .dma_timeouts = fields[8],
        .sequence_gaps = fields[9]
    };
    return DRATS_OK;
}
