# Timing measurement plan

Firmware will record timer ticks for trigger, DMA completion, task wake, processing
completion, and UART submission in fixed binary trace records. DWT CYCCNT measures
short CPU intervals; a free-running 32-bit timer supplies event timestamps; GPIO edges
allow an external logic analyzer to validate both. Counter wrap is extended explicitly.

Reports must include firmware commit, compiler flags, clock tree, acquisition settings,
sensor/SPI rate, telemetry link rate, trace overhead, run duration, sample count, p50,
p95, p99, maximum, and loss counters. Host/device latency is reported only after clock
synchronization or as separate device and transport intervals. No measurements have
yet been collected.
