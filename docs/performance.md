# Performance results

No physical-hardware performance result exists yet. Design targets live in
`architecture.md`; they must not be quoted as achieved metrics.

The current ARM linker report for the software-only target records 8,972 bytes in the
768 KiB application flash slot and 15,992 bytes of static RAM allocation. These figures
are reproducible build sizes, not runtime high-water measurements; the generated
`build/target/size-report.json` is the authoritative artifact and may change with code.

Host test duration and simulated throughput are useful for regression detection only.
They do not establish MCU acquisition rate, interrupt latency, jitter, CPU load, UART
throughput, SRAM/flash size, or recovery time. This file will link raw result artifacts
and reproduction commands once measurements are collected.
