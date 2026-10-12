#ifndef DRATS_TIMING_TRACE_H
#define DRATS_TIMING_TRACE_H

#include "drats/trace.h"

#include <stdint.h>

#if defined(DRATS_ENABLE_TRACE)
extern drats_trace_buffer_t drats_timing_trace;
void drats_timing_trace_init(void);
void drats_timing_trace_record(drats_trace_event_t event, uint16_t argument);
#else
static inline void drats_timing_trace_init(void) {
}
static inline void drats_timing_trace_record(drats_trace_event_t event,
                                             uint16_t argument) {
    (void)event;
    (void)argument;
}
#endif

#endif
