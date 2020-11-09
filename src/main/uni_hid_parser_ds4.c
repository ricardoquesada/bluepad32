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

// More info about DUALSHOCK 4 gamepad:
// https://manuals.playstation.net/document/en/ps4/basic/pn_controller.html

// Technical info taken from:
// https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c
// https://github.com/chrippa/ds4drv/blob/master/ds4drv/device.py

#include "uni_hid_parser_ds4.h"

#include "hid_usage.h"
#include "uni_config.h"
#include "uni_debug.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"

void uni_hid_parser_ds4_init_report(uni_hid_device_t* d) {
  // Reset old state. Each report contains a full-state.
  memset(&d->gamepad, 0, sizeof(d->gamepad));
}

void uni_hid_parser_ds4_parse_raw(uni_hid_device_t* d, const uint8_t* report,
                                  uint16_t len) {
  // printf_hexdump(report, len);
  if (report[0] != 0x11) {
    loge("DS4: Unexpected report type: got 0x%02x, want: 0x11\n", report[0]);
    return;
  }
  if (len != 78) {
    loge("DS4: Unexpected report len: got %d, want: 78\n", len);
    return;
  }
  uni_gamepad_t* gp = &d->gamepad;
  const uint8_t* data = &report[3];

  // Axis
  gp->axis_x = (data[0] - 127) * 4;
  gp->axis_y = (data[1] - 127) * 4;
  gp->axis_rx = (data[2] - 127) * 4;
  gp->axis_ry = (data[3] - 127) * 4;
  gp->updated_states |= (GAMEPAD_STATE_AXIS_X | GAMEPAD_STATE_AXIS_Y |
                         GAMEPAD_STATE_AXIS_RX | GAMEPAD_STATE_AXIS_RY);

  // Hat
  uint8_t value = data[4] & 0xf;
  if (value > 7) value = 0xff; /* Center 0, 0 */
  gp->dpad = uni_hid_parser_hat_to_dpad(value);
  gp->updated_states |= GAMEPAD_STATE_DPAD;

  // Buttons
  if (data[4] & 0x10)  // West
    gp->buttons |= BUTTON_X;
  else
    gp->buttons &= ~BUTTON_X;

  if (data[4] & 0x20)  // South
    gp->buttons |= BUTTON_A;
  else
    gp->buttons &= ~BUTTON_A;

  if (data[4] & 0x40)  // East
    gp->buttons |= BUTTON_B;
  else
    gp->buttons &= ~BUTTON_B;

  if (data[4] & 0x80)  // North
    gp->buttons |= BUTTON_Y;
  else
    gp->buttons &= ~BUTTON_Y;
  gp->updated_states |= GAMEPAD_STATE_BUTTON_X | GAMEPAD_STATE_BUTTON_Y |
                        GAMEPAD_STATE_BUTTON_A | GAMEPAD_STATE_BUTTON_B;

  if (data[5] & 0x01)  // Shoulder L
    gp->buttons |= BUTTON_SHOULDER_L;
  else
    gp->buttons &= ~BUTTON_SHOULDER_L;

  if (data[5] & 0x02)  // Shoulder R
    gp->buttons |= BUTTON_SHOULDER_R;
  else
    gp->buttons &= ~BUTTON_SHOULDER_R;

  if (data[5] & 0x04)  // Trigger L
    gp->buttons |= BUTTON_TRIGGER_L;
  else
    gp->buttons &= ~BUTTON_TRIGGER_L;

  if (data[5] & 0x08)  // Trigger R
    gp->buttons |= BUTTON_TRIGGER_R;
  else
    gp->buttons &= ~BUTTON_TRIGGER_R;
  gp->updated_states |=
      GAMEPAD_STATE_BUTTON_TRIGGER_L | GAMEPAD_STATE_BUTTON_TRIGGER_R |
      GAMEPAD_STATE_BUTTON_SHOULDER_L | GAMEPAD_STATE_BUTTON_SHOULDER_R;

  if (data[5] & 0x10)  // Share
    gp->misc_buttons |= MISC_BUTTON_BACK;
  else
    gp->misc_buttons &= ~MISC_BUTTON_BACK;

  if (data[5] & 0x20)  // Options
    gp->misc_buttons |= MISC_BUTTON_HOME;
  else
    gp->misc_buttons &= ~MISC_BUTTON_HOME;
  gp->updated_states |=
      GAMEPAD_STATE_MISC_BUTTON_BACK | GAMEPAD_STATE_MISC_BUTTON_HOME;

  if (data[5] & 0x40)  // Thumb L
    gp->buttons |= BUTTON_THUMB_L;
  else
    gp->buttons &= ~BUTTON_THUMB_L;

  if (data[5] & 0x80)  // Thumb R
    gp->buttons |= BUTTON_THUMB_R;
  else
    gp->buttons &= ~BUTTON_THUMB_R;
  gp->updated_states |=
      GAMEPAD_STATE_BUTTON_THUMB_L | GAMEPAD_STATE_BUTTON_THUMB_R;

  if (data[6] & 0x01)  // PlayStation button
    gp->misc_buttons |= MISC_BUTTON_SYSTEM;
  else
    gp->misc_buttons &= ~MISC_BUTTON_SYSTEM;
  gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM;

  gp->brake = data[7] * 4;
  gp->accelerator = data[8] * 4;
  gp->updated_states |= GAMEPAD_STATE_BRAKE | GAMEPAD_STATE_ACCELERATOR;
}

