# Firmware update design status

Implemented image metadata contains magic, format version, image size, firmware semantic
version, monotonic security counter, SHA-256 digest, signing algorithm identifier,
key identifier, and signature. `host/drats/update_image.py` creates and verifies
ECDSA-P256/SHA-256 packages through OpenSSL. The signature authenticates the format,
size, semantic version, security counter, algorithm, key identifier, and image digest.
Integration tests reject payload tampering, metadata tampering, and rollback counters.
Target-side verification still requires an established embedded library such as Mbed
TLS; no custom cryptography is used or planned.

The boot state machine is EMPTY -> DOWNLOADING -> VERIFIED -> PENDING -> CONFIRMED.
Writes update redundant metadata records with generation and CRC after image chunks
are durable. Reset during download resumes or erases the incomplete slot. A pending
image must confirm after self-test; otherwise the bootloader selects the last confirmed
image when the physical flash layout supports it.

`common/src/boot_record.c` implements the redundant 32-byte record format and selection
policy. The commit word is written last. Host tests interrupt every prefix write and
every partial commit-word write, require selection of the older committed generation,
then exercise pending-attempt exhaustion, invalid-slot rollback, and generation wrap.

The build also produces `drats_development_bootloader` at `0x08000000`. It validates
slot A's stack/reset vectors, disables SysTick and NVIC state, sets VTOR and MSP, and
branches to the application at `0x08020000`. It intentionally does not authenticate
the application and is only a board-bring-up bridge. It must be extended with the
signed-image verifier before any secure-boot claim.

Nothing in this repository currently validates secure boot, A/B update, target flash
programming, anti-rollback, or protected key storage. Host-enforced counter policy is
not hardware anti-rollback; that requires a protected monotonic store.
