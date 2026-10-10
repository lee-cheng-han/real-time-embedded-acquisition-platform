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

Required: NUCLEO-H563ZI, a 3.3 V SPI sensor/breakout with a documented maximum SPI
clock, jumper wires, and USB cable. A logic analyzer or oscilloscope is strongly
recommended for independent jitter and ISR-latency measurements. The exact sensor is
deliberately not selected in software yet; the real driver contract is separated from
the simulator and will be bound after choosing hardware.

Before energizing a sensor, record its supply voltage, SPI mode, maximum bus rate,
chip-select timing, startup delay, and pin mapping here. Never infer Arduino-header
labels to MCU alternate functions without checking the MB1404 schematic.
