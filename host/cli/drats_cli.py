#!/usr/bin/env python3
"""Inspect or export captured DRATS telemetry without third-party packages."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from drats.protocol import (  # noqa: E402
    ProtocolError, decode_sample_block, decode_stream, encode_config_set
)


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
    }
    print(json.dumps(result, sort_keys=True))
    return 0 if frames else 1


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


def main() -> int:
    parser = argparse.ArgumentParser(prog="drats-cli")
    sub = parser.add_subparsers(dest="command", required=True)
    inspect_parser = sub.add_parser("inspect")
    inspect_parser.add_argument("capture", type=Path)
    export_parser = sub.add_parser("export")
    export_parser.add_argument("capture", type=Path)
    export_parser.add_argument("output", type=Path)
    config_parser = sub.add_parser("make-config")
    config_parser.add_argument("output", type=Path)
    config_parser.add_argument("--rate", type=int, required=True)
    config_parser.add_argument("--block-size", type=int, default=32)
    config_parser.add_argument("--overflow", choices=("stop", "drop-newest"),
                               default="stop")
    config_parser.add_argument("--disable-telemetry", action="store_true")
    config_parser.add_argument("--request-id", type=int, default=1)
    args = parser.parse_args()
    if args.command == "inspect":
        return inspect_capture(args.capture)
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
    return export_capture(args.capture, args.output)


if __name__ == "__main__":
    raise SystemExit(main())
