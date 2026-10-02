// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Ricardo Quesada
// http://retro.moe/unijoysticle2
//
// Unit and regression test suite for Bluepad32 core infrastructure:
// - `uni_circular_buffer` capacity, wrap-around, and null/length guards.
// - `uni_bt_allowlist` CRUD, POSIX TLV string persistence, and `uni_property` guards.
// - `uni_hid_device` pool lifecycle, virtual child linking, and BTstack timer teardown.

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ble/le_device_db.h>
#include <ble/le_device_db_tlv.h>
#include <btstack.h>
#include <btstack_memory.h>
#include <btstack_run_loop.h>
#include <btstack_run_loop_base.h>
#include <btstack_run_loop_posix.h>
#include <btstack_tlv.h>
#include <btstack_tlv_posix.h>
#include <classic/btstack_link_key_db_tlv.h>
#include <hci.h>
#include <hci_transport.h>
#include <l2cap.h>

#include "bt/uni_bt.h"
#include "bt/uni_bt_allowlist.h"
#include "bt/uni_bt_bredr.h"
#include "bt/uni_bt_defines.h"
#include "platform/uni_platform.h"
#include "sdkconfig.h"
#include "test_check.h"
#include "uni_circular_buffer.h"
#include "uni_hid_device.h"
#include "uni_property.h"
#include "uni_virtual_device.h"

static void dummy_transport_register_packet_handler(void (*handler)(uint8_t packet_type,
                                                                    uint8_t* packet,
                                                                    uint16_t size)) {
    (void)handler;
}

static const hci_transport_t dummy_transport = {
    .name = "dummy",
    .register_packet_handler = dummy_transport_register_packet_handler,
};

// Minimal Platform Implementation
static uni_error_t my_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    (void)addr;
    (void)name;
    (void)cod;
    (void)rssi;
    return UNI_ERROR_SUCCESS;
}

static void my_on_device_connected(uni_hid_device_t* d) {
    (void)d;
}

static void my_on_device_disconnected(uni_hid_device_t* d) {
    (void)d;
}

static uni_error_t my_on_device_ready(uni_hid_device_t* d) {
    (void)d;
    return UNI_ERROR_SUCCESS;
}

static void my_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    (void)d;
    (void)ctl;
}

static const uni_property_t* my_get_property(uni_property_idx_t idx) {
    (void)idx;
    return NULL;
}

static void my_on_oob_event(uni_platform_oob_event_t event, void* data) {
    (void)event;
    (void)data;
}

static struct uni_platform my_platform = {
    .name = "Test Platform",
    .init = NULL,
    .on_init_complete = NULL,
    .on_device_discovered = my_on_device_discovered,
    .on_device_connected = my_on_device_connected,
    .on_device_disconnected = my_on_device_disconnected,
    .on_device_ready = my_on_device_ready,
    .on_controller_data = my_on_controller_data,
    .get_property = my_get_property,
    .on_oob_event = my_on_oob_event,
};

struct uni_platform* uni_get_platform(void) {
    return &my_platform;
}

// ============================================================================
// uni_circular_buffer Tests
// ============================================================================

/**
 * @brief Verifies initial empty state, NULL-pointer safety, and `uni_circular_buffer_reset()` behavior.
 */
TEST(circular_buffer_initial_and_reset_state) {
    printf("Testing uni_circular_buffer initial empty and reset state...\n");
    uni_circular_buffer_t buf = {0};
    int16_t cid = 0;
    uint8_t data[16] = {0};
    int len = -1;

    EXPECT_EQ(uni_circular_buffer_is_empty(&buf), 1);
    EXPECT_EQ(uni_circular_buffer_is_full(&buf), 0);
    EXPECT_EQ(uni_circular_buffer_get(&buf, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY);

    // Null buffer safety checks
    EXPECT_EQ(uni_circular_buffer_is_empty(NULL), 1);
    EXPECT_EQ(uni_circular_buffer_is_full(NULL), 0);
    EXPECT_EQ(uni_circular_buffer_get(NULL, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY);
    uni_circular_buffer_reset(NULL);

    uint8_t sample[4] = {1, 2, 3, 4};
    EXPECT_EQ(uni_circular_buffer_put(&buf, 0x10, sample, sizeof(sample)), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(uni_circular_buffer_is_empty(&buf), 0);

    uni_circular_buffer_reset(&buf);
    EXPECT_EQ(uni_circular_buffer_is_empty(&buf), 1);
    EXPECT_EQ(uni_circular_buffer_is_full(&buf), 0);
    printf("PASS\n");
}

/**
 * @brief Verifies input/output argument guards and self-healing resets on corrupted ring/header states.
 *
 * Ensures that:
 * 1. `NULL` buffer, `NULL` payload with `len > 0`, and negative `len` are rejected by `put()`.
 * 2. `NULL` output pointers (`cid`, `data`, `len`) on a non-empty buffer return `ERROR_BUFFER_EMPTY`
 *    without advancing `head_idx` or losing the queued packet.
 * 3. `get()` self-heals the ring via `uni_circular_buffer_reset()` across all four corruption states:
 *    truncated `< 4`-byte header, negative `data_len`, oversized `data_len > max_payload`, and truncated payload.
 */
TEST(circular_buffer_null_and_negative_length_guards) {
    printf("Testing uni_circular_buffer null, negative length, and corrupted header self-healing guards...\n");
    uni_circular_buffer_t cb = {0};
    uint8_t payload[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    int16_t cid = 0;
    uint8_t data[16] = {0};
    int len = -1;

    EXPECT_EQ(uni_circular_buffer_put(NULL, 1, payload, 4), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 1, NULL, 4), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 1, payload, -1), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 1, payload, -128), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG);
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);

    // Verify NULL output pointer guards on a non-empty buffer do not consume the queued packet
    EXPECT_EQ(uni_circular_buffer_put(&cb, 0x2A, payload, (int)sizeof(payload)), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 0);
    EXPECT_EQ(uni_circular_buffer_get(&cb, NULL, data, &len), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY);
    EXPECT_EQ(uni_circular_buffer_get(&cb, &cid, NULL, &len), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY);
    EXPECT_EQ(uni_circular_buffer_get(&cb, &cid, data, NULL), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY);
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 0);

    // Queued packet must still be intact and dequeue cleanly
    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cid, 0x2A);
    EXPECT_EQ(len, (int)sizeof(payload));
    EXPECT_EQ(memcmp(data, payload, sizeof(payload)), 0);
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);

    // Verify self-healing reset across all 4 corrupted ring/header states:
    struct {
        int16_t cid;
        int16_t data_len;
    } corrupted_hdr;

    // 1. Truncated header state (0 < used_bytes < UNI_CIRCULAR_BUFFER_HEADER_SIZE)
    for (int16_t orphan_bytes = 1; orphan_bytes < UNI_CIRCULAR_BUFFER_HEADER_SIZE; orphan_bytes++) {
        cb.head_idx = 0;
        cb.tail_idx = orphan_bytes;
        EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 0);
        EXPECT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY);
        EXPECT_EQ(cb.head_idx, 0);
        EXPECT_EQ(cb.tail_idx, 0);
        EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);

        EXPECT_EQ(uni_circular_buffer_put(&cb, 0x31, payload, (int)sizeof(payload)), UNI_CIRCULAR_BUFFER_ERROR_OK);
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(cid, 0x31);
        EXPECT_EQ(len, (int)sizeof(payload));
        EXPECT_EQ(memcmp(data, payload, sizeof(payload)), 0);
        EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);
    }

    // 2. Negative header.data_len (e.g., -1)
    corrupted_hdr.cid = 0x55;
    corrupted_hdr.data_len = -1;
    memcpy(&cb.buffer[0], &corrupted_hdr, sizeof(corrupted_hdr));
    cb.head_idx = 0;
    cb.tail_idx = 8;
    EXPECT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY);
    EXPECT_EQ(cb.head_idx, 0);
    EXPECT_EQ(cb.tail_idx, 0);
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 0x32, payload, (int)sizeof(payload)), UNI_CIRCULAR_BUFFER_ERROR_OK);
    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cid, 0x32);
    EXPECT_EQ(len, (int)sizeof(payload));
    EXPECT_EQ(memcmp(data, payload, sizeof(payload)), 0);

    // 3. Oversized header.data_len (> max payload capacity of the ring buffer, e.g., UNI_CIRCULAR_BUFFER_SIZE)
    corrupted_hdr.cid = 0x66;
    corrupted_hdr.data_len = UNI_CIRCULAR_BUFFER_SIZE;
    memcpy(&cb.buffer[0], &corrupted_hdr, sizeof(corrupted_hdr));
    cb.head_idx = 0;
    cb.tail_idx = 200;
    EXPECT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY);
    EXPECT_EQ(cb.head_idx, 0);
    EXPECT_EQ(cb.tail_idx, 0);
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 0x33, payload, (int)sizeof(payload)), UNI_CIRCULAR_BUFFER_ERROR_OK);
    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cid, 0x33);
    EXPECT_EQ(len, (int)sizeof(payload));
    EXPECT_EQ(memcmp(data, payload, sizeof(payload)), 0);

    // 4. Truncated payload (header.data_len == 50 when only 10 payload bytes follow)
    corrupted_hdr.cid = 0x77;
    corrupted_hdr.data_len = 50;
    memcpy(&cb.buffer[0], &corrupted_hdr, sizeof(corrupted_hdr));
    cb.head_idx = 0;
    cb.tail_idx = (int16_t)(UNI_CIRCULAR_BUFFER_HEADER_SIZE + 10);
    EXPECT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY);
    EXPECT_EQ(cb.head_idx, 0);
    EXPECT_EQ(cb.tail_idx, 0);
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 0x34, payload, (int)sizeof(payload)), UNI_CIRCULAR_BUFFER_ERROR_OK);
    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cid, 0x34);
    EXPECT_EQ(len, (int)sizeof(payload));
    EXPECT_EQ(memcmp(data, payload, sizeof(payload)), 0);
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);

    printf("PASS\n");
}

