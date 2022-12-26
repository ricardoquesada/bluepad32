/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2019 Ricardo Quesada

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

// Generic gamepads. Gamepads that were desigened to be "multi console, and
// might implemenet usages that are invalid for specific consoles. In order to
// keep clean the pure-console implementations, add here the generic ones.

#include "uni_hid_parser_generic.h"

#include "hid_usage.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_log.h"

void uni_hid_parser_generic_init_report(uni_hid_device_t* d) {
    // Reset old state. Each report contains a full-state.
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));

    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_generic_parse_usage(uni_hid_device_t* d,
                                        hid_globals_t* globals,
                                        uint16_t usage_page,
                                        uint16_t usage,
                                        int32_t value) {
    uint8_t hat;
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
                    ctl->gamepad.axis_rx = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_RX:  // duplicate of AXIS_Z
                    ctl->gamepad.axis_rx = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_RY:  // duplicate of AXIS_RZ
                    ctl->gamepad.axis_ry = uni_hid_parser_process_pedal(globals, value);
                    break;
                case HID_USAGE_AXIS_RZ:
                    ctl->gamepad.axis_ry = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_HAT:
                    hat = uni_hid_parser_process_hat(globals, value);
                    ctl->gamepad.dpad = uni_hid_parser_hat_to_dpad(hat);
                    break;
                case HID_USAGE_DPAD_UP:
                case HID_USAGE_DPAD_DOWN:
                case HID_USAGE_DPAD_RIGHT:
                case HID_USAGE_DPAD_LEFT:
                    uni_hid_parser_process_dpad(usage, value, &ctl->gamepad.dpad);
                    break;
                default:
                    logi("Generic: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        case HID_USAGE_PAGE_SIMULATION_CONTROLS:
            switch (usage) {
                case HID_USAGE_ACCELERATOR:
                    ctl->gamepad.throttle = uni_hid_parser_process_pedal(globals, value);
                    break;
                case HID_USAGE_BRAKE:
                    ctl->gamepad.brake = uni_hid_parser_process_pedal(globals, value);
                    break;
                default:
                    logi("Generic: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            };
            break;
        case HID_USAGE_PAGE_KEYBOARD_KEYPAD:
            switch (usage) {
                case 0:
                    break;
                case 0x28:  // keyboard return
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
                    break;
                case 0xe0:  // keyboard left control
                case 0xe1:  // keyboard left shift
                case 0xe2:  // keyboard left alt
                case 0xe3:  // keyboard left GUI
                case 0xe4:  // keyboard right control
                case 0xe5:  // keyboard right shift
                case 0xe6:  // keyboard right alt
                case 0xe7:  // keyboard right GUI
                    break;
                default:
                    logi("Generic: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        case HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS:
            switch (usage) {
                case HID_USAGE_BATTERY_STRENGTH:
                    ctl->battery = value;
                    break;
                default:
                    logi("Generic: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        case HID_USAGE_PAGE_BUTTON: {
            switch (usage) {
                case 0x01:  // Button A
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_A;
                    break;
                case 0x02:  // Button B
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_B;
                    break;
                case 0x03:  // non-existant button C?
                    // unmapped
                    break;
                case 0x04:  // Button X
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_X;
                    break;
                case 0x05:  // Button Y
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_Y;
                    break;
                case 0x06:  // non-existant button Z?
                    // unmapped
                    break;
                case 0x07:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_SHOULDER_L;
                    break;
                case 0x08:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_SHOULDER_R;
                    break;
                case 0x09:  // ???
                case 0x0a:  // ???
                    break;
                case 0x0b:  // select button ?
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_BACK;
                    break;
                case 0x0c:  // start button ?
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_HOME;
                    break;
                case 0x0d:
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
                    break;
                case 0x0e:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_THUMB_L;
                    break;
                case 0x0f:
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_THUMB_R;
                    break;
                case 0x10:  // unsupported
                    break;
                default:
                    logi("Generic: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }
        case HID_USAGE_PAGE_CONSUMER:
            switch (usage) {
                case HID_USAGE_FAST_FORWARD:
                    break;
                case HID_USAGE_REWIND:
                    break;
                case HID_USAGE_PLAY_PAUSE:
                    break;
                case HID_USAGE_AC_SEARCH:
                    break;
                case HID_USAGE_AC_HOME:
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_HOME;
                    break;
                case HID_USAGE_AC_BACK:
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_BACK;
                    break;
                default:
                    logi("Generic: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        // unknown usage page
        default:
            logi("Generic: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
            break;
    }
}
