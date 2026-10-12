# Verification strategy

Host checks cover portable logic with GCC warnings-as-errors, AddressSanitizer and
UndefinedBehaviorSanitizer. CTest owns the test registry and can emit JUnit XML through
`scripts/test.sh`. Tests use deterministic seeds and explicit assertions.

The signed-image tooling test generates an ephemeral P-256 key, verifies a valid package,
then requires rejection of modified image bytes, authenticated metadata, and an
insufficient security counter. The full check also signs the target binary with the
workspace-local development key, compiles the exact bootloader Mbed TLS verifier for the host,
accepts the valid target package, and rejects image, signature, and rollback mutations.
It writes `build/host/boot-verifier-report.json`; CTest writes the complete host test
registry to `build/host/test-results.xml`.

| Layer | Pass criterion |
|---|---|
| unit | ring/pool boundaries, legal ownership, CRC vectors, frame round trip/rejection, state and configuration transitions all pass |
| integration | 100,000 simulated samples retain order; decoded telemetry matches source; all blocks return FREE |
| fault injection | delay, transfer error, saturation, corrupt packet, stale handle, and lost-completion policies reach their specified state/counter |
| endurance | configured duration completes with counters internally consistent and sanitizer-clean |
| timing trace | fixed ring wraps in sequence; decoder accepts committed records, rejects incomplete dumps, and emits JSON/CSV percentiles |
| target build | firmware and authenticated-development-bootloader ELF/map/bin artifacts build with zero first-party warnings, fit partition limits, pass vector/SP/reset-symbol checks, and produce a signed factory image |
| hardware | scripted smoke/endurance/fault runs produce raw machine-readable records and meet the gate in `architecture.md` |

Host simulation cannot validate NVIC priorities, DMA request routing, ISR latency,
timer jitter, memory bus contention, cache maintenance, UART electrical behavior,
watchdog reset, flash interruption, or physical boot handoff. Those checks remain
blocked until the board is present.
