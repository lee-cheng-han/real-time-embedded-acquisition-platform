# Verification strategy

Host checks cover portable logic with GCC warnings-as-errors, AddressSanitizer and
UndefinedBehaviorSanitizer. CTest owns the test registry and can emit JUnit XML through
`scripts/test.sh`. Tests use deterministic seeds and explicit assertions.

The signed-image integration test generates an ephemeral P-256 key, verifies a valid
package, then requires rejection of modified image bytes, authenticated metadata, and
an insufficient security counter. Keys and packages remain inside a temporary directory.

| Layer | Pass criterion |
|---|---|
| unit | ring/pool boundaries, legal ownership, CRC vectors, frame round trip/rejection, state and configuration transitions all pass |
| integration | 100,000 simulated samples retain order; decoded telemetry matches source; all blocks return FREE |
| fault injection | delay, transfer error, saturation, corrupt packet, stale handle, and lost-completion policies reach their specified state/counter |
| endurance | configured duration completes with counters internally consistent and sanitizer-clean |
| target build | firmware and development-bootloader ELF/map/bin artifacts build with zero warnings, fit partition limits, and pass vector/SP/reset-symbol checks |
| hardware | scripted smoke/endurance/fault runs produce raw machine-readable records and meet the gate in `architecture.md` |

Host simulation cannot validate NVIC priorities, DMA request routing, ISR latency,
timer jitter, memory bus contention, cache maintenance, UART electrical behavior,
watchdog reset, flash interruption, or boot handoff. Those checks remain blocked until
the board is present and the target build exists.
