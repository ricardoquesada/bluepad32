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

#include "uni_hid_parser_ouya.h"

#include "hid_usage.h"
#include "uni_debug.h"
#include "uni_hid_device.h"

void uni_hid_parser_ouya_init_report(uni_hid_device_t* d) {
  // Reset old state. Each report contains a full-state.
  d->gamepad.updated_states = 0;
}

void uni_hid_parser_ouya_parse_usage(uni_hid_device_t* d,
                                     hid_globals_t* globals,
                                     uint16_t usage_page, uint16_t usage,
                                     int32_t value) {
  uni_gamepad_t* gp = &d->gamepad;
  switch (usage_page) {
    case HID_USAGE_PAGE_GENERIC_DESKTOP:
      switch (usage) {
        case HID_USAGE_AXIS_X:
          gp->axis_x = uni_hid_parser_process_axis(globals, value);
          gp->updated_states |= GAMEPAD_STATE_AXIS_X;
          break;
        case HID_USAGE_AXIS_Y:
          gp->axis_y = uni_hid_parser_process_axis(globals, value);
          gp->updated_states |= GAMEPAD_STATE_AXIS_Y;
          break;
        case HID_USAGE_AXIS_Z:
          gp->brake = uni_hid_parser_process_pedal(globals, value);
          gp->updated_states |= GAMEPAD_STATE_BRAKE;
          break;
        case HID_USAGE_AXIS_RX:
          gp->axis_rx = uni_hid_parser_process_axis(globals, value);
          gp->updated_states |= GAMEPAD_STATE_AXIS_RX;
          break;
        case HID_USAGE_AXIS_RY:
          gp->axis_ry = uni_hid_parser_process_axis(globals, value);
          gp->updated_states |= GAMEPAD_STATE_AXIS_RY;
          break;
        case HID_USAGE_AXIS_RZ:
          gp->accelerator = uni_hid_parser_process_pedal(globals, value);
          gp->updated_states |= GAMEPAD_STATE_ACCELERATOR;
          break;
        default:
          logi(
              "OUYA: Unsupported usage page=0x%04x, page=0x%04x, "
              "value=0x%04x\n",
              usage_page, usage, value);
          break;
      }
      break;
    case HID_USAGE_PAGE_BUTTON:
      switch (usage) {
        case 1:
          if (value)
            gp->buttons |= BUTTON_A;
          else
            gp->buttons &= ~BUTTON_A;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_A;
          break;
        case 2:
          if (value)
            gp->buttons |= BUTTON_X;
          else
            gp->buttons &= ~BUTTON_X;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_X;
          break;
        case 3:
          if (value)
            gp->buttons |= BUTTON_Y;
          else
            gp->buttons &= ~BUTTON_Y;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_Y;
          break;
        case 4:
          if (value)
            gp->buttons |= BUTTON_B;
          else
            gp->buttons &= ~BUTTON_B;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_B;
          break;
        case 5:
          if (value)
            gp->buttons |= BUTTON_SHOULDER_L;
          else
            gp->buttons &= ~BUTTON_SHOULDER_L;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_L;
          break;
        case 6:
          if (value)
            gp->buttons |= BUTTON_SHOULDER_R;
          else
            gp->buttons &= ~BUTTON_SHOULDER_R;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_R;
          break;
        case 7:
          if (value)
            gp->buttons |= BUTTON_THUMB_L;
          else
            gp->buttons &= ~BUTTON_THUMB_L;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_THUMB_L;
          break;
        case 8:
          if (value)
            gp->buttons |= BUTTON_THUMB_R;
          else
            gp->buttons &= ~BUTTON_THUMB_R;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_THUMB_R;
          break;
        case 9:
          if (value)
            gp->dpad |= DPAD_UP;
          else
            gp->dpad &= ~DPAD_UP;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case 0x0a:
          if (value)
            gp->dpad |= DPAD_DOWN;
          else
            gp->dpad &= ~DPAD_DOWN;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case 0x0b:
          if (value)
            gp->dpad |= DPAD_LEFT;
          else
            gp->dpad &= ~DPAD_LEFT;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case 0x0c:
          if (value)
            gp->dpad |= DPAD_RIGHT;
          else
            gp->dpad &= ~DPAD_RIGHT;
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case 0x0d:  // Triggered by Brake pedal
          if (value)
            gp->buttons |= BUTTON_TRIGGER_L;
          else
            gp->buttons &= ~BUTTON_TRIGGER_L;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_TRIGGER_L;
          break;
        case 0x0e:  // Triggered by Accelerator pedal
          if (value)
            gp->buttons |= BUTTON_TRIGGER_R;
          else
            gp->buttons &= ~BUTTON_TRIGGER_R;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_TRIGGER_R;
          break;
        case 0x0f:
          if (value)
            gp->misc_buttons |= MISC_BUTTON_SYSTEM;
          else
            gp->misc_buttons &= ~MISC_BUTTON_SYSTEM;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
          break;
        case 0x10:  // Not mapped but reported.
          break;
        default:
          logi(
              "OUYA: Unsupported usage page=0x%04x, page=0x%04x, "
              "value=0x%04x\n",
              usage_page, usage, value);
          break;
      }
      break;
    case 0xff00:  // OUYA specific, but not mapped apparently
      break;
    default:
      logi("OUYA: Unsupported usage page=0x%04x, page=0x%04x, value=0x%04x\n",
           usage_page, usage, value);
      break;
  }
}

void uni_hid_parser_ouya_update_led(uni_hid_device_t* d) {
#if 0
  const uint8_t report[] = {0xa2, 0x07, 0xff, 0xff, 0xff,
                            0xff, 0xff, 0xff, 0xff, 0xff};
  uni_hid_device_queue_report(d, report, sizeof(report));
#else
  UNUSED(d);
#endif
}