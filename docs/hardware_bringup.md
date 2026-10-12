# Hardware bring-up and evidence runbook

This runbook starts from a clean checkout and preserves the raw artifacts needed to
turn software-validated claims into hardware-validated claims. It has not yet been run
because no board is connected.

## Build and inspect

Run the complete build before connecting hardware:

```sh
./scripts/check_all.sh
cat build/factory/sensor-factory-report.json
cat build/target-sensor/size-report.json
```

The report must say `"pass": true`. `drats_sensor_factory.bin` contains the
authenticated development bootloader at `0x08000000`, the real-sensor application at
`0x08020000`, and its signed metadata at `0x081e0000`.

## Wiring and pre-power checks

Use a NUCLEO-H563ZI and a 3.3 V ICM-42688-P breakout. With power disconnected, verify
continuity and absence of shorts, then connect PA5/SCK, PA6/MISO, PA7/MOSI, PD14/CS,
3.3 V, and ground. Confirm the breakout accepts 3.3 V and check its pin labels against
its own schematic. The board-revision schematic remains authoritative for header pins.

## Program and first boot

After installing STM32CubeProgrammer, inspect its local `--help` output and program the
factory image over the onboard ST-LINK. The expected CLI form is:

```sh
STM32_Programmer_CLI -c port=SWD mode=UR -e all \
  -w build/factory/drats_sensor_factory.bin 0x08000000 -v -rst
```

Keep the full programmer log under `build/hardware/`. A successful verify is evidence
that bytes reached flash; it is not evidence that authenticated handoff or acquisition
worked.

## Serial smoke test

Find the ST-LINK virtual COM device, create an evidence directory, and capture the raw
stream. Replace `/dev/ttyACM0` only after identifying the actual device.

```sh
mkdir -p build/hardware
python3 host/cli/drats_cli.py capture /dev/ttyACM0 \
  build/hardware/smoke.drat --baud 2000000 --duration 30 \
  | tee build/hardware/capture-report.json
python3 host/cli/drats_cli.py inspect build/hardware/smoke.drat \
  | tee build/hardware/inspect-report.json
python3 host/cli/drats_cli.py health build/hardware/smoke.drat \
  | tee build/hardware/health-report.json
python3 host/benchmarks/analyze.py build/hardware/smoke.drat \
  --svg build/hardware/smoke-jitter.svg \
  | tee build/hardware/jitter-report.json
```

Pass criteria are at least one sample frame and one health frame, monotonically
increasing frame/sample sequences, zero unexplained gaps, RUNNING acquisition state,
and zero DMA errors/timeouts after sensor identity succeeds.

Apply a configuration and require an acknowledgement:

```sh
python3 host/cli/drats_cli.py configure /dev/ttyACM0 --rate 1000 \
  --block-size 32 --overflow stop --request-id 100 \
  | tee build/hardware/config-ack.json
```

## Timing trace

Halt the target after a representative run and connect `gdb-multiarch` to the active
SWD GDB server. Extract the trace with the command in `docs/timing.md`, then decode it:

```sh
python3 scripts/decode_timing_trace.py build/hardware/timing-trace.bin \
  --output build/hardware/timing-trace.json \
  --csv build/hardware/timing-trace.csv
```

Record compiler version, commit, clock configuration, sensor configuration, trace
duration, and logic-analyzer setup alongside the result. Treat the first trace as
diagnostic; publish latency or jitter only after an external GPIO measurement bounds
instrumentation error.

## Fault and endurance gates

Run each fault separately and preserve its capture: disconnect/reconnect the sensor,
force telemetry congestion by pausing the reader, inject an invalid configuration,
and allow the watchdog to expire with an intentionally stalled test build. A recovery
passes only when the sensor is re-probed, acquisition returns to RUNNING, and counters
show the event. Power interruption during flash erase/program cannot be claimed until
a real flash updater exists.

For the sustained gate, capture for 12 hours at 1 ksample/s. Require no unexplained
sequence gaps or ownership failures, health counters consistent with the host decoder,
no watchdog resets, and stable stack high-water evidence once that hardware-only
measurement is added. Archive raw capture, JSON reports, firmware ELF/map, factory
report, tool versions, and the Git commit together.
