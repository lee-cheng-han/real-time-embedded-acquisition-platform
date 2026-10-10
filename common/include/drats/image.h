#ifndef DRATS_IMAGE_H
#define DRATS_IMAGE_H

#include "drats/status.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DRATS_IMAGE_MAGIC UINT32_C(0x474D4944)
#define DRATS_IMAGE_FORMAT_VERSION 1U
#define DRATS_IMAGE_DIGEST_SIZE 32U
#define DRATS_IMAGE_SIGNATURE_MAX 72U

typedef enum {
    DRATS_SIGNATURE_ECDSA_P256_SHA256 = 1
} drats_signature_algorithm_t;

typedef struct {
    uint32_t magic;
    uint16_t format_version;
    uint16_t header_size;
    uint32_t image_size;
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    uint32_t security_counter;
    uint16_t signature_algorithm;
    uint16_t signature_length;
    uint8_t key_id[8];
    uint8_t digest[DRATS_IMAGE_DIGEST_SIZE];
    uint8_t signature[DRATS_IMAGE_SIGNATURE_MAX];
} drats_image_metadata_t;

typedef bool (*drats_signature_verifier_t)(
    const drats_image_metadata_t *metadata,
    void *context);

drats_status_t drats_image_metadata_validate(
    const drats_image_metadata_t *metadata,
    size_t slot_capacity,
    uint32_t minimum_security_counter,
    drats_signature_verifier_t verifier,
    void *verifier_context);

#endif
