#!/usr/bin/env python3
"""Validate target vector placement and boot-critical symbols."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import subprocess


def symbols(elf: Path) -> dict[str, int]:
    output = subprocess.run(
        ["arm-none-eabi-nm", "-n", str(elf)], check=True,
        capture_output=True, text=True).stdout
    found: dict[str, int] = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) == 3:
            try:
                found[fields[2]] = int(fields[0], 16)
            except ValueError:
                pass
    return found


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--flash-origin", type=lambda value: int(value, 0),
                        default=0x08020000)
    parser.add_argument("--flash-size", type=lambda value: int(value, 0),
                        default=768 * 1024)
    args = parser.parse_args()
    raw = args.binary.read_bytes()
    if len(raw) < 8:
        raise SystemExit("target binary has no complete vector table")
    initial_sp, reset_vector = struct.unpack_from("<II", raw)
    table = symbols(args.elf)
    checks = {
        "binary_fits_slot": len(raw) <= args.flash_size,
        "initial_sp_in_sram": 0x20000000 < initial_sp <= 0x200A0000,
        "reset_vector_is_thumb": bool(reset_vector & 1),
        "reset_vector_in_slot": args.flash_origin <= (reset_vector & ~1) <
                                args.flash_origin + args.flash_size,
        "vector_symbol_at_origin": table.get("g_pfnVectors") == args.flash_origin,
        "reset_symbol_matches_vector": table.get("Reset_Handler") ==
                                       (reset_vector & ~1),
    }
    report = {
        "binary_bytes": len(raw),
        "flash_origin": hex(args.flash_origin),
        "initial_sp": hex(initial_sp),
        "pass": all(checks.values()),
        "reset_vector": hex(reset_vector),
        "checks": checks,
    }
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
