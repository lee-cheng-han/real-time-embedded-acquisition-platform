#include "drats/protocol.h"
#include "test_support.h"

#include <stdint.h>

static uint32_t next_random(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    *state = value;
    return value;
}

int main(void) {
    uint32_t random_state = UINT32_C(0xD4A75EED);
    uint8_t input[DRATS_PROTOCOL_MAX_FRAME + 16U];
    for (size_t iteration = 0; iteration < 20000U; ++iteration) {
        const size_t length = next_random(&random_state) % sizeof(input);
        for (size_t i = 0; i < length; ++i) {
            input[i] = (uint8_t)next_random(&random_state);
        }
        drats_frame_header_t header;
        const uint8_t *payload = NULL;
        const drats_status_t status = drats_frame_decode(
            input, length, &header, &payload);
        CHECK((status >= DRATS_E_AUTH) && (status <= DRATS_OK));
        if (status == DRATS_OK) {
            CHECK(payload >= input);
            CHECK(payload < input + length);
        }
    }
    return 0;
}
