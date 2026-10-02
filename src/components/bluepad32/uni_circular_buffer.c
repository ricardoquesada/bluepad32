// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

/**
 * @file uni_circular_buffer.c
 * @brief Variable-length byte-stream ring buffer implementation for outgoing L2CAP/HID packets.
 *
 * Replaces fixed-size 128-byte packet slots with a packed byte-stream ring buffer
 * (`UNI_CIRCULAR_BUFFER_SIZE` = 4,096 bytes) so bursts of small HID output reports
 * (such as 11-byte Nintendo Switch rumble packets streamed during 60-120 Hz IMU traffic)
 * do not exhaust the queue due to internal slot fragmentation.
 *
 * Key architectural invariants:
 * - Each queued packet is serialized as a 4-byte header (`uni_circular_buffer_header_t`:
 *   `int16_t cid`, `int16_t data_len`) followed immediately by `data_len` payload bytes,
 *   wrapping seamlessly across index `UNI_CIRCULAR_BUFFER_SIZE - 1 -> 0`.
 * - Headers and payloads are copied via `memcpy` rather than direct struct pointer casts
 *   into `b->buffer[]`, preventing unaligned memory access faults on strict-alignment
 *   architectures (e.g., ARM Cortex-M0+ / RP2040).
 * - Dequeued payloads are copied directly into the caller-supplied `void* data` buffer,
 *   reassembling split payloads across the ring boundary without requiring a duplicate
 *   staging buffer inside `uni_circular_buffer_t`.
 */

#include "uni_circular_buffer.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Maximum single-packet payload bytes that can fit into the ring buffer.
 *
 * One byte of `UNI_CIRCULAR_BUFFER_SIZE` is reserved as the empty/full sentinel,
 * and `UNI_CIRCULAR_BUFFER_HEADER_SIZE` (4 bytes) is occupied by the packet header.
 */
#define UNI_CIRCULAR_BUFFER_MAX_DATA_SIZE (UNI_CIRCULAR_BUFFER_SIZE - 1 - UNI_CIRCULAR_BUFFER_HEADER_SIZE)

/**
 * @brief Internal 4-byte packet header serialized before each variable-length payload in `b->buffer`.
 *
 * Kept private to the translation unit so `_Static_assert` checks remain strictly in C11 code
 * while `uni_circular_buffer.h` stays compatible with strict ISO C++23 (`-Wpedantic -Werror`).
 */
typedef struct uni_circular_buffer_header_s {
    int16_t cid;      /**< Destination L2CAP channel ID. */
    int16_t data_len; /**< Payload length in bytes (`0 <= data_len <= UNI_CIRCULAR_BUFFER_MAX_DATA_SIZE`). */
} uni_circular_buffer_header_t;

_Static_assert(sizeof(uni_circular_buffer_header_t) == UNI_CIRCULAR_BUFFER_HEADER_SIZE,
               "uni_circular_buffer_header_t must match UNI_CIRCULAR_BUFFER_HEADER_SIZE");
_Static_assert(UNI_CIRCULAR_BUFFER_SIZE > UNI_CIRCULAR_BUFFER_HEADER_SIZE && UNI_CIRCULAR_BUFFER_SIZE <= INT16_MAX,
               "UNI_CIRCULAR_BUFFER_SIZE must fit in positive int16_t indices");

/**
 * @brief Returns the number of bytes currently occupied in the ring buffer (`0 .. UNI_CIRCULAR_BUFFER_SIZE - 1`).
 */
static int uni_circular_buffer_used_bytes(const uni_circular_buffer_t* b) {
    int used = b->tail_idx - b->head_idx;
    if (used < 0) {
        used += UNI_CIRCULAR_BUFFER_SIZE;
    }
    return used;
}

/**
 * @brief Returns the number of writable bytes available in the ring buffer (`0 .. UNI_CIRCULAR_BUFFER_SIZE - 1`).
 */
static int uni_circular_buffer_free_bytes(const uni_circular_buffer_t* b) {
    // One byte is kept open as a sentinel to distinguish full from empty (head_idx == tail_idx).
    return (UNI_CIRCULAR_BUFFER_SIZE - 1) - uni_circular_buffer_used_bytes(b);
}

/**
 * @brief Writes `len` bytes (`len > 0`) from `src` into `b->buffer` at `b->tail_idx`,
 * splitting across the `UNI_CIRCULAR_BUFFER_SIZE` wrap-around boundary if necessary.
 */
static void uni_circular_buffer_write_bytes(uni_circular_buffer_t* b, const uint8_t* src, int len) {
    int first_chunk = UNI_CIRCULAR_BUFFER_SIZE - b->tail_idx;
    if (first_chunk > len) {
        first_chunk = len;
    }
    memcpy(&b->buffer[b->tail_idx], src, (size_t)first_chunk);

    // Only copy a second chunk when the write wraps past the end of `b->buffer`.
    int second_chunk = len - first_chunk;
    if (second_chunk > 0) {
        memcpy(&b->buffer[0], src + first_chunk, (size_t)second_chunk);
    }

    b->tail_idx = (int16_t)((b->tail_idx + len) % UNI_CIRCULAR_BUFFER_SIZE);
}

