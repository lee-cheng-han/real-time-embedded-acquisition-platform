# Proposed memory map

The STM32H563ZI has 2 MiB dual-bank internal flash. This preliminary layout reserves
128 KiB for an immutable-style boot/update manager, two 768 KiB application slots,
256 KiB scratch/download space, and 128 KiB for metadata/settings/diagnostics.

| Region | Offset | Size |
|---|---:|---:|
| bootloader | `0x000000` | 128 KiB |
| application A | `0x020000` | 768 KiB |
| application B | `0x0E0000` | 768 KiB |
| update scratch | `0x1A0000` | 256 KiB |
| metadata/settings/diagnostics | `0x1E0000` | 128 KiB |

The bootloader and application-A origins and sizes are enforced by linker scripts and
machine-checked after each target build. The factory image stores application A's
144-byte signed metadata at the beginning of the metadata region. Application B,
scratch, and the rest of the metadata region remain a feasibility layout. Bank/sector
boundaries, swap
behavior, read-while-write guarantees, secure aliases, option bytes, erase alignment,
and boot addresses must be reconciled with RM0481 and verified on the exact ZIT6 part.
If safe A/B placement cannot be proven, the fallback is a small protected bootloader
plus external QSPI staging or host-assisted recovery; the README will not claim A/B.
