// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_CIRCULAR_BUFFER_H
#define UNI_CIRCULAR_BUFFER_H

#include <stdint.h>

// Fixed-capacity ring buffer used to queue outgoing L2CAP/HID packets (such as
// rumble, LED, and feature/output reports) across connected controllers while
// waiting for BTstack's asynchronous L2CAP_EVENT_CAN_SEND_NOW callback.

// UNI_CIRCULAR_BUFFER_SIZE represents the total number of slots in the ring
// buffer. Because one slot is reserved as a sentinel to distinguish the full
// state from the empty state without a separate counter, at most
// (UNI_CIRCULAR_BUFFER_SIZE - 1) packets can be queued simultaneously.
// Multiple gamepads could be connected at the same time, each queuing
// multiple packets: Think of 8 gamepads wanting to rumble at the same time.
#define UNI_CIRCULAR_BUFFER_SIZE 32
// UNI_CIRCULAR_BUFFER_DATA_SIZE represents the maximum payload size (in bytes)
// of each queued packet. Payloads up to and including 128 bytes are supported.
#define UNI_CIRCULAR_BUFFER_DATA_SIZE 128

enum {
    UNI_CIRCULAR_BUFFER_ERROR_OK = 0,
    UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL,
    UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY,
    UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG,
};

// Single queued packet entry associating an L2CAP channel ID (cid) with its payload.
typedef struct uni_circular_buffer_data_s {
    int16_t cid;
    uint8_t data[UNI_CIRCULAR_BUFFER_DATA_SIZE];
    int data_len;
} uni_circular_buffer_data_t;

// Ring buffer state. Empty when head_idx == tail_idx; full when advancing
// tail_idx by one slot (modulo UNI_CIRCULAR_BUFFER_SIZE) would equal head_idx.
typedef struct uni_circular_buffer_s {
    uni_circular_buffer_data_t buffer[UNI_CIRCULAR_BUFFER_SIZE];
    int16_t head_idx;
    int16_t tail_idx;
} uni_circular_buffer_t;

// Enqueues a packet of `len` bytes (`0 <= len <= UNI_CIRCULAR_BUFFER_DATA_SIZE`)
// associated with `cid` at the tail of the buffer.
// Returns UNI_CIRCULAR_BUFFER_ERROR_OK on success, UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL
// if no slots remain, or UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG on invalid arguments
// (`b == NULL`, `len < 0`, `len > UNI_CIRCULAR_BUFFER_DATA_SIZE`, or `data == NULL` when `len > 0`).
uint8_t uni_circular_buffer_put(uni_circular_buffer_t* b, int16_t cid, const void* data, int len);

// Dequeues the oldest packet from the head of the buffer into `*cid`, `*data`, and `*len`.
// `*data` is set to point directly to the internal slot buffer (valid until the slot is overwritten).
// Returns UNI_CIRCULAR_BUFFER_ERROR_OK on success, or UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY
// if the buffer is empty or any pointer argument is NULL.
uint8_t uni_circular_buffer_get(uni_circular_buffer_t* b, int16_t* cid, void** data, int* len);

// Returns non-zero (1) if the buffer is empty or `b == NULL`, and 0 otherwise.
uint8_t uni_circular_buffer_is_empty(const uni_circular_buffer_t* b);

// Returns non-zero (1) if the buffer has reached capacity (UNI_CIRCULAR_BUFFER_SIZE - 1 items),
// or 0 if slots are available or `b == NULL`.
uint8_t uni_circular_buffer_is_full(const uni_circular_buffer_t* b);

// Resets head and tail indices to 0, discarding any queued packets. Safe to call with NULL.
void uni_circular_buffer_reset(uni_circular_buffer_t* b);

#endif  // UNI_CIRCULAR_BUFFER_H
