#!/usr/bin/env python3
"""Inspect or export captured DRATS telemetry without third-party packages."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from drats.protocol import (  # noqa: E402
    ProtocolError, decode_config_ack, decode_health, decode_sample_block, decode_stream,
    encode_config_set
)
from drats.serial_transport import SerialTransport  # noqa: E402


def inspect_capture(path: Path) -> int:
    frames = list(decode_stream(path.read_bytes()))
    missing = sum(max(0, b.sequence - a.sequence - 1)
                  for a, b in zip(frames, frames[1:]))
    result = {
        "frames": len(frames),
        "missing_frame_sequences": missing,
        "first_sequence": frames[0].sequence if frames else None,
        "last_sequence": frames[-1].sequence if frames else None,
        "bytes": path.stat().st_size,
        "health_reports": sum(frame.message_type == 2 for frame in frames),
    }
    print(json.dumps(result, sort_keys=True))
    return 0 if frames else 1


def health_capture(path: Path) -> int:
    reports = [decode_health(frame) for frame in decode_stream(path.read_bytes())
               if frame.message_type == 2]
    if not reports:
        return 1
    fields = ("uptime_ms", "reset_flags", "acquisition_state",
              "blocks_completed", "samples_completed", "dropped_blocks",
              "queue_overflows", "dma_errors", "dma_timeouts",
              "sequence_gaps")
    print(json.dumps(dict(zip(fields, reports[-1])), sort_keys=True))
    return 0


def export_capture(path: Path, output: Path) -> int:
    frames = decode_stream(path.read_bytes())
    rows_written = 0
    with output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["sequence", "device_timestamp_ns", "channel", "value"])
        for frame in frames:
            try:
                first, period_ns, samples = decode_sample_block(frame)
            except ProtocolError:
                continue
            for sample_index, sample in enumerate(samples):
                timestamp = frame.timestamp_ns + sample_index * period_ns
                for channel, value in enumerate(sample):
                    writer.writerow([first + sample_index, timestamp, channel, value])
                    rows_written += 1
    print(json.dumps({"rows": rows_written, "output": str(output)}, sort_keys=True))
    return 0


def configure_device(device: str, request: bytes, request_id: int,
                     baud: int, timeout: float) -> int:
    deadline = time.monotonic() + timeout
    captured = bytearray()
    with SerialTransport(device, baud) as serial:
        serial.write(request, timeout=min(timeout, 1.0))
        while time.monotonic() < deadline:
            remaining = max(0.0, deadline - time.monotonic())
            captured.extend(serial.read(timeout=min(0.1, remaining)))
            for frame in decode_stream(bytes(captured)):
                if frame.message_type != 17:
                    continue
                ack = decode_config_ack(frame)
                if ack[0] == request_id:
                    result = {
                        "request_id": ack[0], "status": ack[1],
                        "sample_rate_hz": ack[2], "samples_per_block": ack[3],
                        "overflow_policy": ack[4], "telemetry_enabled": ack[5],
                    }
                    print(json.dumps(result, sort_keys=True))
                    return 0 if ack[1] == 0 else 1
    raise TimeoutError(f"no configuration acknowledgement from {device}")


def capture_device(device: str, output: Path, baud: int, duration: float) -> int:
    if duration <= 0:
        raise ValueError("capture duration must be positive")
    deadline = time.monotonic() + duration
    captured = bytearray()
    started = time.monotonic()
    with SerialTransport(device, baud) as serial:
        while time.monotonic() < deadline:
            remaining = max(0.0, deadline - time.monotonic())
            captured.extend(serial.read(timeout=min(0.1, remaining)))
    elapsed = time.monotonic() - started
    output.write_bytes(captured)
    frames = list(decode_stream(bytes(captured)))
    missing = sum(max(0, current.sequence - previous.sequence - 1)
                  for previous, current in zip(frames, frames[1:]))
    print(json.dumps({
        "bytes": len(captured), "duration_seconds": elapsed,
        "frames": len(frames), "missing_frame_sequences": missing,
        "output": str(output),
        "throughput_bytes_per_second": len(captured) / elapsed,
    }, sort_keys=True))
    return 0 if frames else 1


def main() -> int:
    parser = argparse.ArgumentParser(prog="drats-cli")
    sub = parser.add_subparsers(dest="command", required=True)
    inspect_parser = sub.add_parser("inspect")
    inspect_parser.add_argument("capture", type=Path)
    export_parser = sub.add_parser("export")
    export_parser.add_argument("capture", type=Path)
    export_parser.add_argument("output", type=Path)
    health_parser = sub.add_parser("health")
    health_parser.add_argument("capture", type=Path)
    config_parser = sub.add_parser("make-config")
    config_parser.add_argument("output", type=Path)
    config_parser.add_argument("--rate", type=int, required=True)
    config_parser.add_argument("--block-size", type=int, default=32)
    config_parser.add_argument("--overflow", choices=("stop", "drop-newest"),
                               default="stop")
    config_parser.add_argument("--disable-telemetry", action="store_true")
    config_parser.add_argument("--request-id", type=int, default=1)
    live_parser = sub.add_parser("configure")
    live_parser.add_argument("device")
    live_parser.add_argument("--rate", type=int, required=True)
    live_parser.add_argument("--block-size", type=int, default=32)
    live_parser.add_argument("--overflow", choices=("stop", "drop-newest"),
                             default="stop")
    live_parser.add_argument("--disable-telemetry", action="store_true")
    live_parser.add_argument("--request-id", type=int, default=1)
    live_parser.add_argument("--baud", type=int, default=2_000_000)
    live_parser.add_argument("--timeout", type=float, default=2.0)
    capture_parser = sub.add_parser("capture")
    capture_parser.add_argument("device")
    capture_parser.add_argument("output", type=Path)
    capture_parser.add_argument("--baud", type=int, default=2_000_000)
    capture_parser.add_argument("--duration", type=float, default=10.0)
    args = parser.parse_args()
    if args.command == "inspect":
        return inspect_capture(args.capture)
    if args.command == "health":
        return health_capture(args.capture)
    if args.command == "make-config":
        frame = encode_config_set(
            frame_sequence=0, request_id=args.request_id,
            sample_rate_hz=args.rate, samples_per_block=args.block_size,
            overflow_policy=0 if args.overflow == "stop" else 1,
            telemetry_enabled=not args.disable_telemetry)
        args.output.write_bytes(frame)
        print(json.dumps({"bytes": len(frame), "output": str(args.output)},
                         sort_keys=True))
        return 0
    if args.command == "configure":
        frame = encode_config_set(
            frame_sequence=0, request_id=args.request_id,
            sample_rate_hz=args.rate, samples_per_block=args.block_size,
            overflow_policy=0 if args.overflow == "stop" else 1,
            telemetry_enabled=not args.disable_telemetry)
        return configure_device(args.device, frame, args.request_id,
                                args.baud, args.timeout)
    if args.command == "capture":
        return capture_device(args.device, args.output, args.baud, args.duration)
    return export_capture(args.capture, args.output)


if __name__ == "__main__":
    raise SystemExit(main())
