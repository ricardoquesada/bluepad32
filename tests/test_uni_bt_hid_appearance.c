/* Copyright (c) 2026 ultrausbt. SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include "bt/uni_bt_hid_appearance.h"
#define CHECK(expected, ...)                                       \
    do {                                                           \
        const uint8_t d[] = {__VA_ARGS__};                         \
        assert(uni_bt_hid_appearance(d, sizeof(d)) == (expected)); \
    } while (0)
int main(void) {
    CHECK(0x3c2, 0x05, 1, 0x09, 2, 0xa1, 1, 0x09, 1, 0xa1, 0, 0xc0, 0xc0);
    CHECK(0x3c1, 0x05, 1, 0x09, 6, 0xa1, 1, 0xc0);
    CHECK(0x3c3, 0x05, 1, 0x09, 4, 0xa1, 1, 0xc0);
    CHECK(0x3c4, 0x05, 1, 0x09, 5, 0xa1, 1, 0xc0);
    // Consumer control collection before keyboard; never classify it as mouse.
    CHECK(0x3c1, 0x05, 0x0c, 0x09, 1, 0xa1, 1, 0xc0, 0x05, 1, 0x09, 6, 0xa1, 1, 0xc0);
    CHECK(0, 0x05, 1, 0x09, 2, 0xa1, 1, 0xc0, 0x09, 6, 0xa1, 1, 0xc0);
    CHECK(0, 0x05, 1, 0x09, 2, 0xa1, 1);              // unterminated
    CHECK(0, 0x05, 1, 0x0b, 2);                       // truncated
    CHECK(0, 0xc0);                                   // collection underflow
    CHECK(0, 0xb4);                                   // global pop underflow
    CHECK(0, 0x06, 0, 0xff, 0x09, 2, 0xa1, 1, 0xc0);  // vendor usage
    CHECK(0x3c2, 0x0b, 2, 0, 1, 0, 0xa1, 1, 0xc0);    // extended usage
    CHECK(0x3c1, 0x05, 1, 0xa4, 0x05, 0x0c, 0xb4, 0x09, 6, 0xa1, 1, 0xc0);
    CHECK(0, 0x05, 1, 0x09, 2, 0x81, 0, 0xa1, 1, 0xc0);  // local usage expired
    assert(uni_bt_hid_appearance(NULL, 0) == 0);
    puts("BLE HID classification tests passed");
}
