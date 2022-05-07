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
#include "uni_debug.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"

void uni_hid_parser_mouse_init_report(uni_hid_device_t* d) {
    // Reset old state. Each report contains a full-state.
    uni_gamepad_t* gp = &d->gamepad;
    memset(gp, 0, sizeof(*gp));
    gp->updated_states = GAMEPAD_STATE_AXIS_X | GAMEPAD_STATE_AXIS_Y | GAMEPAD_STATE_BUTTON_A | GAMEPAD_STATE_BUTTON_B |
                         GAMEPAD_STATE_BUTTON_X | GAMEPAD_STATE_BUTTON_Y;
}

void uni_hid_parser_mouse_parse_usage(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page,
                                      uint16_t usage,
                                      int32_t value) {
    // print_parser_globals(globals);

    // TODO: should be a union of gamepad/mouse/keyboard
    uni_gamepad_t* gp = &d->gamepad;
    switch (usage_page) {
        case HID_USAGE_PAGE_GENERIC_DESKTOP: {
            switch (usage) {
                case HID_USAGE_AXIS_X:
                    // Mouse delta X
                    gp->axis_x = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_Y:
                    // Mouse delta Y
                    gp->axis_y = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_WHEEL:
                    // TODO: do something
                    break;
                default:
                    logi("Mouse: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }

        case HID_USAGE_PAGE_BUTTON: {
            switch (usage) {
                case 0x01:  // Left click
                    if (value)
                        gp->buttons |= BUTTON_A;
                    break;
                case 0x02:  // Right click
                    if (value)
                        gp->buttons |= BUTTON_B;
                    break;
                case 0x03:  // Middle click
                    if (value)
                        gp->buttons |= BUTTON_X;
                    break;
                case 0x04:  // Back button
                    if (value)
                        gp->buttons |= BUTTON_SHOULDER_L;
                    break;
                case 0x05:  // Forward button
                    if (value)
                        gp->buttons |= BUTTON_SHOULDER_R;
                    break;
                default:
                    logi("Mouse: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }

        case HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS: {
            switch (usage) {
                case HID_USAGE_BATTERY_STRENGTH:
                    logd("Mouse: Battery strength: %d\n", value);
                    gp->battery = value;
                    break;
                default:
                    logi("Mouse: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }

        // unknown usage page
        default:
            logi("Mouse: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
            break;
    }
}