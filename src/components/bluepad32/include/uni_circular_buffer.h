// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_CIRCULAR_BUFFER_H
#define UNI_CIRCULAR_BUFFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file uni_circular_buffer.h
 * @brief Pre-allocated variable-length byte-stream ring buffer for queuing L2CAP/HID packets.
 *
 * Queues outgoing L2CAP/HID packets (such as rumble, LED, and feature/output reports)
 * across connected controllers while waiting for BTstack's asynchronous
 * L2CAP_EVENT_CAN_SEND_NOW callback.
 */

/**
 * @brief Circular buffer total byte capacity.
 *
 * UNI_CIRCULAR_BUFFER_SIZE represents the total number of bytes in the ring
 * buffer. Each queued packet occupies (UNI_CIRCULAR_BUFFER_HEADER_SIZE + len)
 * bytes. Because one byte is reserved as a sentinel to distinguish the full
 * state from the empty state (head_idx == tail_idx) without a separate counter,
 * at most (UNI_CIRCULAR_BUFFER_SIZE - 1) bytes (4,095 bytes) can be used.
 */
#define UNI_CIRCULAR_BUFFER_SIZE 4096

/**
 * @brief Per-packet header size in bytes inside the byte-stream ring buffer.
 *
 * Each queued packet prefixes its variable-length payload with a 4-byte header
 * consisting of int16_t cid (2 bytes) and int16_t data_len (2 bytes).
 */
#define UNI_CIRCULAR_BUFFER_HEADER_SIZE 4

/**
 * @brief Circular buffer error codes.
 */
enum {
    UNI_CIRCULAR_BUFFER_ERROR_OK = 0,         /**< Operation succeeded. */
    UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL,    /**< Insufficient free bytes in ring buffer for header + payload. */
    UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY,   /**< Buffer is empty, NULL output argument, or corrupted header reset. */
    UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG, /**< Invalid argument (NULL buffer/data, len < 0) or packet exceeds ring
                                                 capacity. */
};

/**
 * @brief Pre-allocated variable-length byte-stream ring buffer state.
 *
 * `buffer` stores packed `[cid (2B), data_len (2B), payload (len B)]` records
 * that wrap seamlessly across the end of the array.
 *
 * Empty when `head_idx == tail_idx`; full when fewer than
 * `UNI_CIRCULAR_BUFFER_HEADER_SIZE` free bytes remain (with 1 byte reserved
 * as a sentinel).
 */
typedef struct uni_circular_buffer_s {
    uint8_t buffer[UNI_CIRCULAR_BUFFER_SIZE]; /**< Raw byte-stream ring buffer storing header + payload records. */
    int16_t head_idx; /**< Read byte index into `buffer` (`0 .. UNI_CIRCULAR_BUFFER_SIZE - 1`). */
    int16_t tail_idx; /**< Write byte index into `buffer` (`0 .. UNI_CIRCULAR_BUFFER_SIZE - 1`). */
} uni_circular_buffer_t;

/**
 * @brief Enqueues a packet of `len` bytes associated with `cid` at the tail of the buffer.
 *
 * @param b The circular buffer instance.
 * @param cid The L2CAP channel ID.
 * @param data Pointer to the payload data (may be NULL only when `len == 0`).
 * @param len The size of the payload in bytes.
 * @return UNI_CIRCULAR_BUFFER_ERROR_OK on success, UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL
 *         if fewer than `UNI_CIRCULAR_BUFFER_HEADER_SIZE + len` free bytes remain, or
 *         UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG on invalid arguments or if
 *         `UNI_CIRCULAR_BUFFER_HEADER_SIZE + len > UNI_CIRCULAR_BUFFER_SIZE - 1`.
 */
uint8_t uni_circular_buffer_put(uni_circular_buffer_t* b, int16_t cid, const void* data, int len);

/**
 * @brief Dequeues the oldest packet from the head of the buffer into caller-provided `data`.
 *
 * @param b The circular buffer instance.
 * @param cid Pointer to store the dequeued L2CAP channel ID.
 * @param data Caller-allocated destination buffer to receive the dequeued payload bytes.
 * @param len Pointer to store the dequeued payload size in bytes.
 * @return UNI_CIRCULAR_BUFFER_ERROR_OK on success, or UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY
 *         if the buffer is empty or any pointer argument is NULL.
 */
uint8_t uni_circular_buffer_get(uni_circular_buffer_t* b, int16_t* cid, void* data, int* len);

/**
 * @brief Returns whether the buffer is empty.
 *
 * @param b The circular buffer instance.
 * @return 1 if the buffer is empty or `b == NULL`, 0 otherwise.
 */
uint8_t uni_circular_buffer_is_empty(const uni_circular_buffer_t* b);

/**
 * @brief Returns whether the buffer is full.
 *
 * @param b The circular buffer instance.
 * @return 1 if fewer than `UNI_CIRCULAR_BUFFER_HEADER_SIZE` (4) free bytes remain,
 *         or 0 if at least 4 free bytes are available or `b == NULL`.
 */
uint8_t uni_circular_buffer_is_full(const uni_circular_buffer_t* b);

/**
 * @brief Resets head and tail indices to 0, discarding any queued packets. Safe to call with NULL.
 *
 * @param b The circular buffer instance.
 */
void uni_circular_buffer_reset(uni_circular_buffer_t* b);

#ifdef __cplusplus
}
#endif

#endif  // UNI_CIRCULAR_BUFFER_H
