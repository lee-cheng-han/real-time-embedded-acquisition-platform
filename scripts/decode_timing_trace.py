#!/usr/bin/env python3
"""Decode a halted-target dump of the DRATS fixed timing trace."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
import struct

MAGIC = 0x45435254
VERSION = 1
HEADER = struct.Struct("<IHHIIQ")
RECORD = struct.Struct("<IHHQII")
EVENT_NAMES = {
    1: "sample_trigger",
    2: "acquisition_wake",
    3: "dma_complete",
    4: "processing_complete",
    5: "uart_submit",
    6: "recovery",
}


def percentile(values: list[float], percent: int) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    index = ((len(ordered) - 1) * percent + 99) // 100
    return ordered[index]


def statistics(values: list[float]) -> dict[str, float | int | None]:
    return {
        "count": len(values),
        "p50_us": percentile(values, 50),
        "p95_us": percentile(values, 95),
        "p99_us": percentile(values, 99),
        "max_us": max(values) if values else None,
    }


def decode(data: bytes) -> dict[str, object]:
    if len(data) < HEADER.size:
        raise ValueError("trace dump is shorter than its header")
    magic, version, record_size, capacity, next_sequence, tick_hz = \
        HEADER.unpack_from(data)
    if (magic != MAGIC or version != VERSION or record_size != RECORD.size or
            capacity == 0 or tick_hz == 0):
        raise ValueError("unsupported or corrupt trace header")
    expected_size = HEADER.size + capacity * record_size
    if len(data) != expected_size:
        raise ValueError("trace dump size does not match its header")

    records: list[dict[str, int | str]] = []
    available = min(next_sequence, capacity)
    first = (next_sequence - available) & 0xFFFFFFFF
    for index in range(available):
        sequence = (first + index) & 0xFFFFFFFF
        slot = sequence % capacity
        offset = HEADER.size + slot * record_size
        stored, event, argument, timestamp, committed, reserved = \
            RECORD.unpack_from(data, offset)
        if stored != sequence or committed != sequence or reserved != 0:
            raise ValueError(f"trace slot for sequence {sequence} is incomplete")
        records.append({
            "sequence": sequence,
            "event": EVENT_NAMES.get(event, f"unknown_{event}"),
            "event_id": event,
            "argument": argument,
            "timestamp_ticks": timestamp,
        })

    by_event: dict[str, list[float]] = {}
    previous: dict[str, int] = {}
    for record in records:
        name = str(record["event"])
        timestamp = int(record["timestamp_ticks"])
        if name in previous:
            interval_us = (timestamp - previous[name]) * 1_000_000.0 / tick_hz
            by_event.setdefault(name, []).append(interval_us)
        previous[name] = timestamp
    interval_stats = {name: statistics(values)
                      for name, values in sorted(by_event.items())}

    pair_definitions = {
        "trigger_to_acquisition_wake": ("sample_trigger", "acquisition_wake"),
        "acquisition_wake_to_dma_complete": ("acquisition_wake", "dma_complete"),
        "processing_complete_to_uart_submit": ("processing_complete", "uart_submit"),
    }
    latency_values: dict[str, list[float]] = {
        name: [] for name in pair_definitions
    }
    starts: dict[str, int] = {}
    for record in records:
        event = str(record["event"])
        timestamp = int(record["timestamp_ticks"])
        for name, (start_event, end_event) in pair_definitions.items():
            if event == start_event:
                starts[name] = timestamp
            elif event == end_event and name in starts:
                latency_values[name].append(
                    (timestamp - starts.pop(name)) * 1_000_000.0 / tick_hz)
    latency_stats = {name: statistics(values)
                     for name, values in latency_values.items()}
    return {
        "capacity": capacity,
        "next_sequence": next_sequence,
        "record_count": len(records),
        "tick_hz": tick_hz,
        "intervals": interval_stats,
        "latencies": latency_stats,
        "records": records,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--csv", type=Path)
    args = parser.parse_args()
    report = decode(args.input.read_bytes())
    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output is None:
        print(encoded, end="")
    else:
        args.output.write_text(encoded, encoding="utf-8")
    if args.csv is not None:
        with args.csv.open("w", newline="", encoding="utf-8") as output:
            writer = csv.DictWriter(output, fieldnames=(
                "sequence", "event", "event_id", "argument",
                "timestamp_ticks"))
            writer.writeheader()
            writer.writerows(report["records"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
