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

// For more info about Android mappings see:
// https://developer.android.com/training/game-controllers/controller-input

#include "uni_hid_parser_smarttvremote.h"

#include "hid_usage.h"
#include "uni_debug.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"

void uni_hid_parser_smarttvremote_init_report(uni_hid_device_t* d) {
  // Reset old state. Each report contains a full-state.
  d->gamepad.updated_states = 0;
}
void uni_hid_parser_smarttvremote_parse_usage(uni_hid_device_t* d,
                                              hid_globals_t* globals,
                                              uint16_t usage_page,
                                              uint16_t usage, int32_t value) {
  UNUSED(globals);
  uni_gamepad_t* gp = &d->gamepad;
  // print_parser_globals(globals);
  switch (usage_page) {
    case HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS:
      switch (usage) {
        case HID_USAGE_BATTERY_STRENGHT:
          gp->battery = value;
          break;
        default:
          logi(
              "SmartTVRemote: Unsupported page: 0x%04x, usage: 0x%04x, "
              "value=0x%x\n",
              usage_page, usage, value);
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
            gp->dpad |= DPAD_RIGHT;
          else
            gp->dpad &= ~DPAD_RIGHT;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case HID_USAGE_KB_LEFT_ARROW:
          if (value)
            gp->dpad |= DPAD_LEFT;
          else
            gp->dpad &= ~DPAD_LEFT;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case HID_USAGE_KB_DOWN_ARROW:
          if (value)
            gp->dpad |= DPAD_DOWN;
          else
            gp->dpad &= ~DPAD_DOWN;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case HID_USAGE_KB_UP_ARROW:
          if (value)
            gp->dpad |= DPAD_UP;
          else
            gp->dpad &= ~DPAD_UP;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case HID_USAGE_KP_ENTER:
          if (value)
            gp->buttons |= BUTTON_A;
          else
            gp->buttons &= ~BUTTON_A;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_A;
          break;
        case HID_USAGE_KB_POWER:
          break;                        // unmapped apparently
        case HID_USAGE_KB_RESERVED_F1:  // Back button (reserved)
          if (value)
            gp->misc_buttons |= MISC_BUTTON_BACK;
          else
            gp->misc_buttons &= ~MISC_BUTTON_BACK;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_BACK;
          break;
        default:
          logi(
              "SmartTVRemote: Unsupported page: 0x%04x, usage: 0x%04x, "
              "value=0x%x\n",
              usage_page, usage, value);
          break;
      }
      break;
    case HID_USAGE_PAGE_CONSUMER: {
      switch (usage) {
        case HID_USAGE_MENU:
          if (value)
            gp->misc_buttons |= MISC_BUTTON_SYSTEM;
          else
            gp->misc_buttons &= ~MISC_BUTTON_SYSTEM;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
          break;
        case HID_USAGE_MEDIA_SELECT_TV:
        case HID_USAGE_FAST_FORWARD:
        case HID_USAGE_REWIND:
        case HID_USAGE_PLAY_PAUSE:
        case HID_USAGE_MUTE:
        case HID_USAGE_VOLUMEN_UP:
        case HID_USAGE_VOLUMEN_DOWN:
        case HID_USAGE_AC_SEARCH:  // mic / search
          break;
        case HID_USAGE_AC_HOME:
          if (value)
            gp->misc_buttons |= MISC_BUTTON_HOME;
          else
            gp->misc_buttons &= ~MISC_BUTTON_HOME;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_HOME;
          break;
        default:
          logi(
              "SmartTVRemote: Unsupported page: 0x%04x, usage: 0x%04x, "
              "value=0x%x\n",
              usage_page, usage, value);
          break;
      }
      break;
    }
    // unknown usage page
    default:
      logi(
          "SmartTVRemote: Unsupported page: 0x%04x, usage: 0x%04x, "
          "value=0x%x\n",
          usage_page, usage, value);
      break;
  }
}