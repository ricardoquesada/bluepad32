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

#include "uni_hid_parser_icade.h"

#include "hid_usage.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

/*
 * iCade cabinet:
 *
 *   ↑      A C Y L
 *  ← →
 *   ↓      B X Z R
 *
 *
 *  UP ON,OFF  = w,e
 *  RT ON,OFF  = d,c
 *  DN ON,OFF  = x,z
 *  LT ON,OFF  = a,q
 *  A  ON,OFF  = y,t        : Mapped to Button A
 *  B  ON,OFF  = h,r        : Mapped to Button B
 *  C  ON,OFF  = u,f        : Mapped to Button X
 *  X  ON,OFF  = j,n        : Mapped to Button Y
 *  Y  ON,OFF  = i,m        : Mapped to "Home" button (debug)
 *  Z  ON,OFF  = k,p        : Mapped to Shoulder L
 *  L  ON,OFF  = o,g        : Mapped to "System" button (swap)
 *  R  ON,OFF  = l,v        : Mapped to Shoulder R
 */

/*
 * iCade 8-bitty (Thanks Paul Rickards)
 *
 * D-pap (same as Cabinet):
 *  UP ON,OFF  = w,e
 *  RT ON,OFF  = d,c
 *  DN ON,OFF  = x,z
 *  LT ON,OFF  = a,q
 *
 * Shoulder:
 *  Left  ON,OFF  = h,r         : Mapped to Shoulder Left
 *  Right ON,OFF  = j,n         : Mapped to Shoulder Right
 *
 * Face Buttons
 *  Upper Left  ON,OFF  = i,m   : Mapped to Button X
 *  Upper Right ON,OFF  = o,g   : Mapped to Button Y
 *  Lower Left  ON,OFF  = k,p   : Mapped to button A
 *  Lower Right ON,OFF  = l,v   : Mapped to button B
 *
 * Center "select/start" buttons
 *  Select ON,OFF  = y,t        : Mapped to "System" button (swap)
 *  Start  ON,OFF  = u,f        : Mapped to "Home" button (debug)
 */

// Different types of iCade devices. Mappings are slightly different.
typedef enum {
    ICADE_CABINET,
    ICADE_8BITTY,
} icade_model_t;

