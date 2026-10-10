#!/usr/bin/env python3
"""Combine the development bootloader and slot-A application into one flash image."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

BOOT_CAPACITY = 0x20000
APP_OFFSET = 0x20000
APP_CAPACITY = 0xC0000
SRAM_ORIGIN = 0x20000000
SRAM_LIMIT = 0x200A0000
FLASH_ORIGIN = 0x08000000


def valid_vectors(data: bytes, image_origin: int, image_capacity: int) -> bool:
    if len(data) < 8:
        return False
    stack, reset = struct.unpack_from("<II", data)
    return (SRAM_ORIGIN < stack <= SRAM_LIMIT and stack % 8 == 0 and
            reset & 1 == 1 and
            image_origin <= (reset & ~1) < image_origin + image_capacity)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("bootloader", type=Path)
    parser.add_argument("application", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    bootloader = args.bootloader.read_bytes()
    application = args.application.read_bytes()
    checks = {
        "application_fits": len(application) <= APP_CAPACITY,
        "application_vectors_valid": valid_vectors(
            application, FLASH_ORIGIN + APP_OFFSET, APP_CAPACITY),
        "bootloader_fits": len(bootloader) <= BOOT_CAPACITY,
        "bootloader_vectors_valid": valid_vectors(
            bootloader, FLASH_ORIGIN, BOOT_CAPACITY),
    }
    if not all(checks.values()):
        raise SystemExit(f"factory image validation failed: {checks}")
    combined = bytearray(b"\xff" * (APP_OFFSET + len(application)))
    combined[:len(bootloader)] = bootloader
    combined[APP_OFFSET:] = application
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(combined)
    report = {
        "application_bytes": len(application),
        "application_offset": hex(APP_OFFSET),
        "bootloader_bytes": len(bootloader),
        "checks": checks,
        "output": str(args.output),
        "output_bytes": len(combined),
        "pass": True,
        "sha256": hashlib.sha256(combined).hexdigest(),
    }
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
