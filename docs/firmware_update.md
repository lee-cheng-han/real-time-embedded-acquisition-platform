# Firmware update design status

Implemented image metadata contains magic, format version, image size, firmware semantic
version, monotonic security counter, SHA-256 digest, signing algorithm identifier,
key identifier, and signature. `host/drats/update_image.py` creates and verifies
ECDSA-P256/SHA-256 packages through OpenSSL. The signature authenticates the format,
size, semantic version, security counter, algorithm, key identifier, and image digest.
Integration tests reject payload tampering, metadata tampering, and rollback counters.
The development bootloader uses a pinned minimal Mbed TLS 3.6.7 build to hash the image,
reconstruct and hash the authenticated metadata, validate the embedded P-256 public
point, and verify the DER ECDSA signature. A native integration test compiles and runs
that same verifier against the signed factory package and requires rejection of image
tampering, signature tampering, and a higher minimum counter.

The boot state machine is EMPTY -> DOWNLOADING -> VERIFIED -> PENDING -> CONFIRMED.
Writes update redundant metadata records with generation and CRC after image chunks
are durable. Reset during download resumes or erases the incomplete slot. A pending
image must confirm after self-test; otherwise the bootloader selects the last confirmed
image when the physical flash layout supports it.

`common/src/boot_record.c` implements the redundant 32-byte record format and selection
policy. The commit word is written last. Host tests interrupt every prefix write and
every partial commit-word write, require selection of the older committed generation,
then exercise pending-attempt exhaustion, invalid-slot rollback, and generation wrap.

The build produces `drats_development_bootloader` at `0x08000000`. It reads signed
metadata from `0x081e0000`, authenticates application A at `0x08020000`, enforces a
minimum counter of one, validates stack/reset vectors, disables SysTick and NVIC state,
sets VTOR and MSP, and branches to the application. The build generates an ignored,
workspace-local development key and compiles only its generated public point into the
bootloader. This supplies development evidence, not a production root of trust.

Nothing in this repository currently validates boot on physical hardware, A/B update,
target flash programming, persistent anti-rollback, bootloader write protection, debug
lockdown, or protected production key handling. A fixed minimum counter is not hardware
anti-rollback; that requires a protected monotonic store.
