// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

// Supported controller:
// https://atari.com/products/classic-joystick

#include "parser/uni_hid_parser_atari.h"

#include "controller/uni_controller.h"
#include "hid_usage.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

typedef struct __attribute((packed)) {
    uint8_t report_id;  // 0x01
    uint8_t buttons[2];
    uint16_t axis;
} atari_input_report_01_t;

static void atari_parse_report_id_01(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    // Report example:
    // 01 00 00 FF 03
    if (len != sizeof(atari_input_report_01_t)) {
        logi("Atari: Unexpected report len = %d\n", len);
        return;
    }

    const atari_input_report_01_t* r = (const atari_input_report_01_t*)report;
    uni_controller_t* ctl = &d->controller;

    // Main button, at the top-left
    ctl->gamepad.buttons |= (r->buttons[0] & 0x01) ? BUTTON_A : 0;
    ctl->gamepad.buttons |= (r->buttons[0] & 0x02) ? BUTTON_B : 0;

    ctl->gamepad.misc_buttons |= (r->buttons[1] & 0x01) ? MISC_BUTTON_SYSTEM : 0;
    ctl->gamepad.misc_buttons |= (r->buttons[1] & 0x02) ? MISC_BUTTON_SELECT : 0;
    ctl->gamepad.misc_buttons |= (r->buttons[1] & 0x04) ? MISC_BUTTON_START : 0;

    const uint8_t dpad_map[] = {
        0,                       // idle
        DPAD_UP,                 // 1
        DPAD_UP | DPAD_RIGHT,    // 2
        DPAD_RIGHT,              // 3
        DPAD_DOWN | DPAD_RIGHT,  // 4
        DPAD_DOWN,               // 5
        DPAD_DOWN | DPAD_LEFT,   // 6
        DPAD_LEFT,               // 7
        DPAD_UP | DPAD_LEFT,     // 8
    };

    // Convert joystick to dpad
    uint8_t joy_value = r->buttons[1] >> 4;
    ctl->gamepad.dpad = dpad_map[joy_value];

    // No need to map, already in the 0, 0x400 range, but just in case in changes.
    ctl->gamepad.throttle = r->axis & 0x3ff;
}

static void atari_parse_report_id_02(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    ARG_UNUSED(d);
    // Battery status (???)
    // Report example:
    // 02 34
    printf_hexdump(report, len);
}

static void atari_parse_report_id_03(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    ARG_UNUSED(d);
    // Unknown
    // Report example:
    // 03 44 03 77 1B 4F EC 45 01
    printf_hexdump(report, len);
}

void uni_hid_parser_atari_setup(struct uni_hid_device_s* d) {
    uni_hid_device_set_ready_complete(d);
}

void uni_hid_parser_atari_init_report(uni_hid_device_t* d) {
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));
    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_atari_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    // Report Id
    switch (report[0]) {
        case 0x01:
            atari_parse_report_id_01(d, report, len);
            break;

        case 0x02:
            atari_parse_report_id_02(d, report, len);
            break;

        case 0x03:
            atari_parse_report_id_03(d, report, len);
            break;

        default:
            logi("Atari: Unknown report id = %x#\n", report[0]);
    }
}
