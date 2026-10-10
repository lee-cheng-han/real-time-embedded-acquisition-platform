#include "drats/stream_decoder.h"

#include <stdbool.h>
#include <string.h>

static const uint8_t magic[4] = {'D', 'R', 'A', 'T'};

static uint16_t get_u16(const uint8_t *value) {
    return (uint16_t)((uint16_t)value[0] | ((uint16_t)value[1] << 8U));
}

static void discard_prefix(drats_stream_decoder_t *decoder, size_t count) {
    if (count < decoder->length) {
        memmove(decoder->storage, &decoder->storage[count],
                decoder->length - count);
    }
    decoder->length -= count;
    decoder->bytes_discarded += (uint32_t)count;
}

void drats_stream_decoder_init(drats_stream_decoder_t *decoder) {
    if (decoder != NULL) {
        memset(decoder, 0, sizeof(*decoder));
    }
}

void drats_stream_decoder_feed(drats_stream_decoder_t *decoder,
                               const uint8_t *data,
                               size_t length,
                               drats_frame_callback_t callback,
                               void *callback_context) {
    if ((decoder == NULL) || ((length != 0U) && (data == NULL))) {
        return;
    }
    for (size_t input = 0; input < length; ++input) {
        if (decoder->length == sizeof(decoder->storage)) {
            discard_prefix(decoder, 1U);
            ++decoder->corrupt_candidates;
        }
        decoder->storage[decoder->length++] = data[input];

        bool progress = true;
        while (progress) {
            progress = false;
            while ((decoder->length >= sizeof(magic)) &&
                   (memcmp(decoder->storage, magic, sizeof(magic)) != 0)) {
                discard_prefix(decoder, 1U);
                progress = true;
            }
            if (decoder->length < DRATS_PROTOCOL_HEADER_SIZE) {
                continue;
            }
            const uint16_t payload_length = get_u16(&decoder->storage[20]);
            if (payload_length > DRATS_PROTOCOL_MAX_PAYLOAD) {
                discard_prefix(decoder, 1U);
                ++decoder->corrupt_candidates;
                progress = true;
                continue;
            }
            const size_t frame_length = DRATS_PROTOCOL_HEADER_SIZE +
                                        payload_length + DRATS_PROTOCOL_CRC_SIZE;
            if (decoder->length < frame_length) {
                continue;
            }
            drats_frame_header_t header;
            const uint8_t *payload = NULL;
            if (drats_frame_decode(decoder->storage, frame_length,
                                   &header, &payload) == DRATS_OK) {
                if (callback != NULL) {
                    callback(&header, payload, callback_context);
                }
                ++decoder->frames_decoded;
                if (frame_length < decoder->length) {
                    memmove(decoder->storage, &decoder->storage[frame_length],
                            decoder->length - frame_length);
                }
                decoder->length -= frame_length;
            } else {
                discard_prefix(decoder, 1U);
                ++decoder->corrupt_candidates;
            }
            progress = true;
        }
    }
}