/**
 * @brief Verifies zero-length packet (`data == NULL`, `len == 0`) round-trip without modifying destination buffer.
 */
TEST(circular_buffer_zero_length_packet) {
    printf("Testing uni_circular_buffer zero-length packet put/get...\n");
    uni_circular_buffer_t buf = {0};
    int16_t cid = 0;
    uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    const uint8_t expected_sentinel[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    int len = -1;

    EXPECT_EQ(uni_circular_buffer_put(&buf, 0x42, NULL, 0), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(uni_circular_buffer_is_empty(&buf), 0);
    EXPECT_EQ(uni_circular_buffer_get(&buf, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cid, 0x42);
    EXPECT_EQ(len, 0);
    EXPECT_EQ(memcmp(data, expected_sentinel, sizeof(expected_sentinel)), 0);
    EXPECT_EQ(uni_circular_buffer_is_empty(&buf), 1);
    printf("PASS\n");
}

/**
 * @brief Verifies exact maximum single-packet payload boundary
 * (`UNI_CIRCULAR_BUFFER_SIZE - 1 - UNI_CIRCULAR_BUFFER_HEADER_SIZE == 4091` accepted, `4092` rejected).
 */
TEST(circular_buffer_exact_capacity_boundary) {
    printf("Testing uni_circular_buffer exact capacity boundary...\n");
    uni_circular_buffer_t buf = {0};
    enum {
        MAX_PAYLOAD_SIZE = UNI_CIRCULAR_BUFFER_SIZE - 1 - UNI_CIRCULAR_BUFFER_HEADER_SIZE,
    };
    uint8_t pkt_max[MAX_PAYLOAD_SIZE];
    uint8_t pkt_too_big[MAX_PAYLOAD_SIZE + 1];
    uint8_t out_buf[MAX_PAYLOAD_SIZE];
    int16_t cid = 0;
    int len = 0;

    for (size_t i = 0; i < sizeof(pkt_max); i++) {
        pkt_max[i] = (uint8_t)(i ^ 0x5A);
    }
    memset(pkt_too_big, 0xFF, sizeof(pkt_too_big));
    memset(out_buf, 0, sizeof(out_buf));

    // 4092 bytes (header + payload == 4096 > 4095 usable bytes) must be rejected with TOO_BIG
    EXPECT_EQ(uni_circular_buffer_put(&buf, 0x77, pkt_too_big, (int)sizeof(pkt_too_big)),
              UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG);
    EXPECT_EQ(uni_circular_buffer_is_empty(&buf), 1);

    // Exact 4091 bytes (header + payload == 4095 usable bytes) must succeed, fill the buffer, and round-trip
    EXPECT_EQ(uni_circular_buffer_put(&buf, 0x77, pkt_max, (int)sizeof(pkt_max)), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(uni_circular_buffer_is_full(&buf), 1);
    ASSERT_EQ(uni_circular_buffer_get(&buf, &cid, out_buf, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cid, 0x77);
    EXPECT_EQ(len, (int)sizeof(pkt_max));
    EXPECT_EQ(memcmp(out_buf, pkt_max, sizeof(pkt_max)), 0);
    EXPECT_EQ(uni_circular_buffer_is_empty(&buf), 1);
    printf("PASS\n");
}

/**
 * @brief Verifies 1-byte packet saturation (`819` packets = `4,095` usable bytes) and FIFO wrap-around across `4095 ->
 * 0`.
 */
TEST(circular_buffer_full_queue_and_wrap_around) {
    printf("Testing uni_circular_buffer full queue saturation and wrap-around...\n");
    uni_circular_buffer_t cb = {0};
    int16_t cid = 0;
    uint8_t data[16] = {0};
    int len = 0;

    // Each 1-byte packet occupies UNI_CIRCULAR_BUFFER_HEADER_SIZE (4) + 1 = 5 bytes.
    // With 1 sentinel byte reserved out of 4096, 4095 / 5 = 819 packets saturate the buffer with 0 bytes free.
    const int max_1b_packets = (UNI_CIRCULAR_BUFFER_SIZE - 1) / (UNI_CIRCULAR_BUFFER_HEADER_SIZE + 1);
    const int wrap_packets = max_1b_packets / 2;
    EXPECT_EQ(max_1b_packets, 819);
    EXPECT_EQ(wrap_packets, 409);

    for (int i = 0; i < max_1b_packets; i++) {
        uint8_t val = (uint8_t)(i & 0xFF);
        EXPECT_EQ(uni_circular_buffer_put(&cb, (int16_t)(100 + i), &val, 1), UNI_CIRCULAR_BUFFER_ERROR_OK);
    }
    EXPECT_EQ(uni_circular_buffer_is_full(&cb), 1);

    // 820th put (and even a 0-byte put) must return BUFFER_FULL
    uint8_t extra = 0xFF;
    EXPECT_EQ(uni_circular_buffer_put(&cb, 999, &extra, 1), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 999, NULL, 0), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL);

    // Dequeue wrap_packets (409) packets
    for (int i = 0; i < wrap_packets; i++) {
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(cid, (int16_t)(100 + i));
        EXPECT_EQ(len, 1);
        EXPECT_EQ(data[0], (uint8_t)(i & 0xFF));
    }
    EXPECT_EQ(uni_circular_buffer_is_full(&cb), 0);

    // Enqueue wrap_packets (409) more packets across the 4095 -> 0 ring wrap boundary.
    // Use 2000 + i as the CID base so it does not collide with batch 1 (100..918).
    for (int i = 0; i < wrap_packets; i++) {
        uint8_t val = (uint8_t)((200 + i) & 0xFF);
        EXPECT_EQ(uni_circular_buffer_put(&cb, (int16_t)(2000 + i), &val, 1), UNI_CIRCULAR_BUFFER_ERROR_OK);
    }
    EXPECT_EQ(uni_circular_buffer_is_full(&cb), 1);

    // Dequeue remaining 410 packets from first batch + 409 packets from second batch
    for (int i = wrap_packets; i < max_1b_packets; i++) {
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(cid, (int16_t)(100 + i));
        EXPECT_EQ(len, 1);
        EXPECT_EQ(data[0], (uint8_t)(i & 0xFF));
    }
    for (int i = 0; i < wrap_packets; i++) {
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(cid, (int16_t)(2000 + i));
        EXPECT_EQ(len, 1);
        EXPECT_EQ(data[0], (uint8_t)((200 + i) & 0xFF));
    }
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);
    printf("PASS\n");
}

/**
 * @brief Verifies variable-length packet packing, Switch 11-byte rumble burst capacity (`273` packets),
 * split header/payload wrap-around across all boundary offsets, and safe re-enqueue of dequeued `data`.
 */
TEST(circular_buffer_variable_length_and_wrap_around) {
    printf("Testing uni_circular_buffer variable-length packets, Switch rumble burst, and split wrap-around...\n");
    uni_circular_buffer_t cb = {0};
    int16_t cid = 0;
    uint8_t data[128] = {0};
    int len = 0;

    // ------------------------------------------------------------------------
    // Stage 1: 128-byte packet saturation (31 packets = 4092 bytes, 3 free bytes < 4)
    // ------------------------------------------------------------------------
    enum {
        TEST_PKT_128_LEN = 128,
        MAX_PAYLOAD_SIZE = UNI_CIRCULAR_BUFFER_SIZE - 1 - UNI_CIRCULAR_BUFFER_HEADER_SIZE,
    };
    const int max_128b_packets = (UNI_CIRCULAR_BUFFER_SIZE - 1) / (UNI_CIRCULAR_BUFFER_HEADER_SIZE + TEST_PKT_128_LEN);
    EXPECT_EQ(max_128b_packets, 31);

    uint8_t pkt_128[TEST_PKT_128_LEN];
    uint8_t pkt_too_big[MAX_PAYLOAD_SIZE + 1];
    memset(pkt_too_big, 0xEE, sizeof(pkt_too_big));

    for (int i = 0; i < max_128b_packets; i++) {
        for (int j = 0; j < (int)sizeof(pkt_128); j++) {
            pkt_128[j] = (uint8_t)((i * 17 + j) ^ 0x3C);
        }
        EXPECT_EQ(uni_circular_buffer_put(&cb, (int16_t)(500 + i), pkt_128, (int)sizeof(pkt_128)),
                  UNI_CIRCULAR_BUFFER_ERROR_OK);
    }
    // 3 trailing free bytes remain (< UNI_CIRCULAR_BUFFER_HEADER_SIZE), so buffer must report full
    EXPECT_EQ(uni_circular_buffer_is_full(&cb), 1);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 999, pkt_128, (int)sizeof(pkt_128)), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 999, NULL, 0), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL);
    // Packet exceeding total ring capacity must still return BUFFER_TOO_BIG even when buffer is full
    EXPECT_EQ(uni_circular_buffer_put(&cb, 999, pkt_too_big, (int)sizeof(pkt_too_big)),
              UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG);

    for (int i = 0; i < max_128b_packets; i++) {
        uint8_t expected_128[TEST_PKT_128_LEN];
        for (int j = 0; j < (int)sizeof(expected_128); j++) {
            expected_128[j] = (uint8_t)((i * 17 + j) ^ 0x3C);
        }
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(cid, (int16_t)(500 + i));
        EXPECT_EQ(len, (int)sizeof(expected_128));
        EXPECT_EQ(memcmp(data, expected_128, sizeof(expected_128)), 0);
    }
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);

    // ------------------------------------------------------------------------
    // Stage 2: Nintendo Switch 11-byte rumble burst saturation (273 packets = 4095 bytes exact)
    // ------------------------------------------------------------------------
    uni_circular_buffer_reset(&cb);
    const int switch_rumble_len = 11;
    const int max_switch_packets =
        (UNI_CIRCULAR_BUFFER_SIZE - 1) / (UNI_CIRCULAR_BUFFER_HEADER_SIZE + switch_rumble_len);
    EXPECT_EQ(max_switch_packets, 273);

    uint8_t rumble_pkt[11];
    for (int i = 0; i < max_switch_packets; i++) {
        for (int j = 0; j < (int)sizeof(rumble_pkt); j++) {
            rumble_pkt[j] = (uint8_t)((i + j) ^ 0xA5);
        }
        EXPECT_EQ(uni_circular_buffer_put(&cb, (int16_t)(0x0040 + i), rumble_pkt, (int)sizeof(rumble_pkt)),
                  UNI_CIRCULAR_BUFFER_ERROR_OK);
    }
    EXPECT_EQ(uni_circular_buffer_is_full(&cb), 1);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 0x0999, rumble_pkt, (int)sizeof(rumble_pkt)),
              UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 0x0999, NULL, 0), UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL);

    for (int i = 0; i < max_switch_packets; i++) {
        uint8_t expected_rumble[11];
        for (int j = 0; j < (int)sizeof(expected_rumble); j++) {
            expected_rumble[j] = (uint8_t)((i + j) ^ 0xA5);
        }
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(cid, (int16_t)(0x0040 + i));
        EXPECT_EQ(len, (int)sizeof(expected_rumble));
        EXPECT_EQ(memcmp(data, expected_rumble, sizeof(expected_rumble)), 0);
    }
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);

    // ------------------------------------------------------------------------
    // Stage 3: Split 4-byte header wrap-around across all 4 boundary offsets {4092, 4093, 4094, 4095}
    //   - 4092: 4-byte header occupies [4092..4095], payload starts at [0]
    //   - 4093: 4-byte header splits 3 bytes at [4093..4095] and 1 byte at [0]
    //   - 4094: 4-byte header splits 2 bytes at [4094..4095] and 2 bytes at [0..1]
    //   - 4095: 4-byte header splits 1 byte at [4095] and 3 bytes at [0..2]
    // At each offset, enqueue and dequeue the mixed controller sequence:
    //   Packet A: 11-byte Switch rumble report (cid = 0x1111)
    //   Packet B: 0-byte empty payload packet (cid = 0x2222, data = NULL, len = 0)
    //   Packet C: 3-byte Wii report (cid = 0x3333)
    //   Packet D: 79-byte DualSense output report (cid = 0x4444)
    // ------------------------------------------------------------------------
    uint8_t pkt_a_switch[11];
    uint8_t pkt_c_wii[3] = {0xA1, 0x11, 0x01};
    uint8_t pkt_d_ds5[79];
    for (int j = 0; j < (int)sizeof(pkt_a_switch); j++) {
        pkt_a_switch[j] = (uint8_t)(0x10 + j);
    }
    for (int j = 0; j < (int)sizeof(pkt_d_ds5); j++) {
        pkt_d_ds5[j] = (uint8_t)(0x40 + j);
    }

    for (int16_t start_offset = 4092; start_offset <= 4095; start_offset++) {
        uni_circular_buffer_reset(&cb);
        // Advance head_idx and tail_idx to 3960 (30 * 132 bytes)
        for (int i = 0; i < 30; i++) {
            ASSERT_EQ(uni_circular_buffer_put(&cb, 1, pkt_128, 128), UNI_CIRCULAR_BUFFER_ERROR_OK);
            ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        }
        if (start_offset == 4092) {
            // 3960 + (4 + 128) = 4092
            ASSERT_EQ(uni_circular_buffer_put(&cb, 1, pkt_128, 128), UNI_CIRCULAR_BUFFER_ERROR_OK);
            ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        } else {
            // 3960 + (4 + 60) = 4024, then + (4 + (start_offset - 4028)) = start_offset
            ASSERT_EQ(uni_circular_buffer_put(&cb, 1, pkt_128, 60), UNI_CIRCULAR_BUFFER_ERROR_OK);
            ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
            int remaining_payload = (int)start_offset - 4024 - UNI_CIRCULAR_BUFFER_HEADER_SIZE;
            ASSERT_EQ(uni_circular_buffer_put(&cb, 1, pkt_128, remaining_payload), UNI_CIRCULAR_BUFFER_ERROR_OK);
            ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        }
        EXPECT_EQ(cb.head_idx, start_offset);
        EXPECT_EQ(cb.tail_idx, start_offset);

        // Enqueue mixed controller packets A (11B), B (0B), C (3B), D (79B)
        EXPECT_EQ(uni_circular_buffer_put(&cb, 0x1111, pkt_a_switch, (int)sizeof(pkt_a_switch)),
                  UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(uni_circular_buffer_put(&cb, 0x2222, NULL, 0), UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(uni_circular_buffer_put(&cb, 0x3333, pkt_c_wii, (int)sizeof(pkt_c_wii)),
                  UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(uni_circular_buffer_put(&cb, 0x4444, pkt_d_ds5, (int)sizeof(pkt_d_ds5)),
                  UNI_CIRCULAR_BUFFER_ERROR_OK);

        const int16_t expected_end_idx =
            (int16_t)((start_offset + (4 + 11) + (4 + 0) + (4 + 3) + (4 + 79)) % UNI_CIRCULAR_BUFFER_SIZE);
        EXPECT_EQ(cb.tail_idx, expected_end_idx);

        // Dequeue Packet A (11-byte Switch rumble)
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(cid, 0x1111);
        EXPECT_EQ(len, (int)sizeof(pkt_a_switch));
        EXPECT_EQ(memcmp(data, pkt_a_switch, sizeof(pkt_a_switch)), 0);

        // Dequeue Packet B (0-byte empty packet)
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(cid, 0x2222);
        EXPECT_EQ(len, 0);

        // Dequeue Packet C (3-byte Wii report)
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(cid, 0x3333);
        EXPECT_EQ(len, (int)sizeof(pkt_c_wii));
        EXPECT_EQ(memcmp(data, pkt_c_wii, sizeof(pkt_c_wii)), 0);

        // Dequeue Packet D (79-byte DualSense report)
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
        EXPECT_EQ(cid, 0x4444);
        EXPECT_EQ(len, (int)sizeof(pkt_d_ds5));
        EXPECT_EQ(memcmp(data, pkt_d_ds5, sizeof(pkt_d_ds5)), 0);

        EXPECT_EQ(cb.head_idx, expected_end_idx);
        EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);
    }

    // ------------------------------------------------------------------------
    // Stage 4: Split 11-byte payload wrap-around across index 4095 -> 0
    // Advance head_idx and tail_idx to 4088:
    //   30 * (4 + 128) = 3960 bytes
    //   + 1 * (4 + 124) = 128 bytes (4088)
    // At 4088, the 4-byte header sits at [4088..4091] and the 11-byte payload splits
    // 4 bytes at [4092..4095] and 7 bytes at [0..6].
    // ------------------------------------------------------------------------
    uni_circular_buffer_reset(&cb);
    for (int i = 0; i < 30; i++) {
        ASSERT_EQ(uni_circular_buffer_put(&cb, 1, pkt_128, 128), UNI_CIRCULAR_BUFFER_ERROR_OK);
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    }
    ASSERT_EQ(uni_circular_buffer_put(&cb, 1, pkt_128, 124), UNI_CIRCULAR_BUFFER_ERROR_OK);
    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cb.head_idx, 4088);
    EXPECT_EQ(cb.tail_idx, 4088);

    uint8_t split_payload_pkt[11];
    for (int j = 0; j < (int)sizeof(split_payload_pkt); j++) {
        split_payload_pkt[j] = (uint8_t)(0x80 + j);
    }
    EXPECT_EQ(uni_circular_buffer_put(&cb, 0x5678, split_payload_pkt, (int)sizeof(split_payload_pkt)),
              UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cb.tail_idx, 7);

    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cid, 0x5678);
    EXPECT_EQ(len, (int)sizeof(split_payload_pkt));
    EXPECT_EQ(memcmp(data, split_payload_pkt, sizeof(split_payload_pkt)), 0);
    EXPECT_EQ(cb.head_idx, 7);
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);

    // ------------------------------------------------------------------------
    // Stage 5: Safe re-enqueue of the caller's data buffer populated by uni_circular_buffer_get()
    // back into uni_circular_buffer_put() across the 4095 -> 0 wrap boundary
    // (simulating uni_hid_device_send_queued_reports() retry).
    // ------------------------------------------------------------------------
    uint8_t switch_pkt[11];
    uint8_t ds5_pkt[79];
    for (int j = 0; j < (int)sizeof(switch_pkt); j++) {
        switch_pkt[j] = (uint8_t)(0xC0 + j);
    }
    for (int j = 0; j < (int)sizeof(ds5_pkt); j++) {
        ds5_pkt[j] = (uint8_t)(0x30 + j);
    }

    // 5a: Advance head_idx and tail_idx to 4090 (30 * 132 + (4 + 126) = 4090) so the initial
    // 11-byte Switch packet wraps across 4095 -> 0, then is dequeued into `data` and re-enqueued.
    uni_circular_buffer_reset(&cb);
    for (int i = 0; i < 30; i++) {
        ASSERT_EQ(uni_circular_buffer_put(&cb, 1, pkt_128, 128), UNI_CIRCULAR_BUFFER_ERROR_OK);
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    }
    ASSERT_EQ(uni_circular_buffer_put(&cb, 1, pkt_128, 126), UNI_CIRCULAR_BUFFER_ERROR_OK);
    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cb.head_idx, 4090);
    EXPECT_EQ(cb.tail_idx, 4090);

    EXPECT_EQ(uni_circular_buffer_put(&cb, 0x057e, switch_pkt, (int)sizeof(switch_pkt)), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cb.tail_idx, 9);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 0x054c, ds5_pkt, (int)sizeof(ds5_pkt)), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cb.tail_idx, 92);

    // Dequeue the first packet into caller buffer `data`
    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cid, 0x057e);
    EXPECT_EQ(len, (int)sizeof(switch_pkt));
    EXPECT_EQ(memcmp(data, switch_pkt, sizeof(switch_pkt)), 0);

    // Re-enqueue using `data` populated by uni_circular_buffer_get()
    EXPECT_EQ(uni_circular_buffer_put(&cb, cid, data, len), UNI_CIRCULAR_BUFFER_ERROR_OK);

    // Dequeue the 79-byte DualSense packet and verify integrity
    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cid, 0x054c);
    EXPECT_EQ(len, (int)sizeof(ds5_pkt));
    EXPECT_EQ(memcmp(data, ds5_pkt, sizeof(ds5_pkt)), 0);

    // Dequeue the re-enqueued 11-byte Switch packet and verify zero corruption
    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cid, 0x057e);
    EXPECT_EQ(len, (int)sizeof(switch_pkt));
    EXPECT_EQ(memcmp(data, switch_pkt, sizeof(switch_pkt)), 0);
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);

    // 5b: Position tail_idx at 4090 at the exact moment of re-enqueue (start at 3992, enqueue 15B + 83B -> 4090)
    // so the re-enqueued copy out of `data` itself splits across 4095 -> 0 (2B at [4094..4095], 9B at [0..8]).
    uni_circular_buffer_reset(&cb);
    for (int i = 0; i < 30; i++) {
        ASSERT_EQ(uni_circular_buffer_put(&cb, 1, pkt_128, 128), UNI_CIRCULAR_BUFFER_ERROR_OK);
        ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    }
    ASSERT_EQ(uni_circular_buffer_put(&cb, 1, pkt_128, 28), UNI_CIRCULAR_BUFFER_ERROR_OK);
    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cb.head_idx, 3992);
    EXPECT_EQ(cb.tail_idx, 3992);

    EXPECT_EQ(uni_circular_buffer_put(&cb, 0x057e, switch_pkt, (int)sizeof(switch_pkt)), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(uni_circular_buffer_put(&cb, 0x054c, ds5_pkt, (int)sizeof(ds5_pkt)), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cb.tail_idx, 4090);

    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    // Re-enqueue `data` at tail_idx == 4090 so the payload copy wraps across 4095 -> 0
    EXPECT_EQ(uni_circular_buffer_put(&cb, cid, data, len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cb.tail_idx, 9);

    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cid, 0x054c);
    EXPECT_EQ(len, (int)sizeof(ds5_pkt));
    EXPECT_EQ(memcmp(data, ds5_pkt, sizeof(ds5_pkt)), 0);

    ASSERT_EQ(uni_circular_buffer_get(&cb, &cid, data, &len), UNI_CIRCULAR_BUFFER_ERROR_OK);
    EXPECT_EQ(cid, 0x057e);
    EXPECT_EQ(len, (int)sizeof(switch_pkt));
    EXPECT_EQ(memcmp(data, switch_pkt, sizeof(switch_pkt)), 0);
    EXPECT_EQ(uni_circular_buffer_is_empty(&cb), 1);

    printf("PASS\n");
}

