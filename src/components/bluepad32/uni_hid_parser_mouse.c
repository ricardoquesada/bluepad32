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

#include "uni_hid_parser_mouse.h"

#include "hid_usage.h"
#include "uni_debug.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"

typedef struct {
    int multiplier;
    int divisor;
} mouse_instance_t;
_Static_assert(sizeof(mouse_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "Mouse intance too big");

struct mouse_resolution {
    uint16_t vid;
    uint16_t pid;
    char* name;
    int multiplier;
    int divisor;
};

static mouse_instance_t* get_mouse_instance(uni_hid_device_t* d) {
    return (mouse_instance_t*)&d->parser_data[0];
}

// TL;DR: Mouse reports are hit and miss.
// Don't try to be clever: just use the "delta" multiplied by some constant (defined per mouse).
// Long:
// "Unitless" reports have no sense. How many "deltas" should I move?
// "Unit"-oriented reports (like Apple Magic Mouse), are extremely complex to calculate.
// Should I really do a complicated math in order to translate how many inches the mouse move and
// then try to map that to the Amiga? Error-prone.
//
// Easy-and-hackish wat to solve:
// Since we only support a handful of mice, harcode the resolutions.
// This works only becuase we support a few mice. In the future we might need to do
// "smart" way to solve it... or just allow the user change the resolution via BLE application.
static const struct mouse_resolution resolutions[] = {
    // Apple Magic Mouse 1st gen
    {0x05ac, 0x030d, "Admin Mouse", 1, 4},
    // TECKNET 2600DPI. Has a "DPI" button. The faster one is unusable. Make the faster usable.
    {0x0a5c, 0x4503, "BM30X mouse", 1, 3},

    // No need to add entries where mult & div are both 1
    /*
    {0x0a5c, 0x0001, "BORND Bluetooth Mouse", 1, 1},  // Bornd C170B
    */
};

static int32_t process_mouse_delta(uni_hid_device_t* d, int32_t value) {
    mouse_instance_t* ins = get_mouse_instance(d);

    int mult = ins->multiplier;
    int div = ins->divisor;

    if (mult != 1)
        value *= mult;
    if (div != 1 && div != 0)
        value /= div;

    // Quadrature-driver expect values between -127 and 127.
    if (value < -110)
        value = -110;
    if (value > 110)
        value = 110;

    return value;
}

void uni_hid_parser_mouse_setup(uni_hid_device_t* d) {
    // At setup time, fetch the "multiplier" / "divisor" for the mouse.
    mouse_instance_t* ins;
    int mult = 1;
    int div = 1;

    ins = get_mouse_instance(d);

    for (unsigned int i = 0; i < ARRAY_SIZE(resolutions); i++) {
        if (resolutions[i].vid == d->vendor_id && resolutions[i].pid == d->product_id &&
            strcmp(resolutions[i].name, d->name) == 0) {
            mult = resolutions[i].multiplier;
            div = resolutions[i].divisor;
            break;
        }
    }

    ins->multiplier = mult;
    ins->divisor = div;
}

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
    UNUSED(globals);
    // TODO: should be a union of gamepad/mouse/keyboard
    uni_gamepad_t* gp = &d->gamepad;
    switch (usage_page) {
        case HID_USAGE_PAGE_GENERIC_DESKTOP: {
            switch (usage) {
                case HID_USAGE_AXIS_X:
                    // Mouse delta X
                    gp->axis_x = process_mouse_delta(d, value);
                    break;
                case HID_USAGE_AXIS_Y:
                    // Mouse delta Y
                    gp->axis_y = process_mouse_delta(d, value);
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