void uni_hid_parser_ds4_parse_usage(uni_hid_device_t* d, hid_globals_t* globals,
                                    uint16_t usage_page, uint16_t usage,
                                    int32_t value) {
  // print_parser_globals(globals);
  uint8_t hat;
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
          gp->axis_rx = uni_hid_parser_process_axis(globals, value);
          gp->updated_states |= GAMEPAD_STATE_AXIS_RX;
          break;
        case HID_USAGE_AXIS_RX:
          gp->brake = uni_hid_parser_process_pedal(globals, value);
          gp->updated_states |= GAMEPAD_STATE_BRAKE;
          break;
        case HID_USAGE_AXIS_RY:
          gp->accelerator = uni_hid_parser_process_pedal(globals, value);
          gp->updated_states |= GAMEPAD_STATE_ACCELERATOR;
          break;
        case HID_USAGE_AXIS_RZ:
          gp->axis_ry = uni_hid_parser_process_axis(globals, value);
          gp->updated_states |= GAMEPAD_STATE_AXIS_RY;
          break;
        case HID_USAGE_HAT:
          hat = uni_hid_parser_process_hat(globals, value);
          gp->dpad = uni_hid_parser_hat_to_dpad(hat);
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        case HID_USAGE_SYSTEM_MAIN_MENU:
          if (value)
            gp->misc_buttons |= MISC_BUTTON_SYSTEM;
          else
            gp->misc_buttons &= ~MISC_BUTTON_SYSTEM;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
          break;
        case HID_USAGE_DPAD_UP:
        case HID_USAGE_DPAD_DOWN:
        case HID_USAGE_DPAD_RIGHT:
        case HID_USAGE_DPAD_LEFT:
          uni_hid_parser_process_dpad(usage, value, &gp->dpad);
          gp->updated_states |= GAMEPAD_STATE_DPAD;
          break;
        default:
          logi("DS4: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n",
               usage_page, usage, value);
          break;
      }
      break;
    case HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS:
      switch (usage) {
        case HID_USAGE_BATTERY_STRENGHT:
          gp->battery = value;
          break;
        default:
          logi("DS4: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n",
               usage_page, usage, value);
          break;
      }
      break;

    case HID_USAGE_PAGE_BUTTON: {
      switch (usage) {
        case 0x01:  // Square Button (0x01)
          if (value)
            gp->buttons |= BUTTON_X;
          else
            gp->buttons &= ~BUTTON_X;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_X;
          break;
        case 0x02:  // X Button (0x02)
          if (value)
            gp->buttons |= BUTTON_A;
          else
            gp->buttons &= ~BUTTON_A;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_A;
          break;
        case 0x03:  // Circle Button (0x04)
          if (value)
            gp->buttons |= BUTTON_B;
          else
            gp->buttons &= ~BUTTON_B;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_B;
          break;
        case 0x04:  // Triangle Button (0x08)
          if (value)
            gp->buttons |= BUTTON_Y;
          else
            gp->buttons &= ~BUTTON_Y;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_Y;
          break;
        case 0x05:  // Shoulder Left (0x10)
          if (value)
            gp->buttons |= BUTTON_SHOULDER_L;
          else
            gp->buttons &= ~BUTTON_SHOULDER_L;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_L;
          break;
        case 0x06:  // Shoulder Right (0x20)
          if (value)
            gp->buttons |= BUTTON_SHOULDER_R;
          else
            gp->buttons &= ~BUTTON_SHOULDER_R;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_R;
          break;
        case 0x07:  // Trigger L (0x40)
          if (value)
            gp->buttons |= BUTTON_TRIGGER_L;
          else
            gp->buttons &= ~BUTTON_TRIGGER_L;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_TRIGGER_L;
          break;
        case 0x08:  // Trigger R (0x80)
          if (value)
            gp->buttons |= BUTTON_TRIGGER_R;
          else
            gp->buttons &= ~BUTTON_TRIGGER_R;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_TRIGGER_R;
          break;
        case 0x09:  // Share (0x100)
          if (value)
            gp->misc_buttons |= MISC_BUTTON_BACK;
          else
            gp->misc_buttons &= ~MISC_BUTTON_BACK;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_BACK;
          break;
        case 0x0a:  // options button (0x200)
          if (value)
            gp->misc_buttons |= MISC_BUTTON_HOME;
          else
            gp->misc_buttons &= ~MISC_BUTTON_HOME;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_HOME;
          break;
        case 0x0b:  // thumb L (0x400)
          if (value)
            gp->buttons |= BUTTON_THUMB_L;
          else
            gp->buttons &= ~BUTTON_THUMB_L;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_THUMB_L;
          break;
        case 0x0c:  // thumb R (0x800)
          if (value)
            gp->buttons |= BUTTON_THUMB_R;
          else
            gp->buttons &= ~BUTTON_THUMB_R;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_THUMB_R;
          break;
        case 0x0d:  // ps button (0x1000)
          if (value)
            gp->misc_buttons |= MISC_BUTTON_SYSTEM;
          else
            gp->misc_buttons &= ~MISC_BUTTON_SYSTEM;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
          break;
        case 0x0e:  // touch pad button (0x2000)
          // unassigned
          break;
        default:
          logi("DS4: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n",
               usage_page, usage, value);
          break;
      }
      break;
    }
    // unknown usage page
    default:
      logi("DS4: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n",
           usage_page, usage, value);
      break;
  }
}