/**
 * @brief Reads `len` bytes (`len > 0`) from `b->buffer` at `b->head_idx` into `dst`,
 * reassembling across the `UNI_CIRCULAR_BUFFER_SIZE` wrap-around boundary if split.
 */
static void uni_circular_buffer_read_bytes(uni_circular_buffer_t* b, uint8_t* dst, int len) {
    int first_chunk = UNI_CIRCULAR_BUFFER_SIZE - b->head_idx;
    if (first_chunk > len) {
        first_chunk = len;
    }
    memcpy(dst, &b->buffer[b->head_idx], (size_t)first_chunk);

    // Only copy a second chunk when the read wraps past the end of `b->buffer`.
    int second_chunk = len - first_chunk;
    if (second_chunk > 0) {
        memcpy(dst + first_chunk, &b->buffer[0], (size_t)second_chunk);
    }

    b->head_idx = (int16_t)((b->head_idx + len) % UNI_CIRCULAR_BUFFER_SIZE);
}

uint8_t uni_circular_buffer_put(uni_circular_buffer_t* b, int16_t cid, const void* data, int len) {
    // Reject NULL buffer, negative lengths (which would wrap to huge size_t in memcpy),
    // and NULL payload pointers when a non-zero length is requested.
    if (!b || (!data && len > 0) || len < 0) {
        return UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG;
    }
    // Check total ring capacity bound before free-space check so packets that can never fit
    // in the ring buffer deterministically return ERROR_BUFFER_TOO_BIG even when full, and
    // `UNI_CIRCULAR_BUFFER_HEADER_SIZE + len` cannot overflow signed `int`.
    if (len > UNI_CIRCULAR_BUFFER_MAX_DATA_SIZE) {
        return UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG;
    }
    const int required_bytes = UNI_CIRCULAR_BUFFER_HEADER_SIZE + len;
    if (uni_circular_buffer_free_bytes(b) < required_bytes) {
        return UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL;
    }

    const uni_circular_buffer_header_t header = {
        .cid = cid,
        .data_len = (int16_t)len,
    };
    uni_circular_buffer_write_bytes(b, (const uint8_t*)&header, UNI_CIRCULAR_BUFFER_HEADER_SIZE);
    // Only invoke write_bytes when len > 0 so zero-length packets with data == NULL
    // never pass a NULL pointer to memcpy (which is undefined behavior under C11 / UBSan).
    if (len > 0) {
        uni_circular_buffer_write_bytes(b, (const uint8_t*)data, len);
    }
    return UNI_CIRCULAR_BUFFER_ERROR_OK;
}

uint8_t uni_circular_buffer_get(uni_circular_buffer_t* b, int16_t* cid, void* data, int* len) {
    // Guard against NULL buffer or NULL output pointers before dereferencing or advancing head_idx.
    if (!b || !cid || !data || !len) {
        return UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY;
    }
    if (uni_circular_buffer_is_empty(b)) {
        return UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY;
    }
    if (uni_circular_buffer_used_bytes(b) < UNI_CIRCULAR_BUFFER_HEADER_SIZE) {
        // Self-heal if external memory/index corruption leaves 1..3 orphan bytes in the ring.
        uni_circular_buffer_reset(b);
        return UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY;
    }

    uni_circular_buffer_header_t header = {0};
    uni_circular_buffer_read_bytes(b, (uint8_t*)&header, UNI_CIRCULAR_BUFFER_HEADER_SIZE);
    // Validate the deserialized header before copying into `data`; if corrupted or truncated,
    // reset the ring buffer to prevent out-of-bounds reads or infinite dequeue loops.
    if (header.data_len < 0 || header.data_len > UNI_CIRCULAR_BUFFER_MAX_DATA_SIZE ||
        uni_circular_buffer_used_bytes(b) < header.data_len) {
        uni_circular_buffer_reset(b);
        return UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY;
    }

    if (header.data_len > 0) {
        uni_circular_buffer_read_bytes(b, (uint8_t*)data, header.data_len);
    }
    *len = header.data_len;
    *cid = header.cid;
    return UNI_CIRCULAR_BUFFER_ERROR_OK;
}

uint8_t uni_circular_buffer_is_empty(const uni_circular_buffer_t* b) {
    // Treat a NULL buffer as empty so callers do not attempt to dequeue from it.
    if (!b)
        return 1;
    return (b->head_idx == b->tail_idx);
}

uint8_t uni_circular_buffer_is_full(const uni_circular_buffer_t* b) {
    if (!b)
        return 0;
    // The buffer cannot accept any additional packet (even a 0-byte payload)
    // when fewer than UNI_CIRCULAR_BUFFER_HEADER_SIZE (4) free bytes remain.
    return uni_circular_buffer_free_bytes(b) < UNI_CIRCULAR_BUFFER_HEADER_SIZE;
}

void uni_circular_buffer_reset(uni_circular_buffer_t* b) {
    if (!b)
        return;
    b->head_idx = b->tail_idx = 0;
}