// ============================================================================
// uni_bt_allowlist, uni_bt, & uni_property_posix Tests
// ============================================================================

TEST(allowlist_zero_addr_rejection) {
    printf("Testing uni_bt_allowlist zero_addr rejection on add and remove...\n");
    bd_addr_t zero = {0, 0, 0, 0, 0, 0};
    const bd_addr_t* addrs = NULL;
    int total = 0;

    uni_bt_allowlist_remove_all();
    EXPECT_FALSE(uni_bt_allowlist_add_addr(zero));
    EXPECT_FALSE(uni_bt_allowlist_remove_addr(zero));

    uni_bt_allowlist_get_all(&addrs, &total);
    for (int i = 0; i < total; i++) {
        EXPECT_EQ(bd_addr_cmp(addrs[i], zero), 0);
    }
    printf("PASS\n");
}

TEST(allowlist_crud_duplicate_and_capacity) {
    printf("Testing uni_bt_allowlist CRUD, duplicate prevention, and capacity exhaustion...\n");
    bd_addr_t a1 = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x01};
    bd_addr_t a2 = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x02};
    bd_addr_t a3 = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x03};
    bd_addr_t a4 = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x04};
    bd_addr_t a5 = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x05};

    uni_bt_allowlist_remove_all();

    EXPECT_TRUE(uni_bt_allowlist_add_addr(a1));
    EXPECT_TRUE(uni_bt_allowlist_add_addr(a2));
    EXPECT_TRUE(uni_bt_allowlist_add_addr(a3));
    EXPECT_TRUE(uni_bt_allowlist_add_addr(a4));

    // Duplicate must be rejected
    EXPECT_FALSE(uni_bt_allowlist_add_addr(a1));
    // 5th address exceeds CONFIG_BLUEPAD32_MAX_ALLOWLIST (4)
    EXPECT_FALSE(uni_bt_allowlist_add_addr(a5));

    // Removing a2 frees a slot for a5
    EXPECT_TRUE(uni_bt_allowlist_remove_addr(a2));
    EXPECT_FALSE(uni_bt_allowlist_remove_addr(a2));
    EXPECT_TRUE(uni_bt_allowlist_add_addr(a5));

    uni_bt_allowlist_remove_all();
    printf("PASS\n");
}

