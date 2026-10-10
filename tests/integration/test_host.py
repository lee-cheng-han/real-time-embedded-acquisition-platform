#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "host"))

from drats.protocol import (  # noqa: E402
    CONFIG_ACK, CONFIG_SET, Frame, decode_config_ack, decode_sample_block,
    decode_stream, encode_frame, encode_config_set
)


def main() -> int:
    if len(sys.argv) != 2:
        return 2
    capture = subprocess.run([sys.argv[1], "10"], check=True,
                             stdout=subprocess.PIPE).stdout
    frames = list(decode_stream(b"garbage" + capture))
    assert len(frames) == 10
    assert [frame.sequence for frame in frames] == list(range(10))
    first, period_ns, rows = decode_sample_block(frames[0])
    assert first == 0
    assert period_ns == 100_000
    assert len(rows) == 32 and len(rows[0]) == 6
    corrupted = bytearray(capture)
    corrupted[30] ^= 1
    assert len(list(decode_stream(bytes(corrupted)))) == 9
    config = encode_config_set(frame_sequence=44, request_id=9,
                               sample_rate_hz=20_000, samples_per_block=16,
                               overflow_policy=1, telemetry_enabled=True)
    config_frame = list(decode_stream(config))[0]
    assert config_frame.message_type == 16 and config_frame.sequence == 44
    assert CONFIG_SET.unpack(config_frame.payload) == (9, 20_000, 16, 1, 1)
    ack_frame = Frame(17, 0, 45, 0,
                      CONFIG_ACK.pack(9, 0, 20_000, 16, 1, 1))
    decoded_ack = list(decode_stream(encode_frame(ack_frame)))[0]
    assert decode_config_ack(decoded_ack) == (9, 0, 20_000, 16, 1, True)
    rejected_ack = Frame(17, 0, 46, 0, CONFIG_ACK.pack(10, -1, 0, 0, 0, 0))
    decoded_rejection = list(decode_stream(encode_frame(rejected_ack)))[0]
    assert decode_config_ack(decoded_rejection) == (10, -1, 0, 0, 0, False)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
