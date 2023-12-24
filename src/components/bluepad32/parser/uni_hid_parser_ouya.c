// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "parser/uni_hid_parser_ouya.h"

#include "hid_usage.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

void uni_hid_parser_ouya_init_report(uni_hid_device_t* d) {
    // Reset old state. Each report contains a full-state.
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));
    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_ouya_parse_usage(uni_hid_device_t* d,
                                     hid_globals_t* globals,
                                     uint16_t usage_page,
                                     uint16_t usage,
                                     int32_t value) {
    uni_controller_t* ctl = &d->controller;
    switch (usage_page) {
        case HID_USAGE_PAGE_GENERIC_DESKTOP:
            switch (usage) {
                case HID_USAGE_AXIS_X:
                    ctl->gamepad.axis_x = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_Y:
                    ctl->gamepad.axis_y = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_Z:
                    ctl->gamepad.brake = uni_hid_parser_process_pedal(globals, value);
                    break;
                case HID_USAGE_AXIS_RX:
                    ctl->gamepad.axis_rx = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_RY:
                    ctl->gamepad.axis_ry = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_RZ:
                    ctl->gamepad.throttle = uni_hid_parser_process_pedal(globals, value);
                    break;
                default:
                    logi("OUYA: Unsupported usage page=0x%04x, page=0x%04x, value=0x%04x\n", usage_page, usage, value);
                    break;
            }
            break;
        case HID_USAGE_PAGE_BUTTON:
            switch (usage) {
                case 1:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_A;
                    break;
                case 2:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_X;
                    break;
                case 3:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_Y;
                    break;
                case 4:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_B;
                    break;
                case 5:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_SHOULDER_L;
                    break;
                case 6:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_SHOULDER_R;
                    break;
                case 7:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_THUMB_L;
                    break;
                case 8:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_THUMB_R;
                    break;
                case 9:
                    if (value)
                        ctl->gamepad.dpad |= DPAD_UP;
                    break;
                case 0x0a:
                    if (value)
                        ctl->gamepad.dpad |= DPAD_DOWN;
                    break;
                case 0x0b:
                    if (value)
                        ctl->gamepad.dpad |= DPAD_LEFT;
                    break;
                case 0x0c:
                    if (value)
                        ctl->gamepad.dpad |= DPAD_RIGHT;
                    break;
                case 0x0d:  // Triggered by Brake pedal
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_TRIGGER_L;
                    break;
                case 0x0e:  // Triggered by Accelerator pedal
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_TRIGGER_R;
                    break;
                case 0x0f:
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
                    break;
                case 0x10:  // Not mapped but reported.
                    break;
                default:
                    logi("OUYA: Unsupported usage page=0x%04x, page=0x%04x, value=0x%04x\n", usage_page, usage, value);
                    break;
            }
            break;
        case 0xff00:  // OUYA specific, but not mapped apparently
            break;
        default:
            logi("OUYA: Unsupported usage page=0x%04x, page=0x%04x, value=0x%04x\n", usage_page, usage, value);
            break;
    }
}

void uni_hid_parser_ouya_set_player_leds(uni_hid_device_t* d, uint8_t leds) {
#if 0
  const uint8_t report[] = {0xa2, 0x07, 0xff, 0xff, 0xff,
                            0xff, 0xff, 0xff, 0xff, 0xff};
  uni_hid_device_queue_report(d, report, sizeof(report));
#else
    ARG_UNUSED(d);
    ARG_UNUSED(leds);
#endif
}