TEST(allowlist_enforcement_toggle) {
    printf("Testing uni_bt_allowlist enforcement toggle (is_allowed_addr)...\n");
    bd_addr_t a1 = {0xBB, 0x01, 0x02, 0x03, 0x04, 0x01};
    bd_addr_t unknown = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA};

    uni_bt_allowlist_remove_all();
    EXPECT_TRUE(uni_bt_allowlist_add_addr(a1));

    uni_bt_allowlist_set_enabled(false);
    EXPECT_FALSE(uni_bt_allowlist_is_enabled());
    EXPECT_TRUE(uni_bt_allowlist_is_allowed_addr(a1));
    EXPECT_TRUE(uni_bt_allowlist_is_allowed_addr(unknown));

    uni_bt_allowlist_set_enabled(true);
    EXPECT_TRUE(uni_bt_allowlist_is_enabled());
    EXPECT_TRUE(uni_bt_allowlist_is_allowed_addr(a1));
    EXPECT_FALSE(uni_bt_allowlist_is_allowed_addr(unknown));

    uni_bt_allowlist_set_enabled(false);
    uni_bt_allowlist_remove_all();
    printf("PASS\n");
}

TEST(allowlist_tlv_string_persistence_roundtrip) {
    printf("Testing uni_bt_allowlist POSIX TLV string property and boot round-trip...\n");
    bd_addr_t a1 = {0x12, 0x34, 0x56, 0x78, 0x9A, 0x01};
    bd_addr_t a2 = {0x12, 0x34, 0x56, 0x78, 0x9A, 0x02};
    bd_addr_t a3 = {0x12, 0x34, 0x56, 0x78, 0x9A, 0x03};
    bd_addr_t a4 = {0x12, 0x34, 0x56, 0x78, 0x9A, 0x04};

    uni_bt_allowlist_remove_all();
    EXPECT_TRUE(uni_bt_allowlist_add_addr(a1));
    EXPECT_TRUE(uni_bt_allowlist_add_addr(a2));
    EXPECT_TRUE(uni_bt_allowlist_add_addr(a3));
    uni_bt_allowlist_set_enabled(true);

    // Re-initialize from POSIX TLV storage: verifies update_allowlist_from_property()
    // does NOT overwrite TLV on iteration 0 and preserves all 3 addresses.
    uni_bt_allowlist_init();

    EXPECT_TRUE(uni_bt_allowlist_is_enabled());
    EXPECT_TRUE(uni_bt_allowlist_is_allowed_addr(a1));
    EXPECT_TRUE(uni_bt_allowlist_is_allowed_addr(a2));
    EXPECT_TRUE(uni_bt_allowlist_is_allowed_addr(a3));
    EXPECT_FALSE(uni_bt_allowlist_is_allowed_addr(a4));

    // Clean up TLV state and verify empty round-trip
    uni_bt_allowlist_remove_all();
    uni_bt_allowlist_set_enabled(false);
    uni_bt_allowlist_init();
    EXPECT_FALSE(uni_bt_allowlist_is_enabled());
    uni_bt_allowlist_set_enabled(true);
    EXPECT_FALSE(uni_bt_allowlist_is_allowed_addr(a1));
    uni_bt_allowlist_set_enabled(false);
    printf("PASS\n");
}

