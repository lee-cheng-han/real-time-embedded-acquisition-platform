#include "image_verify.h"

#include "mbedtls/ecdsa.h"
#include "mbedtls/sha256.h"

#include <string.h>

typedef struct {
    const uint8_t *image;
    const uint8_t *public_key;
    const uint8_t *expected_key_id;
} verifier_context_t;

static void put_u16(uint8_t *destination, uint16_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *destination, uint32_t value) {
    for (unsigned i = 0U; i < 4U; ++i) {
        destination[i] = (uint8_t)(value >> (8U * i));
    }
}

static bool bytes_equal(const uint8_t *left, const uint8_t *right,
                        size_t length) {
    uint8_t difference = 0U;
    for (size_t i = 0U; i < length; ++i) {
        difference |= left[i] ^ right[i];
    }
    return difference == 0U;
}

static int sha256(const uint8_t *data, size_t length, uint8_t output[32]) {
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    int result = mbedtls_sha256_starts(&context, 0);
    if (result == 0) {
        result = mbedtls_sha256_update(&context, data, length);
    }
    if (result == 0) {
        result = mbedtls_sha256_finish(&context, output);
    }
    mbedtls_sha256_free(&context);
    return result;
}

static bool verify_signature(const drats_image_metadata_t *metadata,
                             void *opaque_context) {
    const verifier_context_t *context = opaque_context;
    uint8_t digest[32];
    if ((sha256(context->image, metadata->image_size, digest) != 0) ||
        !bytes_equal(digest, metadata->digest, sizeof(digest)) ||
        !bytes_equal(metadata->key_id, context->expected_key_id,
                     sizeof(metadata->key_id))) {
        return false;
    }

    uint8_t authenticated[70];
    put_u32(&authenticated[0], metadata->magic);
    put_u16(&authenticated[4], metadata->format_version);
    put_u16(&authenticated[6], metadata->header_size);
    put_u32(&authenticated[8], metadata->image_size);
    put_u32(&authenticated[12], metadata->version_major);
    put_u32(&authenticated[16], metadata->version_minor);
    put_u32(&authenticated[20], metadata->version_patch);
    put_u32(&authenticated[24], metadata->security_counter);
    put_u16(&authenticated[28], metadata->signature_algorithm);
    memcpy(&authenticated[30], metadata->key_id, sizeof(metadata->key_id));
    memcpy(&authenticated[38], metadata->digest, sizeof(metadata->digest));
    if (sha256(authenticated, sizeof(authenticated), digest) != 0) {
        return false;
    }

    mbedtls_ecdsa_context ecdsa;
    mbedtls_ecdsa_init(&ecdsa);
    int result = mbedtls_ecp_group_load(
        &ecdsa.MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256R1);
    if (result == 0) {
        result = mbedtls_ecp_point_read_binary(
            &ecdsa.MBEDTLS_PRIVATE(grp), &ecdsa.MBEDTLS_PRIVATE(Q),
            context->public_key, 65U);
    }
    if (result == 0) {
        result = mbedtls_ecp_check_pubkey(
            &ecdsa.MBEDTLS_PRIVATE(grp), &ecdsa.MBEDTLS_PRIVATE(Q));
    }
    if (result == 0) {
        result = mbedtls_ecdsa_read_signature(
            &ecdsa, digest, sizeof(digest), metadata->signature,
            metadata->signature_length);
    }
    mbedtls_ecdsa_free(&ecdsa);
    return result == 0;
}

bool drats_boot_verify_image(const drats_image_metadata_t *metadata,
                             const uint8_t *image,
                             size_t slot_capacity,
                             uint32_t minimum_security_counter,
                             const uint8_t public_key[65],
                             const uint8_t expected_key_id[8]) {
    if ((metadata == NULL) || (image == NULL) || (public_key == NULL) ||
        (expected_key_id == NULL)) {
        return false;
    }
    verifier_context_t context = {
        .image = image,
        .public_key = public_key,
        .expected_key_id = expected_key_id
    };
    return drats_image_metadata_validate(
        metadata, slot_capacity, minimum_security_counter,
        verify_signature, &context) == DRATS_OK;
}
