# Performance results

No physical-hardware performance result exists yet. Design targets live in
`architecture.md`; they must not be quoted as achieved metrics.

The current ARM linker report for the simulator-backend target records 14,696 bytes in
the 768 KiB application flash slot and 18,036 bytes against the conservative 128 KiB
RAM budget. The sensor/watchdog/timing-trace variant records 17,992 flash bytes and
24,268 RAM bytes; its optional trace accounts for 6,168 bytes of static RAM. These figures
are reproducible build sizes, not runtime high-water measurements; the generated
`build/target/size-report.json` and `build/target-sensor/size-report.json` are the
authoritative artifacts and may change with code.

Host test duration and simulated throughput are useful for regression detection only.
They do not establish MCU acquisition rate, interrupt latency, jitter, CPU load, UART
throughput, SRAM/flash size, or recovery time. This file will link raw result artifacts
and reproduction commands once measurements are collected.