#define CRCPOLY 0xedb88320
static uint32_t crc32_le(uint32_t seed, const void* data, size_t len) {
  uint32_t crc = seed;
  const uint8_t* src = data;
  uint32_t mult;
  int i;

  while (len--) {
    crc ^= *src++;
    for (i = 0; i < 8; i++) {
      mult = (crc & 1) ? CRCPOLY : 0;
      crc = (crc >> 1) ^ mult;
    }
  }

  return crc;
}

void uni_hid_parser_ds4_update_led(uni_hid_device_t* d) {
#if UNI_USE_DUALSHOCK4_REPORT_0x11
  // Force feedback info taken from:
  //
  struct ff_report {
    // Report related
    uint8_t transaction_type;  // type of transaction
    uint8_t report_id;         // report Id
    // Data related
    uint8_t unk0[5];
    uint8_t rumble_left;
    uint8_t rumble_right;
    uint8_t led_red;
    uint8_t led_green;
    uint8_t led_blue;
    uint8_t flash_led1;  // time to flash bright (255 = 2.5 seconds)
    uint8_t flash_led2;  // time to flash dark (255 = 2.5 seconds)
    uint8_t unk1[61];
    uint32_t crc;
  } __attribute__((__packed__));

  struct ff_report ff;
  memset(&ff, 0, sizeof(ff));
  ff.transaction_type = 0xa2;  // DATA | TYPE_OUTPUT
  ff.report_id = 0x11;         // taken from HID descriptor
  ff.unk0[0] = 0xc4;           // HID alone + poll interval
  ff.unk0[2] = 0x7;            // blink + LED + motor
  ff.rumble_left = 0x00;
  ff.rumble_right = 0x00;
  ff.led_red = (d->joystick_port & JOYSTICK_PORT_B) ? 0x30 : 0x00;
  ff.led_green = (d->joystick_port & JOYSTICK_PORT_A) ? 0x30 : 0x00;
  ff.led_blue = 0x00;
  ff.flash_led1 = 0x0;
  ff.flash_led2 = 0x0;

  /* CRC generation */
  uint8_t bthdr = 0xA2;
  uint32_t crc;

  crc = crc32_le(0xffffffff, &bthdr, 1);
  crc = ~crc32_le(crc, (uint8_t*)&ff.report_id, sizeof(ff) - 5);
  ff.crc = crc;

  uni_hid_device_send_intr_report(d, (uint8_t*)&ff, sizeof(ff));
#else
  UNUSED(d);
#endif
}
