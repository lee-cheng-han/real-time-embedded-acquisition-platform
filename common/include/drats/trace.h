#ifndef DRATS_TRACE_H
#define DRATS_TRACE_H

#include "drats/status.h"

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#define DRATS_TRACE_MAGIC UINT32_C(0x45435254)
#define DRATS_TRACE_VERSION 1U
#define DRATS_TRACE_CAPACITY 256U
#define DRATS_TRACE_INVALID_SEQUENCE UINT32_MAX

typedef enum {
    DRATS_TRACE_SAMPLE_TRIGGER = 1,
    DRATS_TRACE_ACQUISITION_WAKE = 2,
    DRATS_TRACE_DMA_COMPLETE = 3,
    DRATS_TRACE_PROCESSING_COMPLETE = 4,
    DRATS_TRACE_UART_SUBMIT = 5,
    DRATS_TRACE_RECOVERY = 6
} drats_trace_event_t;

typedef struct {
    uint32_t sequence;
    uint16_t event;
    uint16_t argument;
    uint64_t timestamp;
    atomic_uint_least32_t committed_sequence;
    uint32_t reserved;
} drats_trace_slot_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t record_size;
    uint32_t capacity;
    atomic_uint_least32_t next_sequence;
    uint64_t tick_hz;
    drats_trace_slot_t records[DRATS_TRACE_CAPACITY];
} drats_trace_buffer_t;

typedef struct {
    uint32_t sequence;
    uint16_t event;
    uint16_t argument;
    uint64_t timestamp;
} drats_trace_sample_t;

void drats_trace_init(drats_trace_buffer_t *trace, uint64_t tick_hz);
void drats_trace_write(drats_trace_buffer_t *trace,
                       drats_trace_event_t event,
                       uint16_t argument,
                       uint64_t timestamp);
drats_status_t drats_trace_snapshot(const drats_trace_buffer_t *trace,
                                    drats_trace_sample_t *output,
                                    size_t capacity,
                                    size_t *count);

#endif
