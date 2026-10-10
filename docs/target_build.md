# STM32H563 target build

The target is a secure-state, TrustZone-disabled FreeRTOS application for the
STM32H563ZIT6. It links at `0x08020000`, the proposed application-A slot. ST's startup
file relocates VTOR through `VECT_TAB_OFFSET=0x20000`. It will therefore require a
bootloader handoff or debugger setup before it can execute; it is not presently a
standalone image for address `0x08000000`.

`scripts/build_target.sh` downloads pinned official sources into ignored `build/deps`:

| Dependency | Commit |
|---|---|
| STM32CubeH5 CMSIS core | `e5c73ea1bbe384cdadb0cf161ae97a0157990766` |
| CMSIS Device H5 v1.7.0 | `884b8dc78e41cbfca008363342b17f4a9e8641f7` |
| FreeRTOS-Kernel | `8be86d4a24fd4091f8f4192018423ab590f408db` |

The build uses Cortex-M33 Thumb soft-float flags, function/data sections, garbage
collection, strict warnings, newlib-nano, and static FreeRTOS allocation. It produces
ELF, BIN, HEX and linker map files. `scripts/check_size.py` enforces a 768 KiB flash-slot
limit and the architecture's 128 KiB initial RAM budget, writing `size-report.json`.

`scripts/build_bootloader.sh` builds a separate development handoff image at
`0x08000000`. It occupies 1,332 flash bytes in the reserved 128 KiB partition and its
post-build report verifies the initial stack pointer, Thumb reset vector, vector-table
origin, reset symbol, and partition bound. It performs vector sanity checks only and is
not the authenticated update bootloader described in `firmware_update.md`.
`scripts/check_all.sh` also creates a validated, SHA-256-reported factory binary with
the development bootloader at offset zero and application A at offset `0x20000`.

The default application uses a configurable simulated six-channel backend. It exercises
the block pool, processing handoff, telemetry encoder, and bounded USART3 TX/RX rings. The
register-level UART mapping is USART3 on PD8/PD9 AF7, as specified for the Nucleo-144
COM1 port by ST's board support package. No physical execution has been claimed.

Set `DRATS_REAL_SENSOR=ON` to build the ICM-42688-P backend and
`DRATS_ENABLE_WATCHDOG=ON` to include IWDG activation. `scripts/check_all.sh` builds
this combination separately under `build/target-sensor`. It samples the sensor at
1 ksample/s from TIM3 notifications through bounded register-level SPI/GPDMA transfers
and timestamps blocks with a 1 MHz TIM2 counter. Timer and DMA completion use separate
indexed task notifications. The path is cross-compiled only and needs board validation.

GPDMA channels 0 and 1 are configured for SPI1 RX/TX requests and notify the acquisition
task after completion or error. TIM3 drives the acquisition cadence; the fixed block
pool provides multiple owned buffers rather than GPDMA linked-list double buffering.
SPI setup and chip-select timing have not been electrically checked.
