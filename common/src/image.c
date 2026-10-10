#include "drats/image.h"

_Static_assert(sizeof(drats_image_metadata_t) == DRATS_IMAGE_METADATA_SIZE,
               "image metadata wire layout changed");

drats_status_t drats_image_metadata_validate(
    const drats_image_metadata_t *metadata,
    size_t slot_capacity,
    uint32_t minimum_security_counter,
    drats_signature_verifier_t verifier,
    void *verifier_context) {
    if ((metadata == NULL) || (verifier == NULL)) {
        return DRATS_E_ARGUMENT;
    }
    if ((metadata->magic != DRATS_IMAGE_MAGIC) ||
        (metadata->format_version != DRATS_IMAGE_FORMAT_VERSION) ||
        (metadata->header_size != sizeof(*metadata)) ||
        (metadata->signature_algorithm != DRATS_SIGNATURE_ECDSA_P256_SHA256)) {
        return DRATS_E_FORMAT;
    }
    if ((metadata->image_size == 0U) ||
        (metadata->image_size > slot_capacity) ||
        (metadata->signature_length == 0U) ||
        (metadata->signature_length > DRATS_IMAGE_SIGNATURE_MAX)) {
        return DRATS_E_SIZE;
    }
    if (metadata->security_counter < minimum_security_counter) {
        return DRATS_E_VERSION;
    }
    if (!verifier(metadata, verifier_context)) {
        return DRATS_E_AUTH;
    }
    return DRATS_OK;
}
