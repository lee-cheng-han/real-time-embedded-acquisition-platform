# Telemetry protocol version 1

Every integer is little-endian. A stream receiver scans for ASCII `DRAT`, reads the
payload length, bounds-checks it, and accepts the frame only when CRC32C validates.

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | synchronization word `DRAT` |
| 4 | 1 | protocol version (`1`) |
| 5 | 1 | message type |
| 6 | 2 | flags |
| 8 | 4 | frame sequence |
| 12 | 8 | device monotonic timestamp in ns |
| 20 | 2 | payload length, 0–1024 |
| 22 | 2 | reserved, zero |
| 24 | N | payload |
| 24+N | 4 | CRC32C of header and payload |

Message types are sample block (`1`), health (`2`), configuration set (`16`),
configuration acknowledgement (`17`), and error (`127`). Unknown types are retained
for forward compatibility. A version mismatch is rejected.

## Sample block payload

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | first sample sequence |
| 4 | 4 | nominal sample period in ns |
| 8 | 2 | sample count |
| 10 | 2 | channel count |
| 12 | `count * channels * 2` | interleaved signed 16-bit samples |

Frame sequence gaps detect missing frames. Sample sequence gaps independently detect
loss before framing. Device timestamps are not compared directly with host time until
a clock relationship has been measured.

## Configuration-set payload

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | request identifier |
| 4 | 4 | requested sample rate in Hz |
| 8 | 2 | samples per block |
| 10 | 1 | overflow policy: stop (`0`) or drop-newest (`1`) |
| 11 | 1 | telemetry enabled (`0` or `1`) |

## Health payload

The device emits message type `2` approximately once per second. Its ten little-endian
`uint32` fields are uptime in milliseconds, RCC reset flags, acquisition state, blocks
completed, samples completed, dropped blocks, queue overflows, DMA errors, DMA
timeouts, and sequence gaps. Counters saturate at `UINT32_MAX` on the wire. Use
`drats_cli.py health <capture>` to print the most recent report.

The device applies valid configuration between acquisition blocks and responds only
after the acquisition task has stopped, reconfigured, and restarted the engine. A
nonzero acknowledgement status carries zeroed configuration fields. The Python client
supports both offline frame generation with `make-config` and live serial operation
with `configure`.

## Configuration acknowledgement payload

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | request identifier |
| 4 | 4 | signed status code |
| 8 | 4 | applied sample rate in Hz |
| 12 | 2 | applied samples per block |
| 14 | 1 | applied overflow policy |
| 15 | 1 | telemetry enabled |

## Compatibility rules

Version 1 decoders reject nonzero reserved fields, impossible lengths, bad CRC, and
unsupported versions. They may skip unknown message types. New fields require a new
version unless they fit a message-specific payload format with its own length/version.
