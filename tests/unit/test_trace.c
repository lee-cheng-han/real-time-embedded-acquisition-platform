#include "drats/trace.h"
#include "test_support.h"

#include <stddef.h>

int main(void) {
    drats_trace_buffer_t trace;
    drats_trace_init(&trace, UINT64_C(1000000));
    for (uint32_t i = 0U; i < DRATS_TRACE_CAPACITY + 17U; ++i) {
        drats_trace_write(&trace, DRATS_TRACE_SAMPLE_TRIGGER,
                          (uint16_t)(i & UINT16_MAX), (uint64_t)i * 10U);
    }
    drats_trace_sample_t samples[DRATS_TRACE_CAPACITY];
    size_t count = 0U;
    CHECK(drats_trace_snapshot(&trace, samples, DRATS_TRACE_CAPACITY,
                               &count) == DRATS_OK);
    CHECK(count == DRATS_TRACE_CAPACITY);
    CHECK(samples[0].sequence == 17U);
    CHECK(samples[0].timestamp == 170U);
    CHECK(samples[count - 1U].sequence == DRATS_TRACE_CAPACITY + 16U);
    CHECK(samples[count - 1U].argument == DRATS_TRACE_CAPACITY + 16U);
    CHECK(drats_trace_snapshot(&trace, samples, count - 1U, &count) ==
          DRATS_E_SIZE);
    atomic_store(&trace.records[20U].committed_sequence,
                 DRATS_TRACE_INVALID_SEQUENCE);
    CHECK(drats_trace_snapshot(&trace, samples, DRATS_TRACE_CAPACITY,
                               &count) == DRATS_E_STALE);
    return 0;
}
