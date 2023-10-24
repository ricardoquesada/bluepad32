/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2022 Ricardo Quesada

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
****************************************************************************/

#include "uni_keyboard.h"
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
