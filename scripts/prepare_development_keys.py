#!/usr/bin/env python3
"""Create an ignored development signing key and its generated C public key."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import subprocess


def run(arguments: list[str]) -> bytes:
    return subprocess.run(arguments, check=True, capture_output=True).stdout


def format_bytes(data: bytes) -> str:
    rows = []
    for offset in range(0, len(data), 8):
        values = ", ".join(f"0x{value:02x}" for value in data[offset:offset + 8])
        rows.append(f"    {values}")
    return ",\n".join(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    private_key = args.output_dir / "development-private.pem"
    public_key = args.output_dir / "development-public.pem"
    if args.force or not private_key.exists():
        run(["openssl", "genpkey", "-algorithm", "EC", "-pkeyopt",
             "ec_paramgen_curve:P-256", "-out", str(private_key)])
    run(["openssl", "pkey", "-in", str(private_key), "-pubout",
         "-out", str(public_key)])
    public_der = run(["openssl", "pkey", "-pubin", "-in", str(public_key),
                      "-pubout", "-outform", "DER"])
    public_point = public_der[-65:]
    if len(public_point) != 65 or public_point[0] != 4:
        raise RuntimeError("OpenSSL did not produce an uncompressed P-256 point")
    key_id = hashlib.sha256(public_der).digest()[:8]

    header = f"""#ifndef DRATS_DEVELOPMENT_PUBLIC_KEY_H
#define DRATS_DEVELOPMENT_PUBLIC_KEY_H

#include <stdint.h>

static const uint8_t drats_development_public_key[65] = {{
{format_bytes(public_point)}
}};

static const uint8_t drats_development_key_id[8] = {{
{format_bytes(key_id)}
}};

#endif
"""
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.header.write_text(header, encoding="utf-8")
    print(f"development key ID: {key_id.hex()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
