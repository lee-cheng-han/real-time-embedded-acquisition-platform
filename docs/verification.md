# Verification strategy

Host checks cover portable logic with GCC warnings-as-errors, AddressSanitizer and
UndefinedBehaviorSanitizer. CTest owns the test registry and can emit JUnit XML through
`scripts/test.sh`. Tests use deterministic seeds and explicit assertions.

| Layer | Pass criterion |
|---|---|
| unit | ring/pool boundaries, legal ownership, CRC vectors, frame round trip/rejection, state and configuration transitions all pass |
| integration | 100,000 simulated samples retain order; decoded telemetry matches source; all blocks return FREE |
| fault injection | delay, transfer error, saturation, corrupt packet, stale handle, and lost-completion policies reach their specified state/counter |
| endurance | configured duration completes with counters internally consistent and sanitizer-clean |
| target build | firmware and bootloader ELF/map/bin artifacts build with zero warnings and fit partition limits |
| hardware | scripted smoke/endurance/fault runs produce raw machine-readable records and meet the gate in `architecture.md` |

Host simulation cannot validate NVIC priorities, DMA request routing, ISR latency,
timer jitter, memory bus contention, cache maintenance, UART electrical behavior,
watchdog reset, flash interruption, or boot handoff. Those checks remain blocked until
the board is present and the target build exists.
