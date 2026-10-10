#ifndef DRATS_STREAM_DECODER_H
#define DRATS_STREAM_DECODER_H

#include "drats/protocol.h"

#include <stddef.h>
#include <stdint.h>

typedef void (*drats_frame_callback_t)(const drats_frame_header_t *header,
                                       const uint8_t *payload,
                                       void *context);

typedef struct {
    uint8_t storage[DRATS_PROTOCOL_MAX_FRAME];
    size_t length;
    uint32_t frames_decoded;
    uint32_t corrupt_candidates;
    uint32_t bytes_discarded;
} drats_stream_decoder_t;

void drats_stream_decoder_init(drats_stream_decoder_t *decoder);
void drats_stream_decoder_feed(drats_stream_decoder_t *decoder,
                               const uint8_t *data,
                               size_t length,
                               drats_frame_callback_t callback,
                               void *callback_context);

#endif
