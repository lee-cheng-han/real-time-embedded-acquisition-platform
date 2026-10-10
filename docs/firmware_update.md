# Firmware update design status

Planned image metadata contains magic, format version, image size, firmware semantic
version, monotonic security counter, SHA-256 digest, signing algorithm identifier,
key identifier, and signature. An established library such as MCUboot plus its vetted
crypto backend will perform signature verification; no custom cryptography is planned.

The boot state machine is EMPTY -> DOWNLOADING -> VERIFIED -> PENDING -> CONFIRMED.
Writes update redundant metadata records with generation and CRC after image chunks
are durable. Reset during download resumes or erases the incomplete slot. A pending
image must confirm after self-test; otherwise the bootloader selects the last confirmed
image when the physical flash layout supports it.

Nothing in this repository currently implements or validates secure boot, A/B update,
anti-rollback, or protected key storage. Anti-rollback requires a protected monotonic
store; signature checking alone does not provide it.
