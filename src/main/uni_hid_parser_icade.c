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
#include "uni_debug.h"
#include "uni_hid_device.h"

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
enum {
  ICADE_CABINET,
  ICADE_8BITTY,
};

void uni_hid_parser_icade_setup(uni_hid_device_t* d) {
  if (d->vendor_id == 0x15e4 && d->product_id) {
    logi("Device detected as iCade Cabinet\n");
    d->data[0] = ICADE_CABINET;
  } else if (d->vendor_id == 0x0a5c && d->product_id == 0x8502) {
    logi("Device detected as iCade 8-Bitty\n");
    d->data[0] = ICADE_8BITTY;
  } else {
    logi("Unknown iCade device: v_id=0x%02x, p_id=0x%02x, File a bug.\n",
         d->vendor_id, d->product_id);
  }
}

void uni_hid_parser_icade_parse_usage(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page, uint16_t usage,
                                      int32_t value) {
  uni_gamepad_t* gp = &d->gamepad;
  UNUSED(globals);
  switch (usage_page) {
    case HID_USAGE_PAGE_KEYBOARD_KEYPAD:
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
          gp->dpad |= DPAD_UP;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case 0x08:  // e (up off)
          gp->dpad &= ~DPAD_UP;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case 0x07:  // d (right on)
          gp->dpad |= DPAD_RIGHT;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case 0x06:  // c (right off)
          gp->dpad &= ~DPAD_RIGHT;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case 0x1b:  // x (down on)
          gp->dpad |= DPAD_DOWN;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case 0x1d:  // z (down off)
          gp->dpad &= ~DPAD_DOWN;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case 0x04:  // a (left on)
          gp->dpad |= DPAD_LEFT;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case 0x14:  // q (left off)
          gp->dpad &= ~DPAD_LEFT;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;

        case 0x1c:  // y (button A / "select": on)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->buttons |= BUTTON_A;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_A;
          } else {
            // 8-Bitty.
            gp->misc_buttons |= MISC_BUTTON_SYSTEM;
            gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
          }
          break;
        case 0x17:  // t (button A / "select": off)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->buttons &= ~BUTTON_A;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_A;
          } else {
            // 8-Bitty.
            gp->misc_buttons &= ~MISC_BUTTON_SYSTEM;
            gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
          }
          break;

        case 0x0b:  // h (button B / shoulder-left: on)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->buttons |= BUTTON_B;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_B;
          } else {
            // 8-bitty.
            gp->buttons |= BUTTON_SHOULDER_L;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_L;
          }
          break;
        case 0x15:  // r (button B, shoulder-left: off)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->buttons &= ~BUTTON_B;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_B;
          } else {
            // 8-bitty.
            gp->buttons &= ~BUTTON_SHOULDER_L;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_L;
          }
          break;

        case 0x18:  // u (button C / "start": on)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->buttons |= BUTTON_X;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_X;
          } else {
            // 8-bitty.
            gp->misc_buttons |= MISC_BUTTON_HOME;
            gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_HOME;
          }
          break;
        case 0x09:  // f (button C / "start": off)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->buttons &= ~BUTTON_X;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_X;
          } else {
            // 8-bitty.
            gp->misc_buttons &= ~MISC_BUTTON_HOME;
            gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_HOME;
          }
          break;

        case 0x0d:  // j (button X / shoulder right: on)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->buttons |= BUTTON_Y;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_Y;
          } else {
            // 8-bitty.
            gp->buttons |= BUTTON_SHOULDER_R;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_R;
          }
          break;
        case 0x11:  // n (button X / shoulder right: off)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->buttons &= ~BUTTON_Y;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_Y;
          } else {
            // 8-bitty.
            gp->buttons &= ~BUTTON_SHOULDER_R;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_R;
          }
          break;

        case 0x12:  // o (button L / Y on)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->misc_buttons |= MISC_BUTTON_SYSTEM;
            gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
          } else {
            // 8-bitty.
            gp->buttons |= BUTTON_Y;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_Y;
          }
          break;
        case 0x0a:  // g (button L / Y: off)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->misc_buttons &= ~MISC_BUTTON_SYSTEM;
            gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
          } else {
            // 8-bitty.
            gp->buttons &= ~BUTTON_Y;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_Y;
          }
          break;

        case 0x0c:  // i (button Y / X: on)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->misc_buttons |= MISC_BUTTON_HOME;
            gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_HOME;
          } else {
            // 8-bitty.
            gp->buttons |= BUTTON_X;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_X;
          }
          break;
        case 0x10:  // m (button Y / X: off)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->misc_buttons &= ~MISC_BUTTON_HOME;
            gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_HOME;
          } else {
            // 8-bitty.
            gp->buttons &= ~BUTTON_X;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_X;
          }
          break;

        case 0x0e:  // k (button  Z / A: on)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->buttons |= BUTTON_SHOULDER_L;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_L;
          } else {
            // 8-bitty.
            gp->buttons |= BUTTON_A;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_A;
          }
          break;
        case 0x13:  // p (button Z / A: off)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->buttons &= ~BUTTON_SHOULDER_L;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_L;
          } else {
            // 8-bitty.
            gp->buttons &= ~BUTTON_A;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_A;
          }
          break;

        case 0x0f:  // l (button R / B: on)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->buttons |= BUTTON_SHOULDER_R;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_R;
          } else {
            // 8-bitty.
            gp->buttons |= BUTTON_B;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_B;
          }
          break;
        case 0x19:  // v (button R / B: off)
          if (d->data[0] == ICADE_CABINET) {
            // Cabinet.
            gp->buttons &= ~BUTTON_SHOULDER_R;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_R;
          } else {
            // 8-bitty.
            gp->buttons &= ~BUTTON_B;
            gp->updated_states |= GAMEPAD_STATE_BUTTON_B;
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
}