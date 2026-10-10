"""Build and verify DRATS ECDSA-P256/SHA-256 update packages with OpenSSL."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile

IMAGE_MAGIC = 0x474D4944
FORMAT_VERSION = 1
SIGNATURE_ALGORITHM = 1
SIGNATURE_MAX = 72
METADATA = struct.Struct("<IHHIIIIIHH8s32s72s")
AUTHENTICATED_METADATA = struct.Struct("<IHHIIIIIH8s32s")
assert METADATA.size == 144


class ImageError(ValueError):
    pass


@dataclass(frozen=True)
class ImageInfo:
    image_size: int
    version: tuple[int, int, int]
    security_counter: int
    key_id: bytes
    digest: bytes
    signature: bytes


def _run(arguments: list[str], *, input_data: bytes | None = None) -> bytes:
    try:
        return subprocess.run(arguments, input=input_data, check=True,
                              capture_output=True).stdout
    except (OSError, subprocess.CalledProcessError) as error:
        raise ImageError(f"OpenSSL operation failed: {arguments[1]}") from error


def public_key_der(key_path: Path, *, private: bool) -> bytes:
    command = ["openssl", "pkey"]
    if not private:
        command.append("-pubin")
    command.extend(["-in", str(key_path), "-pubout", "-outform", "DER"])
    return _run(command)


def build_package(image: bytes, private_key: Path,
                  version: tuple[int, int, int],
                  security_counter: int) -> bytes:
    if not image or any(part < 0 or part > 0xFFFFFFFF for part in version):
        raise ImageError("invalid image or semantic version")
    if security_counter < 0 or security_counter > 0xFFFFFFFF:
        raise ImageError("security counter is outside uint32 range")
    digest = hashlib.sha256(image).digest()
    key_id = hashlib.sha256(public_key_der(private_key, private=True)).digest()[:8]
    authenticated = AUTHENTICATED_METADATA.pack(
        IMAGE_MAGIC, FORMAT_VERSION, METADATA.size, len(image), *version,
        security_counter, SIGNATURE_ALGORITHM, key_id, digest)
    with tempfile.TemporaryDirectory() as directory:
        manifest_path = Path(directory) / "manifest.bin"
        signature_path = Path(directory) / "signature.der"
        manifest_path.write_bytes(authenticated)
        _run(["openssl", "dgst", "-sha256", "-sign", str(private_key),
              "-out", str(signature_path), str(manifest_path)])
        signature = signature_path.read_bytes()
    if not 1 <= len(signature) <= SIGNATURE_MAX:
        raise ImageError("ECDSA signature exceeds metadata capacity")
    metadata = METADATA.pack(
        IMAGE_MAGIC, FORMAT_VERSION, METADATA.size, len(image),
        *version, security_counter, SIGNATURE_ALGORITHM, len(signature),
        key_id, digest, signature.ljust(SIGNATURE_MAX, b"\0"))
    return metadata + image


def parse_package(package: bytes) -> tuple[ImageInfo, bytes]:
    if len(package) < METADATA.size:
        raise ImageError("package is shorter than its metadata")
    values = METADATA.unpack_from(package)
    (magic, format_version, header_size, image_size, major, minor, patch,
     counter, algorithm, signature_length, key_id, digest,
     signature_field) = values
    if magic != IMAGE_MAGIC or format_version != FORMAT_VERSION:
        raise ImageError("unsupported image metadata")
    if header_size != METADATA.size or algorithm != SIGNATURE_ALGORITHM:
        raise ImageError("invalid image metadata")
    if not 1 <= signature_length <= SIGNATURE_MAX:
        raise ImageError("invalid signature length")
    if len(package) != header_size + image_size:
        raise ImageError("package length does not match metadata")
    info = ImageInfo(image_size, (major, minor, patch), counter, key_id,
                     digest, signature_field[:signature_length])
    return info, package[header_size:]


def verify_package(package: bytes, public_key: Path,
                   minimum_security_counter: int = 0) -> ImageInfo:
    info, image = parse_package(package)
    if info.security_counter < minimum_security_counter:
        raise ImageError("image security counter is below the minimum")
    if hashlib.sha256(image).digest() != info.digest:
        raise ImageError("image digest mismatch")
    expected_key_id = hashlib.sha256(
        public_key_der(public_key, private=False)).digest()[:8]
    if expected_key_id != info.key_id:
        raise ImageError("signing key identifier mismatch")
    authenticated = AUTHENTICATED_METADATA.pack(
        IMAGE_MAGIC, FORMAT_VERSION, METADATA.size, info.image_size,
        *info.version, info.security_counter, SIGNATURE_ALGORITHM,
        info.key_id, info.digest)
    with tempfile.TemporaryDirectory() as directory:
        manifest_path = Path(directory) / "manifest.bin"
        signature_path = Path(directory) / "signature.der"
        manifest_path.write_bytes(authenticated)
        signature_path.write_bytes(info.signature)
        _run(["openssl", "dgst", "-sha256", "-verify", str(public_key),
              "-signature", str(signature_path), str(manifest_path)])
    return info