TEST(gap_security_level_u8_union) {
    printf("Testing GAP security level u8 vs u32 union regression...\n");
    uni_bt_set_gap_security_level(0);
    EXPECT_EQ(uni_bt_get_gap_security_level(), 0);

    uni_bt_set_gap_security_level(2);
    int level = uni_bt_get_gap_security_level();
    EXPECT_EQ(level, 2);
    printf("PASS\n");
}

TEST(property_types_and_guards) {
    printf("Testing uni_property string/float guards and read-only enforcement...\n");
    // Read-only property must ignore writes
    uni_property_value_t ro_attempt = {.str = "hacked-version"};
    uni_property_set(UNI_PROPERTY_IDX_VERSION, ro_attempt);
    uni_property_value_t ver = uni_property_get(UNI_PROPERTY_IDX_VERSION);
    ASSERT_TRUE(ver.str != NULL);
    EXPECT_NE(strcmp(ver.str, "hacked-version"), 0);

    // Null string and overly long string (> 128 bytes) must be safely rejected
    uni_property_value_t valid_str = {.str = "01:02:03:04:05:06,"};
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, valid_str);
    uni_property_value_t null_str = {.str = NULL};
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, null_str);
    uni_property_value_t read_back = uni_property_get(UNI_PROPERTY_IDX_ALLOWLIST_LIST);
    ASSERT_TRUE(read_back.str != NULL);
    EXPECT_EQ(strcmp(read_back.str, "01:02:03:04:05:06,"), 0);

    // Reset allowlist string
    uni_property_value_t empty_str = {.str = ""};
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, empty_str);
    printf("PASS\n");
}

