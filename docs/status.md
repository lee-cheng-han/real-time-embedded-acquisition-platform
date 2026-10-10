# Implementation status

| Capability | Implemented | Host validated | Hardware validated |
|---|---:|---:|---:|
| fixed acquisition block ownership | yes | yes | no |
| binary protocol and CRC | yes | C/Python interoperability | no |
| acquisition/fault policy | foundation | yes | no |
| sensor simulation | yes | yes | n/a |
| Python capture decode/export/analysis | yes | yes | no |
| device configuration transport | no | no | no |
| STM32 startup/linker/CMSIS | yes | cross-build | no |
| FreeRTOS static task architecture | yes | cross-build | no |
| USART3 bounded interrupt TX | yes | cross-build | no |
| real SPI/GPDMA acquisition | no | no | no |
| watchdog supervision policy | yes | host tests | no |
| redundant persistent configuration records | yes | host power-interruption tests | no |
| signed firmware update | metadata/state policy only | host tests with verifier callback | no |
