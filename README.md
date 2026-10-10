# DRATS — Deterministic Real-Time Acquisition & Telemetry System

DRATS is an embedded acquisition platform for the STM32H563ZI. Its end-to-end workload
is periodic SPI sampling into explicitly owned fixed buffers, bounded
processing, and CRC-protected binary telemetry. This repository currently delivers the
host-verifiable core, simulator, and cross-compiled STM32 firmware. It does **not**
claim physical hardware operation or hardware performance.

## What works today

- strict C17 acquisition state machine and static block pool;
- generation-tagged `FREE -> DMA -> READY -> PROCESSING -> TELEMETRY -> FREE`
  ownership with atomic publication;
- bounded single-producer/single-consumer handle queue with explicit full/empty results;
- versioned little-endian telemetry frames with CRC32C;
- deterministic six-channel simulated sensor pipeline;
- dependency-free Python decoding, gap detection, CSV export, and timing analysis;
- live POSIX serial capture and acknowledged runtime configuration;
- a cross-linked Cortex-M33 FreeRTOS image with five static tasks and USART3 telemetry;
- an optional ICM-42688-P backend using register-level SPI1, TIM2 timestamps, sensor
  identity checks, bounded timeouts, and revalidation after transfer failure;
- reset-reason capture and an optional supervisor-gated independent watchdog;
- unit, 100,000-sample integration, cross-language compatibility, and injected-fault
  tests under AddressSanitizer and UndefinedBehaviorSanitizer;
- a hardware-grounded architecture, verification plan, update design, and claim ledger.

The live feature matrix is in [docs/status.md](docs/status.md). Hardware timing targets
in [docs/architecture.md](docs/architecture.md) are requirements, not achieved results.

## Build and test on Linux/WSL

Requirements are CMake 3.20+, a C17 GCC or Clang compiler, Make or Ninja, Bash, and
Python 3. No Python packages are needed.

```sh
cd /home/leech/real-time-embedded-acquisition-platform
./scripts/test.sh
```

The script configures `build/host`, builds with strict warnings and sanitizers, runs
CTest, and writes machine-readable results to `build/host/test-results.xml`.

Generate and inspect a simulated binary capture:

```sh
build/host/drats_sim 1000 > build/host/capture.drat
python3 host/cli/drats_cli.py inspect build/host/capture.drat
python3 host/cli/drats_cli.py export build/host/capture.drat build/host/capture.csv
python3 host/benchmarks/analyze.py build/host/capture.drat --svg build/host/jitter.svg
```

The analysis describes timestamps contained in the capture. Simulator output is not a
hardware benchmark.

## Hardware plan

The baseline is the NUCLEO-H563ZI, selected for its STM32H563ZIT6 (250 MHz Cortex-M33,
2 MiB dual-bank flash, 640 KiB SRAM), timer/SPI/GPDMA resources, and onboard ST-LINK
with virtual COM. See [docs/hardware.md](docs/hardware.md) for primary ST references
and caveats. The selected sensor is a 3.3 V ICM-42688-P SPI breakout.

The ARM cross compiler is installed and target startup, linker placement, FreeRTOS
static tasks, fault capture, and an interrupt-driven USART3 telemetry path build.
No board is connected, so physical execution, timer-triggered GPDMA acquisition,
watchdog-reset validation, flash update, and timing measurements remain blocked.

Build both host evidence and the target artifacts:

```sh
./scripts/check_all.sh
```

Target artifacts are written under `build/target`; see
[docs/target_build.md](docs/target_build.md) for their scope and limitations.

## Repository map

```text
bootloader/       reserved for target boot/update implementation
common/           portable protocol, ownership, acquisition, and fault policy
firmware/         STM32/FreeRTOS application, startup, and register drivers
host/             Python capture, export, and analysis tools
simulation/       deterministic C sensor/pipeline simulator
tests/            unit, integration, fault-injection, and endurance tests
docs/             architecture, protocol, hardware, verification, and evidence
scripts/          reproducible build/test entry points
```

## Roadmap gates

1. Portable core — implemented and host-tested.
2. Pin/clock/DMA design in CubeMX, checked against board schematic, DS14121 and RM0481.
3. CMSIS/startup/linker and FreeRTOS target build using `arm-none-eabi-gcc`.
4. Validate the TIM3-triggered SPI GPDMA path on NUCLEO-H563ZI.
5. Supervisor, watchdog, persistent diagnostics, and hardware fault injection.
6. Established-library signed update with power-interruption tests.
7. Reproducible hardware endurance and timing report.

## Design constraints

There is no dynamic allocation in steady-state C code. The queue is SPSC; adding a
second producer or consumer requires a different primitive. Payload buffers remain in
the pool through telemetry completion, so this path makes no false zero-copy claim:
samples are copied once into the encoded UART frame by the current protocol encoder.
