# Resume evidence ledger

No resume bullet should use a row until its status is **validated**.

| Potential claim | Required evidence | Status |
|---|---|---|
| Designed deterministic, statically allocated acquisition pipeline | architecture review, ownership source, unit and integration results | host core validated; hardware pending |
| Implemented timer-triggered SPI DMA on Cortex-M33 | target driver source, scope trace, sustained-run log | blocked on target integration/hardware |
| Used FreeRTOS ISR notifications and priority design | configuration/source plus runtime trace | target cross-build validated; runtime trace pending |
| Achieved a stated rate/latency/jitter | raw trace, analysis script, conditions, commit | no measurement |
| Recovered from SPI/DMA/overflow faults | injected-fault results and hardware logs | host policy tested; hardware pending |
| Implemented signed, rollback-capable update | bootloader source and reset-boundary test matrix | design only |
