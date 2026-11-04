/* Copyright (c) 2026 ultrausbt — https://github.com/trickydee/ultrausbt-amiga
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef UNI_BT_HID_APPEARANCE_H
#define UNI_BT_HID_APPEARANCE_H
#include <stddef.h>
#include <stdint.h>

// Identify an unambiguous top-level Generic Desktop Application Collection.
// Vendor/consumer collections are ignored. Mixed keyboard/mouse devices are
// rejected rather than assigning the wrong parser. No report bytes are consumed.
static inline uint16_t uni_bt_hid_appearance(const uint8_t* data, size_t len) {
    uint32_t page = 0, usage = 0, pages[8];
    unsigned depth = 0, stack = 0;
    uint16_t result = 0;
    for (size_t pos = 0; pos < len;) {
        uint8_t prefix = data[pos++];
        if (prefix == 0xfe)
            return 0;  // Unsupported long item: fail closed.
        unsigned n = prefix & 3;
        if (n == 3)
            n = 4;
        if (n > len - pos)
            return 0;
        uint32_t value = 0;
        for (unsigned i = 0; i < n; i++)
            value |= (uint32_t)data[pos++] << (8 * i);
        unsigned type = (prefix >> 2) & 3, tag = prefix >> 4;
        if (type == 1) {
            if (tag == 0)
                page = value;
            else if (tag == 10) {
                if (stack == 8)
                    return 0;
                pages[stack++] = page;
            } else if (tag == 11) {
                if (!stack)
                    return 0;
                page = pages[--stack];
            }
        } else if (type == 2 && tag == 0) {
            usage = n == 4 ? value : ((page << 16) | value);
        } else if (type == 0) {
            if (tag == 10) {
                if (depth == 0 && value == 1 && (usage >> 16) == 1) {
                    uint16_t found = 0;
                    switch (usage & 0xffff) {
                        case 2:
                            found = 0x03c2;
                            break;  // mouse
                        case 6:
                            found = 0x03c1;
                            break;  // keyboard
                        case 4:
                            found = 0x03c3;
                            break;  // joystick
                        case 5:
                            found = 0x03c4;
                            break;  // gamepad
                        default:
                            break;
                    }
                    if (found && result && found != result)
                        return 0;
                    if (found)
                        result = found;
                }
                depth++;
            } else if (tag == 12) {
                if (!depth)
                    return 0;
                depth--;
            }
            usage = 0;  // HID local items expire after each main item.
        }
    }
    return depth || stack ? 0 : result;
}
#endif
