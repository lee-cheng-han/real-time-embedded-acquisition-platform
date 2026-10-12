#ifndef DRATS_BOOT_IMAGE_VERIFY_H
#define DRATS_BOOT_IMAGE_VERIFY_H

#include "drats/image.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool drats_boot_verify_image(const drats_image_metadata_t *metadata,
                             const uint8_t *image,
                             size_t slot_capacity,
                             uint32_t minimum_security_counter,
                             const uint8_t public_key[65],
                             const uint8_t expected_key_id[8]);

#endif
