// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

// For more info about Android mappings see:
// https://developer.android.com/training/game-controllers/controller-input

#include "parser/uni_hid_parser_smarttvremote.h"

#include "hid_usage.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

void uni_hid_parser_smarttvremote_init_report(uni_hid_device_t* d) {
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));

    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}
void uni_hid_parser_smarttvremote_parse_usage(uni_hid_device_t* d,
                                              hid_globals_t* globals,
                                              uint16_t usage_page,
                                              uint16_t usage,
                                              int32_t value) {
    ARG_UNUSED(globals);
    uni_controller_t* ctl = &d->controller;
    switch (usage_page) {
        case HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS:
            switch (usage) {
                case HID_USAGE_BATTERY_STRENGTH:
                    ctl->battery = value;
                    break;
                default:
                    logi("SmartTVRemote: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage,
                         value);
                    break;
            }
            break;
        case HID_USAGE_PAGE_KEYBOARD_KEYPAD:
            // FIXME: It is unlikely a device has both a dpap a keyboard, so we report
            // certain keys as dpad, just to avoid having a entry entry in the
            // uni_gamepad_t type.
            switch (usage) {
                case 0x00:  // Reserved, empty on purpose
                    break;
                case HID_USAGE_KB_RIGHT_ARROW:
                    if (value)
                        ctl->gamepad.dpad |= DPAD_RIGHT;
                    break;
                case HID_USAGE_KB_LEFT_ARROW:
                    if (value)
                        ctl->gamepad.dpad |= DPAD_LEFT;
                    break;
                case HID_USAGE_KB_DOWN_ARROW:
                    if (value)
                        ctl->gamepad.dpad |= DPAD_DOWN;
                    break;
                case HID_USAGE_KB_UP_ARROW:
                    if (value)
                        ctl->gamepad.dpad |= DPAD_UP;
                    break;
                case HID_USAGE_KP_ENTER:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_A;
                    break;
                case HID_USAGE_KB_POWER:
                    break;  // unmapped apparently
                case 0xf1:  // Back button (reserved)
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;
                    break;
                default:
                    logi("SmartTVRemote: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage,
                         value);
                    break;
            }
            break;
        case HID_USAGE_PAGE_CONSUMER: {
            switch (usage) {
                case HID_USAGE_MENU:
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
                    break;
                case HID_USAGE_MEDIA_SELECT_TV:
                case HID_USAGE_FAST_FORWARD:
                case HID_USAGE_REWIND:
                case HID_USAGE_PLAY_PAUSE:
                case HID_USAGE_MUTE:
                case HID_USAGE_VOLUME_UP:
                case HID_USAGE_VOLUME_DOWN:
                case HID_USAGE_AC_SEARCH:  // mic / search
                    break;
                case HID_USAGE_AC_HOME:
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_START;
                    break;
                default:
                    logi("SmartTVRemote: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage,
                         value);
                    break;
            }
            break;
        }
        // unknown usage page
        default:
            logi("SmartTVRemote: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
            break;
    }
}
