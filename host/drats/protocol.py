"""Dependency-free decoder for DRATS protocol version 1."""

from __future__ import annotations

from dataclasses import dataclass
import struct
from typing import Iterator

MAGIC = b"DRAT"
VERSION = 1
HEADER = struct.Struct("<4sBBHIQHH")
CRC = struct.Struct("<I")
MAX_PAYLOAD = 1024


class ProtocolError(ValueError):
    """Raised when a complete candidate frame is invalid."""


def crc32c(data: bytes, seed: int = 0) -> int:
    crc = (~seed) & 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            mask = -(crc & 1) & 0xFFFFFFFF
            crc = ((crc >> 1) ^ (0x82F63B78 & mask)) & 0xFFFFFFFF
    return (~crc) & 0xFFFFFFFF


@dataclass(frozen=True)
class Frame:
    message_type: int
    flags: int
    sequence: int
    timestamp_ns: int
    payload: bytes


def encode_frame(frame: Frame) -> bytes:
    if not 0 <= frame.message_type <= 0xFF:
        raise ProtocolError("message type out of range")
    if len(frame.payload) > MAX_PAYLOAD:
        raise ProtocolError("payload exceeds protocol maximum")
    header = HEADER.pack(MAGIC, VERSION, frame.message_type, frame.flags,
                         frame.sequence, frame.timestamp_ns,
                         len(frame.payload), 0)
    body = header + frame.payload
    return body + CRC.pack(crc32c(body))


CONFIG_SET = struct.Struct("<IIHBB")


def encode_config_set(*, frame_sequence: int, request_id: int,
                      sample_rate_hz: int, samples_per_block: int,
                      overflow_policy: int, telemetry_enabled: bool) -> bytes:
    if not 1 <= sample_rate_hz <= 50_000:
        raise ProtocolError("sample rate must be 1..50000 Hz")
    if not 1 <= samples_per_block <= 32:
        raise ProtocolError("samples per block must be 1..32")
    if overflow_policy not in (0, 1):
        raise ProtocolError("overflow policy must be 0 or 1")
    payload = CONFIG_SET.pack(request_id, sample_rate_hz, samples_per_block,
                              overflow_policy, int(telemetry_enabled))
    return encode_frame(Frame(16, 0, frame_sequence, 0, payload))


def decode_frame(candidate: bytes) -> Frame:
    if len(candidate) < HEADER.size + CRC.size:
        raise ProtocolError("short frame")
    magic, version, message_type, flags, sequence, timestamp_ns, length, reserved = (
        HEADER.unpack_from(candidate)
    )
    if magic != MAGIC:
        raise ProtocolError("bad synchronization word")
    if version != VERSION:
        raise ProtocolError(f"unsupported version {version}")
    if reserved != 0:
        raise ProtocolError("reserved header field is nonzero")
    if length > MAX_PAYLOAD:
        raise ProtocolError("payload exceeds protocol maximum")
    expected_size = HEADER.size + length + CRC.size
    if len(candidate) != expected_size:
        raise ProtocolError("frame length mismatch")
    expected_crc = CRC.unpack_from(candidate, expected_size - CRC.size)[0]
    if crc32c(candidate[:-CRC.size]) != expected_crc:
        raise ProtocolError("CRC32C mismatch")
    return Frame(message_type, flags, sequence, timestamp_ns,
                 candidate[HEADER.size:-CRC.size])


def decode_stream(data: bytes) -> Iterator[Frame]:
    """Resynchronize on magic and yield valid frames; corrupt candidates are skipped."""
    offset = 0
    while True:
        start = data.find(MAGIC, offset)
        if start < 0 or len(data) - start < HEADER.size:
            return
        length = struct.unpack_from("<H", data, start + 20)[0]
        total = HEADER.size + length + CRC.size
        if length > MAX_PAYLOAD or start + total > len(data):
            offset = start + 1
            continue
        candidate = data[start:start + total]
        try:
            yield decode_frame(candidate)
            offset = start + total
        except ProtocolError:
            offset = start + 1


SAMPLE_PREFIX = struct.Struct("<IIHH")


def decode_sample_block(frame: Frame) -> tuple[int, int, list[tuple[int, ...]]]:
    if frame.message_type != 1 or len(frame.payload) < SAMPLE_PREFIX.size:
        raise ProtocolError("not a sample block")
    first_sequence, period_ns, count, channels = SAMPLE_PREFIX.unpack_from(frame.payload)
    if count == 0 or channels == 0:
        raise ProtocolError("empty sample geometry")
    sample_data = frame.payload[SAMPLE_PREFIX.size:]
    expected = count * channels * 2
    if len(sample_data) != expected:
        raise ProtocolError("sample payload length mismatch")
    values = struct.unpack(f"<{count * channels}h", sample_data)
    rows = [values[i:i + channels] for i in range(0, len(values), channels)]
    return first_sequence, period_ns, rows
