#include "drats/trace.h"

#include <stddef.h>

_Static_assert(sizeof(atomic_uint_least32_t) == sizeof(uint32_t),
               "trace format requires 32-bit atomic words");
_Static_assert(offsetof(drats_trace_slot_t, timestamp) == 8U,
               "trace record layout changed");
_Static_assert(offsetof(drats_trace_slot_t, committed_sequence) == 16U,
               "trace commit layout changed");
_Static_assert(sizeof(drats_trace_slot_t) == 24U,
               "trace record size changed");
_Static_assert(offsetof(drats_trace_buffer_t, records) == 24U,
               "trace header size changed");

void drats_trace_init(drats_trace_buffer_t *trace, uint64_t tick_hz) {
    if ((trace == NULL) || (tick_hz == 0U)) {
        return;
    }
    trace->magic = DRATS_TRACE_MAGIC;
    trace->version = DRATS_TRACE_VERSION;
    trace->record_size = (uint16_t)sizeof(drats_trace_slot_t);
    trace->capacity = DRATS_TRACE_CAPACITY;
    atomic_init(&trace->next_sequence, 0U);
    trace->tick_hz = tick_hz;
    for (size_t i = 0U; i < DRATS_TRACE_CAPACITY; ++i) {
        trace->records[i].sequence = 0U;
        trace->records[i].event = 0U;
        trace->records[i].argument = 0U;
        trace->records[i].timestamp = 0U;
        atomic_init(&trace->records[i].committed_sequence,
                    DRATS_TRACE_INVALID_SEQUENCE);
        trace->records[i].reserved = 0U;
    }
}

void drats_trace_write(drats_trace_buffer_t *trace,
                       drats_trace_event_t event,
                       uint16_t argument,
                       uint64_t timestamp) {
    if ((trace == NULL) || (event < DRATS_TRACE_SAMPLE_TRIGGER) ||
        (event > DRATS_TRACE_RECOVERY)) {
        return;
    }
    const uint32_t sequence = atomic_fetch_add_explicit(
        &trace->next_sequence, 1U, memory_order_relaxed);
    drats_trace_slot_t *const slot =
        &trace->records[sequence % DRATS_TRACE_CAPACITY];
    atomic_store_explicit(&slot->committed_sequence,
                          DRATS_TRACE_INVALID_SEQUENCE,
                          memory_order_relaxed);
    slot->sequence = sequence;
    slot->event = (uint16_t)event;
    slot->argument = argument;
    slot->timestamp = timestamp;
    slot->reserved = 0U;
    atomic_store_explicit(&slot->committed_sequence, sequence,
                          memory_order_release);
}

drats_status_t drats_trace_snapshot(const drats_trace_buffer_t *trace,
                                    drats_trace_sample_t *output,
                                    size_t capacity,
                                    size_t *count) {
    if ((trace == NULL) || (output == NULL) || (count == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    if ((trace->magic != DRATS_TRACE_MAGIC) ||
        (trace->version != DRATS_TRACE_VERSION) ||
        (trace->record_size != sizeof(drats_trace_slot_t)) ||
        (trace->capacity != DRATS_TRACE_CAPACITY) ||
        (trace->tick_hz == 0U)) {
        return DRATS_E_FORMAT;
    }
    const uint32_t next = atomic_load_explicit(
        &trace->next_sequence, memory_order_acquire);
    const size_t available = next < DRATS_TRACE_CAPACITY ?
                             (size_t)next : DRATS_TRACE_CAPACITY;
    if (capacity < available) {
        return DRATS_E_SIZE;
    }
    const uint32_t first = next - (uint32_t)available;
    for (size_t i = 0U; i < available; ++i) {
        const uint32_t expected = first + (uint32_t)i;
        const drats_trace_slot_t *const slot =
            &trace->records[expected % DRATS_TRACE_CAPACITY];
        if (atomic_load_explicit(&slot->committed_sequence,
                                 memory_order_acquire) != expected) {
            return DRATS_E_STALE;
        }
        output[i] = (drats_trace_sample_t){
            .sequence = slot->sequence,
            .event = slot->event,
            .argument = slot->argument,
            .timestamp = slot->timestamp
        };
        if ((output[i].sequence != expected) ||
            (atomic_load_explicit(&slot->committed_sequence,
                                  memory_order_acquire) != expected)) {
            return DRATS_E_STALE;
        }
    }
    *count = available;
    return DRATS_OK;
}
