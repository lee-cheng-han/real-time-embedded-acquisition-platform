# Implementation status

| Capability | Implemented | Host validated | Hardware validated |
|---|---:|---:|---:|
| fixed acquisition block ownership | yes | yes | no |
| binary protocol and CRC | yes | C/Python interoperability | no |
| acquisition/fault policy | foundation | yes | no |
| sensor simulation | yes | yes | n/a |
| Python capture decode/export/analysis | yes | yes | no |
| device configuration transport | yes | C/Python codec and target cross-build | no |
| STM32 startup/linker/CMSIS | yes | cross-build | no |
| FreeRTOS static task architecture | yes | cross-build | no |
| USART3 bounded interrupt TX/RX | yes | cross-build | no |
| ICM-42688-P register driver | yes | fake-bus unit tests and cross-build | no |
| SPI1/GPDMA real-sensor acquisition | selectable | cross-build | no |
| TIM3-triggered GPDMA acquisition | yes | cross-build | no |
| 64-bit TIM2 monotonic timestamp | yes | cross-build | no |
| watchdog supervision policy | yes | host tests | no |
| IWDG and reset-reason driver | selectable | cross-build | no |
| redundant persistent configuration records | yes | host power-interruption tests | no |
| signed update package tooling | yes, OpenSSL ECDSA-P256/SHA-256 | tamper/rollback integration test | n/a |
| target signed firmware update | metadata/state policy only | host tests with verifier callback | no |
