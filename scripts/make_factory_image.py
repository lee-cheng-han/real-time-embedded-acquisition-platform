#!/usr/bin/env python3
"""Combine the development bootloader and slot-A application into one flash image."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "host"))

from drats.update_image import METADATA, build_package, verify_package

BOOT_CAPACITY = 0x20000
APP_OFFSET = 0x20000
APP_CAPACITY = 0xC0000
METADATA_OFFSET = 0x1E0000
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
    parser.add_argument("--package", type=Path)
    parser.add_argument("--private-key", type=Path, required=True)
    parser.add_argument("--public-key", type=Path, required=True)
    parser.add_argument("--version", type=int, nargs=3, default=(1, 0, 0),
                        metavar=("MAJOR", "MINOR", "PATCH"))
    parser.add_argument("--security-counter", type=int, default=1)
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

    package = build_package(application, args.private_key,
                            tuple(args.version), args.security_counter)
    info = verify_package(package, args.public_key, 1)
    metadata = package[:METADATA.size]
    combined = bytearray(b"\xff" * (METADATA_OFFSET + len(metadata)))
    combined[:len(bootloader)] = bootloader
    combined[APP_OFFSET:APP_OFFSET + len(application)] = application
    combined[METADATA_OFFSET:] = metadata
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(combined)
    if args.package is not None:
        args.package.parent.mkdir(parents=True, exist_ok=True)
        args.package.write_bytes(package)
    report = {
        "application_bytes": len(application),
        "application_offset": hex(APP_OFFSET),
        "bootloader_bytes": len(bootloader),
        "checks": checks,
        "image_sha256": info.digest.hex(),
        "key_id": info.key_id.hex(),
        "metadata_bytes": len(metadata),
        "metadata_offset": hex(METADATA_OFFSET),
        "output": str(args.output),
        "output_bytes": len(combined),
        "package": str(args.package) if args.package is not None else None,
        "package_bytes": len(package),
        "pass": True,
        "security_counter": info.security_counter,
        "sha256": hashlib.sha256(combined).hexdigest(),
        "version": ".".join(str(part) for part in info.version),
    }
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
