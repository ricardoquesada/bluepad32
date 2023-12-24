// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_circular_buffer.h"

#include <string.h>

#include "uni_log.h"

uint8_t uni_circular_buffer_put(uni_circular_buffer_t* b, int16_t cid, const void* data, int len) {
    if (uni_circular_buffer_is_full(b)) {
        return UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL;
    }
    if (len >= UNI_CIRCULAR_BUFFER_DATA_SIZE) {
        return UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG;
    }
    memcpy(&b->buffer[b->tail_idx].data, data, len);
    b->buffer[b->tail_idx].data_len = len;
    b->buffer[b->tail_idx].cid = cid;

    b->tail_idx++;
    if (b->tail_idx == UNI_CIRCULAR_BUFFER_SIZE)
        b->tail_idx = 0;
    return UNI_CIRCULAR_BUFFER_ERROR_OK;
}

uint8_t uni_circular_buffer_get(uni_circular_buffer_t* b, int16_t* cid, void** data, int* len) {
    if (uni_circular_buffer_is_empty(b)) {
        return UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY;
    }
    *data = &b->buffer[b->head_idx].data;
    *len = b->buffer[b->head_idx].data_len;
    *cid = b->buffer[b->head_idx].cid;

    b->head_idx++;
    if (b->head_idx == UNI_CIRCULAR_BUFFER_SIZE)
        b->head_idx = 0;
    return UNI_CIRCULAR_BUFFER_ERROR_OK;
}

uint8_t uni_circular_buffer_is_empty(uni_circular_buffer_t* b) {
    return (b->head_idx == b->tail_idx);
}

uint8_t uni_circular_buffer_is_full(uni_circular_buffer_t* b) {
    return (b->tail_idx + 1 == b->head_idx) || (b->head_idx == 0 && b->tail_idx == UNI_CIRCULAR_BUFFER_SIZE - 1);
}

void uni_circular_buffer_reset(uni_circular_buffer_t* b) {
    b->head_idx = b->tail_idx = 0;
}