// icade_instance_t represents data used by the iCade driver instance.
typedef struct icade_instance_s {
    icade_model_t model;  // ICADE_CABINET or ICADE_8BITTY
} icade_instance_t;
_Static_assert(sizeof(icade_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "iCade intance too big");

static icade_instance_t* get_icade_instance(uni_hid_device_t* d);

void uni_hid_parser_icade_setup(uni_hid_device_t* d) {
    icade_instance_t* ins = get_icade_instance(d);
    if (d->vendor_id == 0x15e4 && d->product_id) {
        logi("Device detected as iCade Cabinet\n");
        ins->model = ICADE_CABINET;
    } else if (d->vendor_id == 0x0a5c && d->product_id == 0x8502) {
        logi("Device detected as iCade 8-Bitty\n");
        ins->model = ICADE_8BITTY;
    } else {
        logi("Unknown iCade device: v_id=0x%02x, p_id=0x%02x, File a bug.\n", d->vendor_id, d->product_id);
    }

    uni_hid_device_set_ready_complete(d);
}

void uni_hid_parser_icade_parse_usage(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page,
                                      uint16_t usage,
                                      int32_t value) {
    uni_controller_t* ctl = &d->controller;
    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
    ctl->battery = UNI_CONTROLLER_BATTERY_NOT_AVAILABLE;

    icade_instance_t* ins = get_icade_instance(d);
    ARG_UNUSED(globals);
    if (usage_page != HID_USAGE_PAGE_KEYBOARD_KEYPAD)
        return;

    switch (usage) {
        case 0x00:  // reserved. ignore
        case 0xe0:  // from 0xe0 - 0xe7: ignore
        case 0xe1:
        case 0xe2:
        case 0xe3:
        case 0xe4:
        case 0xe5:
        case 0xe6:
        case 0xe7:
            break;
        case 0x1a:  // w (up on)
            ctl->gamepad.dpad |= DPAD_UP;
            break;
        case 0x08:  // e (up off)
            ctl->gamepad.dpad &= ~DPAD_UP;
            break;
        case 0x07:  // d (right on)
            ctl->gamepad.dpad |= DPAD_RIGHT;
            break;
        case 0x06:  // c (right off)
            ctl->gamepad.dpad &= ~DPAD_RIGHT;
            break;
        case 0x1b:  // x (down on)
            ctl->gamepad.dpad |= DPAD_DOWN;
            break;
        case 0x1d:  // z (down off)
            ctl->gamepad.dpad &= ~DPAD_DOWN;
            break;
        case 0x04:  // a (left on)
            ctl->gamepad.dpad |= DPAD_LEFT;
            break;
        case 0x14:  // q (left off)
            ctl->gamepad.dpad &= ~DPAD_LEFT;
            break;

        case 0x1c:  // y (button A / "select": on)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.buttons |= BUTTON_A;
            } else {
                // 8-Bitty.
                ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
            }
            break;
        case 0x17:  // t (button A / "select": off)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.buttons &= ~BUTTON_A;
            } else {
                // 8-Bitty.
                ctl->gamepad.misc_buttons &= ~MISC_BUTTON_SYSTEM;
            }
            break;

        case 0x0b:  // h (button B / shoulder-left: on)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.buttons |= BUTTON_B;
            } else {
                // 8-bitty.
                ctl->gamepad.buttons |= BUTTON_SHOULDER_L;
            }
            break;
        case 0x15:  // r (button B, shoulder-left: off)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.buttons &= ~BUTTON_B;
            } else {
                // 8-bitty.
                ctl->gamepad.buttons &= ~BUTTON_SHOULDER_L;
            }
            break;

        case 0x18:  // u (button C / "start": on)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.buttons |= BUTTON_X;
            } else {
                // 8-bitty.
                ctl->gamepad.misc_buttons |= MISC_BUTTON_HOME;
            }
            break;
        case 0x09:  // f (button C / "start": off)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.buttons &= ~BUTTON_X;
            } else {
                // 8-bitty.
                ctl->gamepad.misc_buttons &= ~MISC_BUTTON_HOME;
            }
            break;

        case 0x0d:  // j (button X / shoulder right: on)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.buttons |= BUTTON_Y;
            } else {
                // 8-bitty.
                ctl->gamepad.buttons |= BUTTON_SHOULDER_R;
            }
            break;
        case 0x11:  // n (button X / shoulder right: off)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.buttons &= ~BUTTON_Y;
            } else {
                // 8-bitty.
                ctl->gamepad.buttons &= ~BUTTON_SHOULDER_R;
            }
            break;

        case 0x12:  // o (button L / Y on)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
            } else {
                // 8-bitty.
                ctl->gamepad.buttons |= BUTTON_Y;
            }
            break;
        case 0x0a:  // g (button L / Y: off)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.misc_buttons &= ~MISC_BUTTON_SYSTEM;
            } else {
                // 8-bitty.
                ctl->gamepad.buttons &= ~BUTTON_Y;
            }
            break;

        case 0x0c:  // i (button Y / X: on)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.misc_buttons |= MISC_BUTTON_HOME;
            } else {
                // 8-bitty.
                ctl->gamepad.buttons |= BUTTON_X;
            }
            break;
        case 0x10:  // m (button Y / X: off)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.misc_buttons &= ~MISC_BUTTON_HOME;
            } else {
                // 8-bitty.
                ctl->gamepad.buttons &= ~BUTTON_X;
            }
            break;

        case 0x0e:  // k (button  Z / A: on)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.buttons |= BUTTON_SHOULDER_L;
            } else {
                // 8-bitty.
                ctl->gamepad.buttons |= BUTTON_A;
            }
            break;
        case 0x13:  // p (button Z / A: off)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.buttons &= ~BUTTON_SHOULDER_L;
            } else {
                // 8-bitty.
                ctl->gamepad.buttons &= ~BUTTON_A;
            }
            break;

        case 0x0f:  // l (button R / B: on)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.buttons |= BUTTON_SHOULDER_R;
            } else {
                // 8-bitty.
                ctl->gamepad.buttons |= BUTTON_B;
            }
            break;
        case 0x19:  // v (button R / B: off)
            if (ins->model == ICADE_CABINET) {
                // Cabinet.
                ctl->gamepad.buttons &= ~BUTTON_SHOULDER_R;
            } else {
                // 8-bitty.
                ctl->gamepad.buttons &= ~BUTTON_B;
            }
            break;

        default:
            logi(
                "iCade: Unsupported page: 0x%04x, usage: 0x%04x, "
                "value=0x%x\n",
                usage_page, usage, value);
            break;
    }
}

//
// Helpers
//
static icade_instance_t* get_icade_instance(uni_hid_device_t* d) {
    return (icade_instance_t*)&d->parser_data[0];
}
