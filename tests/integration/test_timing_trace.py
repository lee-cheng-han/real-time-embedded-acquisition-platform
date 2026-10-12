#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

MAGIC = 0x45435254
HEADER = struct.Struct("<IHHIIQ")
RECORD = struct.Struct("<IHHQII")


def main() -> int:
    if len(sys.argv) != 2:
        return 2
    decoder = Path(sys.argv[1])
    capacity = 8
    events = ((1, 100), (2, 112), (3, 125), (1, 1100),
              (2, 1115), (3, 1130), (1, 2100), (1, 3100))
    dump = bytearray(HEADER.pack(MAGIC, 1, RECORD.size, capacity,
                                 len(events), 1_000_000))
    for sequence, (event, timestamp) in enumerate(events):
        dump.extend(RECORD.pack(sequence, event, 0, timestamp, sequence, 0))
    with tempfile.TemporaryDirectory() as directory:
        input_path = Path(directory) / "trace.bin"
        output_path = Path(directory) / "trace.json"
        csv_path = Path(directory) / "trace.csv"
        input_path.write_bytes(dump)
        subprocess.run([sys.executable, str(decoder), str(input_path),
                        "--output", str(output_path), "--csv", str(csv_path)],
                       check=True)
        report = json.loads(output_path.read_text(encoding="utf-8"))
        assert report["record_count"] == 8
        assert report["intervals"]["sample_trigger"]["p99_us"] == 1000.0
        assert report["latencies"]["trigger_to_acquisition_wake"]["max_us"] == 15.0
        assert report["latencies"]["acquisition_wake_to_dma_complete"]["max_us"] == 15.0
        assert len(csv_path.read_text(encoding="utf-8").splitlines()) == 9
        dump[-4:] = b"\x01\x00\x00\x00"
        input_path.write_bytes(dump)
        rejected = subprocess.run(
            [sys.executable, str(decoder), str(input_path)],
            capture_output=True)
        assert rejected.returncode != 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
