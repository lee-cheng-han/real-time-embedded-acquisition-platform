#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "host"))

from drats.update_image import ImageError, build_package, verify_package  # noqa: E402


def main() -> int:
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        private_key = root / "private.pem"
        public_key = root / "public.pem"
        subprocess.run(["openssl", "genpkey", "-algorithm", "EC",
                        "-pkeyopt", "ec_paramgen_curve:P-256",
                        "-out", str(private_key)], check=True,
                       capture_output=True)
        subprocess.run(["openssl", "pkey", "-in", str(private_key),
                        "-pubout", "-out", str(public_key)], check=True,
                       capture_output=True)
        image = bytes(range(256)) * 4
        package = build_package(image, private_key, (1, 4, 2), 7)
        info = verify_package(package, public_key, 7)
        assert info.image_size == len(image)
        assert info.version == (1, 4, 2) and info.security_counter == 7
        for tampered in (package[:-1] + bytes([package[-1] ^ 1]),
                         package[:20] + b"\0\0\0\0" + package[24:]):
            try:
                verify_package(tampered, public_key)
            except ImageError:
                pass
            else:
                raise AssertionError("tampered package was accepted")
        try:
            verify_package(package, public_key, 8)
        except ImageError:
            pass
        else:
            raise AssertionError("rollback counter was accepted")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