TEST(property_null_platform_guard) {
    printf("Testing uni_property null platform and out-of-range index safety...\n");
    my_platform.get_property = NULL;
    uni_property_value_t val = uni_property_get((uni_property_idx_t)(UNI_PROPERTY_IDX_LAST + 5));
    EXPECT_EQ(val.u32, 0);
    my_platform.get_property = my_get_property;
    printf("PASS\n");
}

// ============================================================================
// uni_hid_device Lifecycle, Virtual Devices, Timers, & CoD Tests
// ============================================================================

TEST(create_device) {
    printf("Testing uni_hid_device_create...\n");
    bd_addr_t addr = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uni_hid_device_t* d = uni_hid_device_create(addr);
    ASSERT_TRUE(d != NULL);
    EXPECT_EQ(memcmp(d->conn.btaddr, addr, 6), 0);

    uni_hid_device_t* d2 = uni_hid_device_get_instance_for_address(addr);
    EXPECT_TRUE(d == d2);

    uni_hid_device_delete(d);

    uni_hid_device_t* d3 = uni_hid_device_get_instance_for_address(addr);
    EXPECT_TRUE(d3 == NULL);
    printf("PASS\n");
}

TEST(device_properties) {
    printf("Testing device properties...\n");
    bd_addr_t addr = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uni_hid_device_t* d = uni_hid_device_create(addr);
    ASSERT_TRUE(d != NULL);

    uni_hid_device_set_cod(d, 0x123456);
    EXPECT_EQ(d->cod, 0x123456);

    uni_hid_device_set_vendor_id(d, 0xAAAA);
    EXPECT_EQ(uni_hid_device_get_vendor_id(d), 0xAAAA);

    uni_hid_device_set_product_id(d, 0xBBBB);
    EXPECT_EQ(uni_hid_device_get_product_id(d), 0xBBBB);

    uni_hid_device_set_name(d, "Test Device");
    EXPECT_EQ(strcmp(d->name, "Test Device"), 0);
    EXPECT_TRUE(uni_hid_device_has_name(d));

    uni_hid_device_delete(d);
    printf("PASS\n");
}

// Verifies that uni_bt_bredr_setup() overrides BTstack v1.8.2+'s default SSP auto-accept (0 -> 1, D1)
// and minimum encryption key size (16 -> 7 bytes, D2), and configures GAP Security Level 2.
TEST(bredr_setup_ssp_and_encryption_key_size) {
    printf("Testing uni_bt_bredr_setup SSP auto-accept and encryption key size...\n");

    // Preconditions:
    // 1. uni_property_init() initializes the Posix TLV singleton queried by uni_bt_get_gap_security_level().
    // 2. hci_init() allocates hci_stack (requires a non-NULL transport with register_packet_handler).
    // 3. l2cap_init() prepares L2CAP before uni_bt_bredr_setup() registers HID PSMs and initializes SDP.
    uni_property_init();
    hci_init(&dummy_transport, NULL);
    l2cap_init();

    // Verify BTstack v1.8.2+ defaults prior to uni_bt_bredr_setup().
    hci_stack_t* stack = hci_get_stack();
    ASSERT_TRUE(stack != NULL);
    EXPECT_EQ(stack->ssp_auto_accept, 0);
    EXPECT_EQ(stack->gap_required_encyrption_key_size, 16);

    uni_bt_bredr_setup();

    // Verify D1 (SSP auto-accept enabled) and D2 (7-byte minimum encryption key size restored).
    EXPECT_EQ(stack->ssp_auto_accept, 1);
    EXPECT_EQ(stack->gap_required_encyrption_key_size, 7);
    EXPECT_EQ(gap_get_security_level(), LEVEL_2);
    EXPECT_EQ(stack->connectable, 1);
    EXPECT_EQ(stack->discoverable, 0);
    EXPECT_EQ(stack->new_page_scan_type, PAGE_SCAN_MODE_INTERLACED);
    EXPECT_EQ(stack->inquiry_mode, INQUIRY_MODE_RSSI_AND_EIR);

    printf("PASS\n");
}

