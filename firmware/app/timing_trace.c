#include "timing_trace.h"

#if defined(DRATS_ENABLE_TRACE)

#include "tim2.h"

drats_trace_buffer_t drats_timing_trace;

void drats_timing_trace_init(void) {
    drats_trace_init(&drats_timing_trace, UINT64_C(1000000));
}

void drats_timing_trace_record(drats_trace_event_t event, uint16_t argument) {
    drats_trace_write(&drats_timing_trace, event, argument,
                      drats_tim2_now_us());
}

#endif
