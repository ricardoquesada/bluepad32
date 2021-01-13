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

#include <assert.h>

#include "hid_usage.h"
#include "uni_config.h"
#include "uni_debug.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_utils.h"

typedef struct {
  // Must be first element
  btstack_timer_source_t ts;
  uni_hid_device_t* hid_device;
  bool rumble_in_progress;
} ds4_instance_t;

typedef struct __attribute((packed)) {
  // Report related
  uint8_t transaction_type;  // type of transaction
  uint8_t report_id;         // report Id
  // Data related
  uint8_t unk0[2];
  uint8_t flags;
  uint8_t unk1[2];
  uint8_t motor_right;
  uint8_t motor_left;
  uint8_t led_red;
  uint8_t led_green;
  uint8_t led_blue;
  uint8_t flash_led1;  // time to flash bright (255 = 2.5 seconds)
  uint8_t flash_led2;  // time to flash dark (255 = 2.5 seconds)
  uint8_t unk2[61];
  uint32_t crc;
} ds4_output_report_t;

// When sending the FF report, which "features" should be set.
enum {
  DS4_FF_FLAG_RUMBLE = 1 << 0,
  DS4_FF_FLAG_LED_COLOR = 1 << 1,
  DS4_FF_FLAG_LED_BLINK = 1 << 2,
};

static ds4_instance_t* get_ds4_instance(uni_hid_device_t* d);
static void ds4_send_output_report(uni_hid_device_t* d,
                                   ds4_output_report_t* out);
static void ds4_set_rumble_off(btstack_timer_source_t* ts);

void uni_hid_parser_ds4_setup(struct uni_hid_device_s* d) {
  // From Linux drivers/hid/hid-sony.c:
  // The default behavior of the DUALSHOCK 4 is to send reports using
  // report type 1 when running over Bluetooth. However, when feature
  // report 2 is requested during the controller initialization it starts
  // sending input reports in report 17.

  ds4_instance_t* ins = get_ds4_instance(d);
  memset(ins, 0, sizeof(*ins));
  ins->hid_device = d;  // Used by rumble callbacks

  // Also turns off blinking, LED and rumble.
  ds4_output_report_t out = {0};

  out.flags = DS4_FF_FLAG_RUMBLE | DS4_FF_FLAG_LED_COLOR |
              DS4_FF_FLAG_LED_BLINK;  // blink + LED + motor

  // Default LED color: Blue
  out.led_red = 0x00;
  out.led_green = 0x00;
  out.led_blue = 0x40;

  ds4_send_output_report(d, &out);
}

void uni_hid_parser_ds4_init_report(uni_hid_device_t* d) {
  uni_gamepad_t* gp = &d->gamepad;
  memset(gp, 0, sizeof(*gp));

  // Only report 0x11 is supported which is a "full report". It is safe to set
  // the reported states just once, here:
  gp->updated_states = GAMEPAD_STATE_AXIS_X | GAMEPAD_STATE_AXIS_Y |
                       GAMEPAD_STATE_AXIS_RX | GAMEPAD_STATE_AXIS_RY;
  gp->updated_states |= GAMEPAD_STATE_BRAKE | GAMEPAD_STATE_ACCELERATOR;
  gp->updated_states |= GAMEPAD_STATE_DPAD;
  gp->updated_states |= GAMEPAD_STATE_BUTTON_X | GAMEPAD_STATE_BUTTON_Y |
                        GAMEPAD_STATE_BUTTON_A | GAMEPAD_STATE_BUTTON_B;
  gp->updated_states |=
      GAMEPAD_STATE_BUTTON_TRIGGER_L | GAMEPAD_STATE_BUTTON_TRIGGER_R |
      GAMEPAD_STATE_BUTTON_SHOULDER_L | GAMEPAD_STATE_BUTTON_SHOULDER_R;
  gp->updated_states |=
      GAMEPAD_STATE_BUTTON_THUMB_L | GAMEPAD_STATE_BUTTON_THUMB_R;
  gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_BACK |
                        GAMEPAD_STATE_MISC_BUTTON_HOME |
                        GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
}