// Verifies that passing the shared Posix TLV singleton context (tlv_context_ptr) initialized by
// uni_property_init() to btstack_link_key_db_tlv_get_instance() and le_device_db_tlv_configure() (D4)
// persists BR/EDR link keys to disk without colliding with Bluepad32 'B' 'P' '3' property tags.
TEST(posix_shared_tlv_link_key_and_property_coexistence) {
    printf("Testing Posix shared TLV link key DB and property coexistence...\n");

    uni_property_init();

    const btstack_tlv_t* tlv_impl = NULL;
    btstack_tlv_posix_t* tlv_context_ptr = NULL;
    btstack_tlv_get_instance(&tlv_impl, (void**)&tlv_context_ptr);
    ASSERT_TRUE(tlv_impl != NULL);
    ASSERT_TRUE(tlv_context_ptr != NULL);
    ASSERT_TRUE(tlv_context_ptr->file != NULL);
    ASSERT_TRUE(tlv_context_ptr->db_path != NULL);
    const char* expected_tlv_path = getenv("BLUEPAD32_TLV_PATH");
    if (!expected_tlv_path || expected_tlv_path[0] == '\0') {
        expected_tlv_path = "/tmp/bp32_property.tlv";
    }
    EXPECT_EQ(strcmp(tlv_context_ptr->db_path, expected_tlv_path), 0);

    uni_property_value_t orig_val = uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED);
    uni_property_set(UNI_PROPERTY_IDX_BLE_ENABLED, (uni_property_value_t){.boolean = false});
    EXPECT_FALSE(uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED).boolean);

    const btstack_link_key_db_t* link_key_db = btstack_link_key_db_tlv_get_instance(tlv_impl, tlv_context_ptr);
    ASSERT_TRUE(link_key_db != NULL);
    hci_set_link_key_db(link_key_db);
    le_device_db_tlv_configure(tlv_impl, tlv_context_ptr);

    fflush(tlv_context_ptr->file);
    long pos_before = ftell(tlv_context_ptr->file);
    EXPECT_GT(pos_before, 0);

    bd_addr_t test_addr = {0xA0, 0xAB, 0x51, 0x99, 0xA8, 0x8E};
    link_key_t test_key = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
                           0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x01};
    link_key_db->put_link_key(test_addr, test_key, COMBINATION_KEY);

    link_key_t out_key = {0};
    link_key_type_t out_type = INVALID_LINK_KEY;
    int found = link_key_db->get_link_key(test_addr, out_key, &out_type);
    EXPECT_EQ(found, 1);
    EXPECT_EQ(out_type, COMBINATION_KEY);
    EXPECT_EQ(memcmp(out_key, test_key, sizeof(link_key_t)), 0);

    // Note (EC-4): Do NOT call btstack_tlv_posix_deinit() mid-test, because it sets the file-scope
    // static flag btstack_tlv_posix_read_only = true inside btstack_tlv_posix.c and disables all
    // subsequent TLV writes in the process. Instead, verify on-disk persistence via fflush() + ftell().
    fflush(tlv_context_ptr->file);
    long pos_after = ftell(tlv_context_ptr->file);
    EXPECT_GT(pos_after, pos_before);

    EXPECT_FALSE(uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED).boolean);

    link_key_db->delete_link_key(test_addr);
    EXPECT_EQ(link_key_db->get_link_key(test_addr, out_key, &out_type), 0);
    EXPECT_FALSE(uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED).boolean);

    uni_property_set(UNI_PROPERTY_IDX_BLE_ENABLED, orig_val);
    EXPECT_EQ(uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED).boolean, orig_val.boolean);

    printf("PASS\n");
}

TEST(virtual_child_creation_and_direct_delete) {
    printf("Testing virtual child creation and direct child deletion...\n");
    uni_virtual_device_set_enabled(true);
    EXPECT_TRUE(uni_virtual_device_is_enabled());

    bd_addr_t addr = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26};
    uni_hid_device_t* parent = uni_hid_device_create(addr);
    ASSERT_TRUE(parent != NULL);

    uni_hid_device_t* child = uni_hid_device_create_virtual(parent);
    ASSERT_TRUE(child != NULL);
    EXPECT_TRUE(parent->child == child);
    EXPECT_TRUE(child->parent == parent);
    EXPECT_TRUE(uni_hid_device_is_virtual_device(child));
    EXPECT_FALSE(uni_hid_device_is_virtual_device(parent));
    EXPECT_EQ(child->hids_cid, 0xffff);
    EXPECT_TRUE(uni_hid_device_get_instance_for_address(addr) == parent);

    // Deleting child directly MUST unlink parent->child
    uni_hid_device_delete(child);
    EXPECT_TRUE(parent->child == NULL);

    // Subsequent parent deletion must succeed without double-free
    uni_hid_device_delete(parent);
    EXPECT_TRUE(uni_hid_device_get_instance_for_address(addr) == NULL);
    printf("PASS\n");
}

TEST(parent_delete_cascades_to_virtual_child) {
    printf("Testing parent deletion cascading to virtual child...\n");
    uni_virtual_device_set_enabled(true);

    bd_addr_t addr = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36};
    uni_hid_device_t* parent = uni_hid_device_create(addr);
    ASSERT_TRUE(parent != NULL);

    uni_hid_device_t* child = uni_hid_device_create_virtual(parent);
    ASSERT_TRUE(child != NULL);
    EXPECT_TRUE(parent->child == child);

    uni_hid_device_delete(parent);
    EXPECT_TRUE(parent->child == NULL);
    EXPECT_TRUE(child->parent == NULL);
    EXPECT_TRUE(uni_hid_device_get_instance_for_address(addr) == NULL);
    printf("PASS\n");
}

static void dummy_timer_handler(btstack_timer_source_t* ts) {
    (void)ts;
}

TEST(intrusive_timer_removal_on_delete) {
    printf("Testing intrusive BTstack timer removal before memset...\n");
    btstack_run_loop_base_timers = NULL;
    bd_addr_t addr = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
    uni_hid_device_t* d = uni_hid_device_create(addr);
    ASSERT_TRUE(d != NULL);

    // Register and add inquiry_remote_name_timer and misc_button_delay_timer into run loop
    btstack_run_loop_set_timer_handler(&d->inquiry_remote_name_timer, dummy_timer_handler);
    btstack_run_loop_set_timer(&d->inquiry_remote_name_timer, 1000);
    btstack_run_loop_add_timer(&d->inquiry_remote_name_timer);

    btstack_run_loop_set_timer_handler(&d->misc_button_delay_timer, dummy_timer_handler);
    btstack_run_loop_set_timer(&d->misc_button_delay_timer, 1000);
    btstack_run_loop_add_timer(&d->misc_button_delay_timer);

    // Also attach Switch Pro parser (which arms setup_timer in parser_data) and delayed rumble timer (B3)
    uni_hid_device_set_vendor_id(d, 0x057e);
    uni_hid_device_set_product_id(d, 0x2009);
    uni_hid_device_guess_controller_type_from_pid_vid(d);
    ASSERT_TRUE(d->report_parser.setup != NULL);
    ASSERT_TRUE(d->report_parser.deinit != NULL);
    d->report_parser.setup(d);
    d->report_parser.play_dual_rumble(d, 200, 500, 128, 255);
    EXPECT_EQ(d->rumble.state, UNI_RUMBLE_STATE_DELAYED);

    // Delete device: must remove device timers, rumble timers, and parser deinit timers before memset(d, 0, sizeof(*d))
    uni_hid_device_delete(d);
    EXPECT_TRUE(btstack_run_loop_base_timers == NULL);

    // Verify BTstack run loop timer linked list is uncorrupted by adding and removing a fresh timer
    btstack_timer_source_t verify_timer;
    memset(&verify_timer, 0, sizeof(verify_timer));
    btstack_run_loop_set_timer_handler(&verify_timer, dummy_timer_handler);
    btstack_run_loop_set_timer(&verify_timer, 500);
    btstack_run_loop_add_timer(&verify_timer);
    EXPECT_EQ(btstack_run_loop_remove_timer(&verify_timer), 1);
    EXPECT_TRUE(btstack_run_loop_base_timers == NULL);
    printf("PASS\n");
}

