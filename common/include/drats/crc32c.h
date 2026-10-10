#ifndef DRATS_CRC32C_H
#define DRATS_CRC32C_H

#include <stddef.h>
#include <stdint.h>

uint32_t drats_crc32c(uint32_t seed, const void *data, size_t length);

#endif
