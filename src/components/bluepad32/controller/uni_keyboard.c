// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "controller/uni_keyboard.h"

#include "hid_usage.h"
#include "uni_log.h"

void uni_keyboard_dump(const uni_keyboard_t* kb) {
    // Don't add "\n"
    logi("modifiers=%#x, pressed keys=[", kb->modifiers);
    for (int i = 0; i < UNI_KEYBOARD_PRESSED_KEYS_MAX; i++) {
        if (kb->pressed_keys[i] == HID_USAGE_KB_NONE || kb->pressed_keys[i] == HID_USAGE_KB_ERROR_ROLL_OVER)
            break;
        if (i > 0)
            logi(", ");
        logi("%#x", kb->pressed_keys[i]);
    }
    logi("]");
}
