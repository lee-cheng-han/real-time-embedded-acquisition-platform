#include "drats/crc32c.h"

uint32_t drats_crc32c(uint32_t seed, const void *data, size_t length) {
    const uint8_t *bytes = data;
    uint32_t crc = ~seed;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (UINT32_C(0x82F63B78) & mask);
        }
    }
    return ~crc;
}
