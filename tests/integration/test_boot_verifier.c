#include "development_public_key.h"
#include "image_verify.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int fail(const char *message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return fail("usage: test_boot_verifier PACKAGE");
    }
    FILE *file = fopen(argv[1], "rb");
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0) {
        return fail("cannot open package");
    }
    const long length = ftell(file);
    if (length < (long)DRATS_IMAGE_METADATA_SIZE ||
        fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        return fail("invalid package length");
    }
    uint8_t *package = malloc((size_t)length);
    if (package == NULL ||
        fread(package, 1U, (size_t)length, file) != (size_t)length) {
        free(package);
        fclose(file);
        return fail("cannot read package");
    }
    fclose(file);

    drats_image_metadata_t *metadata = (drats_image_metadata_t *)package;
    uint8_t *image = package + DRATS_IMAGE_METADATA_SIZE;
    const size_t image_capacity = (size_t)length - DRATS_IMAGE_METADATA_SIZE;
    if (!drats_boot_verify_image(metadata, image, image_capacity, 1U,
                                 drats_development_public_key,
                                 drats_development_key_id)) {
        free(package);
        return fail("valid signed package was rejected");
    }

    image[0] ^= 1U;
    const bool accepted_modified_image = drats_boot_verify_image(
        metadata, image, image_capacity, 1U, drats_development_public_key,
        drats_development_key_id);
    image[0] ^= 1U;
    metadata->signature[0] ^= 1U;
    const bool accepted_modified_signature = drats_boot_verify_image(
        metadata, image, image_capacity, 1U, drats_development_public_key,
        drats_development_key_id);
    metadata->signature[0] ^= 1U;
    const bool accepted_rollback = drats_boot_verify_image(
        metadata, image, image_capacity, metadata->security_counter + 1U,
        drats_development_public_key, drats_development_key_id);

    free(package);
    if (accepted_modified_image || accepted_modified_signature ||
        accepted_rollback) {
        return fail("negative authenticated-boot test was accepted");
    }
    puts("embedded verifier accepts the signed image and rejects tampering/rollback");
    return 0;
}
