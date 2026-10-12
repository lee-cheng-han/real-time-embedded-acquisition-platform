# Timing measurement plan

The sensor build records TIM3 triggers, acquisition-task wakes, DMA completions,
processing completions, UART submissions, and recovery entries in a 256-entry static
ring named `drats_timing_trace`. Each 24-byte record has a sequence number, event,
argument, 64-bit TIM2 microsecond timestamp, and release-published commit word. Writers
reserve records atomically, so ISR and task events share the trace without a mutex.
Tracing is optional because its timestamp and atomic reservation add measurement
overhead; `scripts/check_all.sh` enables it for the sensor validation artifact.

With the target halted over SWD, extract the complete trace in GDB:

```gdb
dump binary memory build/timing-trace.bin &drats_timing_trace ((char *)&drats_timing_trace + sizeof(drats_timing_trace))
```

Decode it into machine-readable JSON and CSV:

```sh
python3 scripts/decode_timing_trace.py build/timing-trace.bin \
  --output build/timing-trace.json --csv build/timing-trace.csv
```

The decoder rejects incomplete records and reports p50, p95, p99, and maximum intervals
per event type. It also pairs trigger-to-task-wake, task-wake-to-DMA-complete, and
processing-to-UART-submit events for latency distributions. GPIO edges and an external
logic analyzer should independently validate the trace overhead and interrupt timing.
DWT CYCCNT remains available for shorter CPU intervals where the 1 us TIM2 resolution
is insufficient.

Reports must include firmware commit, compiler flags, clock tree, acquisition settings,
sensor/SPI rate, telemetry link rate, trace overhead, run duration, sample count, p50,
p95, p99, maximum, and loss counters. Host/device latency is reported only after clock
synchronization or as separate device and transport intervals. No measurements have
yet been collected.
