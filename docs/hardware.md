# Hardware baseline

## Recommended board

Use the **NUCLEO-H563ZI** (MB1404) with its STM32H563ZIT6 MCU. ST documents a
250 MHz Arm Cortex-M33, 2 MiB dual-bank flash, 640 KiB SRAM, multiple 16/32-bit
timers and communication peripherals. The Nucleo-144 board includes an ST-LINK
debugger/programmer with SWD and USB virtual COM, so no separate probe is required.
The board was listed by ST at USD 43.50 when this baseline was written; price and
availability can change.

Primary references:

- [NUCLEO-H563ZI product page](https://www.st.com/en/evaluation-tools/nucleo-h563zi.html)
- [STM32H563/573 documentation index](https://www.st.com/en/microcontrollers-microprocessors/stm32h563-573/documentation.html)
- [RM0481 reference manual](https://www.st.com/resource/en/reference_manual/rm0481-stm32h563h573-and-stm32h562-armbased-32bit-mcus-stmicroelectronics.pdf)

The H563 has TrustZone and a dual-bank flash organization, but it does not provide the
H573's AES/PKA/OTFDEC hardware set or immutable ST-iRoT. The planned updater therefore
uses an established software signature implementation. Calling it secure boot requires
later protection of the boot root and public key through reviewed option-byte,
TrustZone, lifecycle, and debug policy. CRC alone is only corruption detection.

## Purchase and wiring plan

Required: NUCLEO-H563ZI, a 3.3 V ICM-42688-P breakout, jumper wires, and USB cable. A
logic analyzer or oscilloscope is strongly recommended for independent jitter and
ISR-latency measurements. TDK specifies a 24 MHz maximum SPI clock; firmware limits
SPI1 to 16 MHz and verifies the `WHO_AM_I` value before enabling acquisition.

Proposed wiring is SPI1 SCK/MISO/MOSI on PA5/PA6/PA7 (AF5), software chip select on
PD14, 3.3 V, and ground. PD14 corresponds to the intended Arduino D10 routing, but this
mapping must be checked against the exact MB1404 board revision before power-up.

Primary sensor references:

- [ICM-42688-P product page](https://www.invensense.tdk.com/en-us/products/6-axis/icm-42688-p/)
- [ICM-42688-P datasheet](https://invensense.tdk.com/wp-content/uploads/2020/04/ds-000347_icm-42688-p-datasheet.pdf)

Before energizing the sensor, verify supply voltage, SPI mode 0, chip-select timing,
and pin mapping against the breakout and MB1404 schematics.
