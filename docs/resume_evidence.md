# Resume evidence ledger

No resume bullet should use a row until its status is **validated**.

| Potential claim | Required evidence | Status |
|---|---|---|
| Designed deterministic, statically allocated acquisition pipeline | architecture review, ownership source, unit and integration results | host core validated; hardware pending |
| Implemented register-level SPI/GPDMA sensor acquisition on Cortex-M33 | target driver/source, identity and recovery tests, board log | source cross-builds; board log pending |
| Implemented hardware-timer-triggered acquisition | target driver source, scope trace, sustained-run log | TIM3/ISR/task source cross-builds; board trace pending |
| Used FreeRTOS ISR notifications and priority design | configuration/source plus runtime trace | target cross-build validated; runtime trace pending |
| Achieved a stated rate/latency/jitter | raw trace, analysis script, conditions, commit | no measurement |
| Recovered from SPI/overflow faults | injected-fault results and hardware logs | host policy and sensor fake-bus tested; hardware pending |
| Implemented observable health and reset diagnostics | protocol codec, device producer, Python decoder | cross-build/compatibility tested; live capture pending |
| Built ECDSA-P256 signed update packaging and verification | OpenSSL tool, tamper/rollback integration test | host tooling validated; do not claim target secure boot |
| Built power-loss-safe redundant boot policy | record codec/selection source and exhaustive cut-point test | host validated; physical flash pending |
| Implemented Cortex-M application handoff | bootloader source, linker/map/vector report, board boot log | cross-build validated; board boot log pending |
| Implemented signed, rollback-capable target update | bootloader source and reset-boundary test matrix | not implemented; do not claim |
