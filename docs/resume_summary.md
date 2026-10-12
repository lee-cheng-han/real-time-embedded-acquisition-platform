# Resume-ready project summary

Use these bullets now; they describe evidence available without implying board results.

- Architected a deterministic C17/FreeRTOS acquisition and telemetry platform for an
  STM32H563 Cortex-M33, using static allocation, generation-tagged buffer ownership,
  bounded SPSC queues, CRC32C framing, runtime configuration, and five priority-ordered
  tasks; validated the portable core with 12 sanitizer-backed unit, integration, fault,
  endurance, interoperability, cryptographic, and trace-format tests.
- Implemented and cross-compiled register-level TIM2/TIM3, SPI1 GPDMA, USART3 interrupt,
  GPIO, reset-reason, and watchdog paths for an ICM-42688-P sensor, including indexed
  ISR-to-task notifications, bounded timeouts, re-probe recovery, health telemetry, and
  a fixed binary timing trace with JSON/CSV percentile analysis.
- Built an authenticated development boot chain with pinned Mbed TLS ECDSA-P256/SHA-256
  verification, signed factory-image tooling, vector/MSP/VTOR handoff, monotonic-counter
  policy, and redundant commit-last boot records; native tests reject payload/signature
  tampering, rollback, and every simulated interrupted record write.

After hardware validation, replace software qualifiers only with measured evidence from
`docs/hardware_bringup.md`. Strong hardware-backed bullets should include the sustained
sample rate and duration, p99/max trigger-to-task and transfer latency, observed loss,
fault recovery time, stack high-water marks, and the exact test commit. Do not call the
development key a production root of trust or claim A/B flash recovery until those
paths exist and pass physical reset-boundary tests.
