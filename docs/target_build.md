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

The current application uses a 1 ksample/s simulated six-channel backend. It exercises
the block pool, processing handoff, telemetry encoder, and bounded USART3 TX ring. The
register-level UART mapping is USART3 on PD8/PD9 AF7, as specified for the Nucleo-144
COM1 port by ST's board support package. No physical execution has been claimed.

GPDMA channel 0 has a conservative ISR shell that snapshots and clears all channel
status flags before notifying the acquisition task. Channel configuration, DMAMUX
request selection, SPI setup, timer trigger routing, and sensor chip-select timing are
not implemented until the actual sensor is selected and the request routing is checked.
