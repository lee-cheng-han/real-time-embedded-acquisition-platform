#!/usr/bin/env python3
"""Create or verify a signed DRATS update package."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from drats.update_image import build_package, verify_package  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(prog="drats-image")
    commands = parser.add_subparsers(dest="command", required=True)
    pack = commands.add_parser("pack")
    pack.add_argument("image", type=Path)
    pack.add_argument("private_key", type=Path)
    pack.add_argument("output", type=Path)
    pack.add_argument("--version", nargs=3, type=int, required=True,
                      metavar=("MAJOR", "MINOR", "PATCH"))
    pack.add_argument("--security-counter", type=int, required=True)
    verify = commands.add_parser("verify")
    verify.add_argument("package", type=Path)
    verify.add_argument("public_key", type=Path)
    verify.add_argument("--minimum-security-counter", type=int, default=0)
    args = parser.parse_args()
    if args.command == "pack":
        package = build_package(args.image.read_bytes(), args.private_key,
                                tuple(args.version), args.security_counter)
        args.output.write_bytes(package)
        print(json.dumps({"bytes": len(package), "output": str(args.output)},
                         sort_keys=True))
        return 0
    info = verify_package(args.package.read_bytes(), args.public_key,
                          args.minimum_security_counter)
    print(json.dumps({
        "image_size": info.image_size,
        "key_id": info.key_id.hex(),
        "security_counter": info.security_counter,
        "version": ".".join(map(str, info.version)),
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
