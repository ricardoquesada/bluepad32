/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2023 Ricardo Quesada

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

#include "uni_hid_parser_keyboard.h"

#include <math.h>
#include <time.h>

#include "hid_usage.h"
#include "uni_common.h"
#include "uni_controller.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_log.h"

void uni_hid_parser_keyboard_setup(uni_hid_device_t* d) {
    uni_hid_device_set_ready_complete(d);
}

void uni_hid_parser_keyboard_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    ARG_UNUSED(d);
    printf_hexdump(report, len);
}

void uni_hid_parser_keyboard_init_report(uni_hid_device_t* d) {
    // Reset old state. Each report contains a full-state.
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));
    ctl->klass = UNI_CONTROLLER_CLASS_KEYBOARD;
}

void uni_hid_parser_keyboard_parse_usage(uni_hid_device_t* d,
                                         hid_globals_t* globals,
                                         uint16_t usage_page,
                                         uint16_t usage,
                                         int32_t value) {
    ARG_UNUSED(globals);
    ARG_UNUSED(d);

    switch (usage_page) {
        case HID_USAGE_PAGE_KEYBOARD_KEYPAD:
            logi("page:%d, usage:%d, value:%d\n", usage_page, usage, value);
            break;

        case HID_USAGE_PAGE_CONSUMER:
            logi("page:%d, usage:%d, value:%d\n", usage_page, usage, value);
            break;

        // unknown usage page
        default:
            logi("Keyboard: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
            break;
    }
}

void uni_hid_parser_keyboard_device_dump(struct uni_hid_device_s* d) {
    ARG_UNUSED(d);

    logi("\tuni_hid_parser_keyboard_device_dump: implement me: \n");
}