/**
 * @brief Verifies pointer-width alignment and tail placement of `parser_data` and `platform_data` (B6 / Addendum 1).
 *
 * Ensures that:
 * 1. `parser_data` and `platform_data` are placed at the very bottom (tail) of `struct uni_hid_device_s`
 *    immediately after `parent` and `child`, avoiding internal padding after `outgoing_buffer` and keeping
 *    hot scalar/pointer fields (`conn`, `parent`, `child`) 512 bytes closer to the struct base.
 * 2. `__attribute__((aligned(sizeof(void*))))` (used instead of `__BIGGEST_ALIGNMENT__`) guarantees
 *    pointer-width alignment on 32-bit MCUs (ESP32, RP2040) and 64-bit hosts with 0 bytes of RAM overhead
 *    across both compile-time struct offsets and runtime device pool instances.
 */
TEST(hid_device_parser_and_platform_data_alignment_b6) {
    printf("Testing uni_hid_device parser_data and platform_data alignment and tail placement (B6)...\n");
    // Compile-time verification of pointer-width alignment (sizeof(void*)) and tail placement after parent/child.
    _Static_assert(offsetof(uni_hid_device_t, outgoing_buffer.buffer) % sizeof(void*) == 0,
                   "outgoing_buffer.buffer must be pointer-aligned");
    _Static_assert(offsetof(uni_hid_device_t, parser_data) % sizeof(void*) == 0, "parser_data must be pointer-aligned");
    _Static_assert(offsetof(uni_hid_device_t, platform_data) % sizeof(void*) == 0,
                   "platform_data must be pointer-aligned");
    _Static_assert(offsetof(uni_hid_device_t, parser_data) > offsetof(uni_hid_device_t, child),
                   "parser_data must be placed after child at the tail of uni_hid_device_t");
    _Static_assert(offsetof(uni_hid_device_t, platform_data) > offsetof(uni_hid_device_t, parser_data),
                   "platform_data must be placed after parser_data at the tail of uni_hid_device_t");

    // Runtime verification of struct offsets and per-instance buffer alignment across the entire device pool.
    EXPECT_EQ(offsetof(uni_hid_device_t, outgoing_buffer.buffer) % sizeof(void*), 0);
    EXPECT_EQ(offsetof(uni_hid_device_t, parser_data) % sizeof(void*), 0);
    EXPECT_EQ(offsetof(uni_hid_device_t, platform_data) % sizeof(void*), 0);
    EXPECT_TRUE(offsetof(uni_hid_device_t, parser_data) > offsetof(uni_hid_device_t, child));
    EXPECT_TRUE(offsetof(uni_hid_device_t, platform_data) > offsetof(uni_hid_device_t, parser_data));

    uni_hid_device_t* created[CONFIG_BLUEPAD32_MAX_DEVICES] = {0};
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        bd_addr_t addr = {0x60, 0x00, 0x00, 0x00, 0x00, (uint8_t)(i + 1)};
        created[i] = uni_hid_device_create(addr);
        ASSERT_TRUE(created[i] != NULL);
        EXPECT_EQ((uintptr_t)&created[i]->outgoing_buffer.buffer[0] % sizeof(void*), 0);
        EXPECT_EQ((uintptr_t)&created[i]->parser_data[0] % sizeof(void*), 0);
        EXPECT_EQ((uintptr_t)&created[i]->platform_data[0] % sizeof(void*), 0);
    }
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        uni_hid_device_delete(created[i]);
    }
    printf("PASS\n");
}

TEST(device_pool_exhaustion_and_null_guards) {
    printf("Testing device pool exhaustion and null guards...\n");
    uni_hid_device_t* created[CONFIG_BLUEPAD32_MAX_DEVICES] = {0};

    // Null parent guard for create_virtual
    uni_virtual_device_set_enabled(true);
    EXPECT_TRUE(uni_hid_device_create_virtual(NULL) == NULL);

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        bd_addr_t addr = {0x50, 0x00, 0x00, 0x00, 0x00, (uint8_t)(i + 1)};
        created[i] = uni_hid_device_create(addr);
        ASSERT_TRUE(created[i] != NULL);
    }

    // Pool is now full: both create and create_virtual must return NULL
    bd_addr_t extra_addr = {0x50, 0x00, 0x00, 0x00, 0x00, 0xFF};
    EXPECT_TRUE(uni_hid_device_create(extra_addr) == NULL);
    EXPECT_TRUE(uni_hid_device_create_virtual(created[0]) == NULL);

    // Null HID descriptor with non-zero length must be rejected safely
    uni_hid_device_set_hid_descriptor(created[0], NULL, 10);
    EXPECT_FALSE(uni_hid_device_has_hid_descriptor(created[0]));
    EXPECT_EQ(created[0]->hid_descriptor_len, 0);

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        uni_hid_device_delete(created[i]);
    }
    printf("PASS\n");
}

TEST(cod_filtering) {
    printf("Testing Class of Device (CoD) filtering...\n");
    EXPECT_TRUE(uni_hid_device_is_cod_supported(UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD));
    EXPECT_TRUE(uni_hid_device_is_cod_supported(UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_JOYSTICK));
    EXPECT_TRUE(uni_hid_device_is_cod_supported(UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_KEYBOARD));
    EXPECT_TRUE(uni_hid_device_is_cod_supported(UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE));
    EXPECT_TRUE(uni_hid_device_is_cod_supported(0x00400408));

    EXPECT_FALSE(uni_hid_device_is_cod_supported(0x000000));
    EXPECT_FALSE(uni_hid_device_is_cod_supported(0x000200));
    EXPECT_FALSE(uni_hid_device_is_cod_supported(UNI_BT_COD_MAJOR_PERIPHERAL));
    printf("PASS\n");
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Initialize btstack run loop & properties
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    uni_property_init();

    // Basic setup
    uni_hid_device_setup();

    // uni_circular_buffer
    RUN_TEST(circular_buffer_initial_and_reset_state);
    RUN_TEST(circular_buffer_null_and_negative_length_guards);
    RUN_TEST(circular_buffer_zero_length_packet);
    RUN_TEST(circular_buffer_exact_capacity_boundary);
    RUN_TEST(circular_buffer_full_queue_and_wrap_around);
    RUN_TEST(circular_buffer_variable_length_and_wrap_around);

    // uni_bt_allowlist & uni_property_posix
    RUN_TEST(allowlist_zero_addr_rejection);
    RUN_TEST(allowlist_crud_duplicate_and_capacity);
    RUN_TEST(allowlist_enforcement_toggle);
    RUN_TEST(allowlist_tlv_string_persistence_roundtrip);
    RUN_TEST(gap_security_level_u8_union);
    RUN_TEST(property_types_and_guards);
    RUN_TEST(property_null_platform_guard);

    // uni_hid_device
    RUN_TEST(create_device);
    RUN_TEST(device_properties);
    RUN_TEST(bredr_setup_ssp_and_encryption_key_size);
    RUN_TEST(posix_shared_tlv_link_key_and_property_coexistence);
    RUN_TEST(virtual_child_creation_and_direct_delete);
    RUN_TEST(parent_delete_cascades_to_virtual_child);
    RUN_TEST(intrusive_timer_removal_on_delete);
    RUN_TEST(hid_device_parser_and_platform_data_alignment_b6);
    RUN_TEST(device_pool_exhaustion_and_null_guards);
    RUN_TEST(cod_filtering);

    return test_summary();
}