void uni_hid_parser_ds4_parse_raw(uni_hid_device_t* d, const uint8_t* report,
                                  uint16_t len) {
  if (report[0] != 0x11) {
    loge("DS4: Unexpected report type: got 0x%02x, want: 0x11\n", report[0]);
    // printf_hexdump(report, len);
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

  // Brake & throttle
  gp->brake = data[7] * 4;
  gp->accelerator = data[8] * 4;

  // Hat
  uint8_t value = data[4] & 0xf;
  if (value > 7) value = 0xff; /* Center 0, 0 */
  gp->dpad = uni_hid_parser_hat_to_dpad(value);

  // Buttons
  // West
  if (data[4] & 0x10) gp->buttons |= BUTTON_X;

  // South
  if (data[4] & 0x20) gp->buttons |= BUTTON_A;

  // East
  if (data[4] & 0x40) gp->buttons |= BUTTON_B;

  // North
  if (data[4] & 0x80) gp->buttons |= BUTTON_Y;

  // Shoulder L
  if (data[5] & 0x01) gp->buttons |= BUTTON_SHOULDER_L;

  // Shoulder R
  if (data[5] & 0x02) gp->buttons |= BUTTON_SHOULDER_R;

  // Trigger L
  if (data[5] & 0x04) gp->buttons |= BUTTON_TRIGGER_L;

  // Trigger R
  if (data[5] & 0x08) gp->buttons |= BUTTON_TRIGGER_R;

  // Share
  if (data[5] & 0x10) gp->misc_buttons |= MISC_BUTTON_BACK;

  // Options
  if (data[5] & 0x20) gp->misc_buttons |= MISC_BUTTON_HOME;

  // Thumb L
  if (data[5] & 0x40) gp->buttons |= BUTTON_THUMB_L;

  // Thumb R
  if (data[5] & 0x80) gp->buttons |= BUTTON_THUMB_R;

  // PlayStation button
  if (data[6] & 0x01) gp->misc_buttons |= MISC_BUTTON_SYSTEM;
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
          break;
        case HID_USAGE_AXIS_Y:
          gp->axis_y = uni_hid_parser_process_axis(globals, value);
          break;
        case HID_USAGE_AXIS_Z:
          gp->axis_rx = uni_hid_parser_process_axis(globals, value);
          break;
        case HID_USAGE_AXIS_RX:
          gp->brake = uni_hid_parser_process_pedal(globals, value);
          break;
        case HID_USAGE_AXIS_RY:
          gp->accelerator = uni_hid_parser_process_pedal(globals, value);
          break;
        case HID_USAGE_AXIS_RZ:
          gp->axis_ry = uni_hid_parser_process_axis(globals, value);
          break;
        case HID_USAGE_HAT:
          hat = uni_hid_parser_process_hat(globals, value);
          gp->dpad = uni_hid_parser_hat_to_dpad(hat);
          break;
        case HID_USAGE_SYSTEM_MAIN_MENU:
          if (value) gp->misc_buttons |= MISC_BUTTON_SYSTEM;
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
          if (value) gp->buttons |= BUTTON_X;
          break;
        case 0x02:  // X Button (0x02)
          if (value) gp->buttons |= BUTTON_A;
          break;
        case 0x03:  // Circle Button (0x04)
          if (value) gp->buttons |= BUTTON_B;
          break;
        case 0x04:  // Triangle Button (0x08)
          if (value) gp->buttons |= BUTTON_Y;
          break;
        case 0x05:  // Shoulder Left (0x10)
          if (value) gp->buttons |= BUTTON_SHOULDER_L;
          break;
        case 0x06:  // Shoulder Right (0x20)
          if (value) gp->buttons |= BUTTON_SHOULDER_R;
          break;
        case 0x07:  // Trigger L (0x40)
          if (value) gp->buttons |= BUTTON_TRIGGER_L;
          break;
        case 0x08:  // Trigger R (0x80)
          if (value) gp->buttons |= BUTTON_TRIGGER_R;
          break;
        case 0x09:  // Share (0x100)
          if (value) gp->misc_buttons |= MISC_BUTTON_BACK;
          break;
        case 0x0a:  // options button (0x200)
          if (value) gp->misc_buttons |= MISC_BUTTON_HOME;
          break;
        case 0x0b:  // thumb L (0x400)
          if (value) gp->buttons |= BUTTON_THUMB_L;
          break;
        case 0x0c:  // thumb R (0x800)
          if (value) gp->buttons |= BUTTON_THUMB_R;
          break;
        case 0x0d:  // ps button (0x1000)
          if (value) gp->misc_buttons |= MISC_BUTTON_SYSTEM;
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

void uni_hid_parser_ds4_set_led_color(uni_hid_device_t* d, uint8_t r, uint8_t g,
                                      uint8_t b) {
  ds4_output_report_t out = {0};

  out.flags = DS4_FF_FLAG_LED_COLOR;  // blink + LED + motor
  out.led_red = r;
  out.led_green = g;
  out.led_blue = b;

  ds4_send_output_report(d, &out);
}

void uni_hid_parser_ds4_set_rumble(uni_hid_device_t* d, uint8_t value,
                                   uint8_t duration) {
  ds4_instance_t* ins = get_ds4_instance(d);
  if (ins->rumble_in_progress) return;

  ds4_output_report_t out = {0};

  out.flags = DS4_FF_FLAG_RUMBLE;  // motor
  // Right motor: small force; left motor: big force
  out.motor_right = value;
  out.motor_left = value;
  ds4_send_output_report(d, &out);

  // set timer to turn off rumble
  ins->ts.process = &ds4_set_rumble_off;
  ins->rumble_in_progress = 1;
  int ms = duration * 4;  // duration: 256 ~= 1 second
  btstack_run_loop_set_timer(&ins->ts, ms);
  btstack_run_loop_add_timer(&ins->ts);
}

//
// Helpers
//
static ds4_instance_t* get_ds4_instance(uni_hid_device_t* d) {
  return (ds4_instance_t*)&d->parser_data[0];
}

static void ds4_send_output_report(uni_hid_device_t* d,
                                   ds4_output_report_t* out) {
  out->transaction_type = 0xa2;  // DATA | TYPE_OUTPUT
  out->report_id = 0x11;         // taken from HID descriptor
  out->unk0[0] = 0xc4;           // HID alone + poll interval

  /* CRC generation */
  uint8_t bthdr = 0xa2;
  uint32_t crc;

  crc = crc32_le(0xffffffff, &bthdr, 1);
  crc = ~crc32_le(crc, (uint8_t*)&out->report_id, sizeof(*out) - 5);
  out->crc = crc;

  uni_hid_device_send_intr_report(d, (uint8_t*)out, sizeof(*out));
}

static void ds4_set_rumble_off(btstack_timer_source_t* ts) {
  ds4_instance_t* ins = (ds4_instance_t*)ts;
  // No need to protect it with a mutex since it runs in the same main thread
  assert(ins->rumble_in_progress);
  ins->rumble_in_progress = 0;

  ds4_output_report_t out = {0};
  out.flags = DS4_FF_FLAG_RUMBLE;  // motor
  ds4_send_output_report(ins->hid_device, &out);
}
