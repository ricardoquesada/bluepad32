/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2020 Ricardo Quesada

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

// Technical info taken from:
// https://aur.archlinux.org/cgit/aur.git/tree/hid-playstation.c?h=hid-playstation-dkms

#include "uni_hid_parser_ds5.h"

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
  uint8_t output_seq;
} ds5_instance_t;

typedef struct __attribute((packed)) {
  // Bluetooth only
  uint8_t transaction_type; /* 0xa2 */
  uint8_t report_id;        /* 0x31 */
  uint8_t seq_tag;
  uint8_t tag;

  // Common to Bluetooth and USB (although we don't support USB).
  uint8_t valid_flag0;
  uint8_t valid_flag1;

  /* For DualShock 4 compatibility mode. */
  uint8_t motor_right;
  uint8_t motor_left;

  /* Audio controls */
  uint8_t reserved1[4];
  uint8_t mute_button_led;

  uint8_t power_save_control;
  uint8_t reserved2[28];

  /* LEDs and lightbar */
  uint8_t valid_flag2;
  uint8_t reserved3[2];
  uint8_t lightbar_setup;
  uint8_t led_brightness;
  uint8_t player_leds;
  uint8_t lightbar_red;
  uint8_t lightbar_green;
  uint8_t lightbar_blue;

  //
  uint8_t reserved4[24];
  uint32_t crc;
} ds5_output_report_t;

/* Touchpad. Not used, added for completeness. */
typedef struct __attribute((packed)) {
  uint8_t contact;
  uint8_t x_lo;
  uint8_t x_hi : 4, y_lo : 4;
  uint8_t y_hi;
} ds5_touch_point_t;

/* Main DualSense input report excluding any BT/USB specific headers. */
typedef struct __attribute((packed)) {
  uint8_t x, y;
  uint8_t rx, ry;
  uint8_t brake, throttle;
  uint8_t seq_number;
  uint8_t buttons[4];
  uint8_t reserved[4];

  /* Motion sensors */
  uint16_t gyro[3];  /* x, y, z */
  uint16_t accel[3]; /* x, y, z */
  uint32_t sensor_timestamp;
  uint8_t reserved2;

  /* Touchpad */
  ds5_touch_point_t points[2];

  uint8_t reserved3[12];
  uint8_t status;
  uint8_t reserved4[11];
} ds5_input_report_t;

static ds5_instance_t* get_ds5_instance(uni_hid_device_t* d);
static void ds5_send_output_report(uni_hid_device_t* d, ds5_output_report_t* out);

void uni_hid_parser_ds5_init_report(uni_hid_device_t* d) {
  UNUSED(d);
  // Do nothing.
}

void uni_hid_parser_ds5_setup(uni_hid_device_t* d) {
  ds5_instance_t* ins = get_ds5_instance(d);
  memset(ins, 0, sizeof(*ins));
  ins->hid_device = d;  // Used by rumble callbacks

  // Turns off blinking, LED and rumble.
  ds5_output_report_t out = {0};

  out.transaction_type = 0xa2;  // DATA | TYPE_OUTPUT
  out.report_id = 0x31;         // taken from HID descriptor
  out.tag = 0x10;               // Magic number must be set to 0x10

  ds5_send_output_report(d, &out);

  uni_gamepad_t* gp = &d->gamepad;
  memset(gp, 0, sizeof(*gp));

  // Only report 0x31 is supported which is a "full report". It is safe to set
  // the reported states just once, here:
  gp->updated_states |= (GAMEPAD_STATE_AXIS_X | GAMEPAD_STATE_AXIS_Y |
                         GAMEPAD_STATE_AXIS_RX | GAMEPAD_STATE_AXIS_RY);
  gp->updated_states |= GAMEPAD_STATE_BRAKE | GAMEPAD_STATE_ACCELERATOR;
  gp->updated_states |= GAMEPAD_STATE_DPAD;
  gp->updated_states |= GAMEPAD_STATE_BUTTON_X | GAMEPAD_STATE_BUTTON_Y |
                        GAMEPAD_STATE_BUTTON_A | GAMEPAD_STATE_BUTTON_B;
  gp->updated_states |=
      GAMEPAD_STATE_BUTTON_TRIGGER_L | GAMEPAD_STATE_BUTTON_TRIGGER_R |
      GAMEPAD_STATE_BUTTON_SHOULDER_L | GAMEPAD_STATE_BUTTON_SHOULDER_R;
  gp->updated_states |=
      GAMEPAD_STATE_MISC_BUTTON_BACK | GAMEPAD_STATE_MISC_BUTTON_HOME;
  gp->updated_states |=
      GAMEPAD_STATE_BUTTON_THUMB_L | GAMEPAD_STATE_BUTTON_THUMB_R;
}

void uni_hid_parser_ds5_parse_raw(uni_hid_device_t* d, const uint8_t* report,
                                  uint16_t len) {
  if (report[0] != 0x31) {
    loge("DS5: Unexpected report type: got 0x%02x, want: 0x31\n", report[0]);
    printf_hexdump(report, len);
    return;
  }
  if (len != 78) {
    loge("DS5: Unexpected report len: got %d, want: 78\n", len);
    return;
  }
  uni_gamepad_t* gp = &d->gamepad;
  const ds5_input_report_t* r = (ds5_input_report_t*)&report[2];

  // Axis
  gp->axis_x = (r->x - 127) * 4;
  gp->axis_y = (r->y - 127) * 4;
  gp->axis_rx = (r->rx - 127) * 4;
  gp->axis_ry = (r->ry - 127) * 4;

  gp->brake = r->brake * 4;
  gp->accelerator = r->throttle * 4;

  // Hat
  uint8_t value = r->buttons[0] & 0xf;
  if (value > 7) value = 0xff; /* Center 0, 0 */
  gp->dpad = uni_hid_parser_hat_to_dpad(value);

  // Buttons
  // TODO: ds3, ds4, ds5 have these buttons in common. Refactor.
  // TODO: create function to set these flags.
  if (r->buttons[0] & 0x10)  // West
    gp->buttons |= BUTTON_X;
  else
    gp->buttons &= ~BUTTON_X;

  if (r->buttons[0] & 0x20)  // South
    gp->buttons |= BUTTON_A;
  else
    gp->buttons &= ~BUTTON_A;

  if (r->buttons[0] & 0x40)  // East
    gp->buttons |= BUTTON_B;
  else
    gp->buttons &= ~BUTTON_B;

  if (r->buttons[0] & 0x80)  // North
    gp->buttons |= BUTTON_Y;
  else
    gp->buttons &= ~BUTTON_Y;

  if (r->buttons[1] & 0x01)  // Shoulder L
    gp->buttons |= BUTTON_SHOULDER_L;
  else
    gp->buttons &= ~BUTTON_SHOULDER_L;

  if (r->buttons[1] & 0x02)  // Shoulder R
    gp->buttons |= BUTTON_SHOULDER_R;
  else
    gp->buttons &= ~BUTTON_SHOULDER_R;

  if (r->buttons[1] & 0x04)  // Trigger L
    gp->buttons |= BUTTON_TRIGGER_L;
  else
    gp->buttons &= ~BUTTON_TRIGGER_L;

  if (r->buttons[1] & 0x08)  // Trigger R
    gp->buttons |= BUTTON_TRIGGER_R;
  else
    gp->buttons &= ~BUTTON_TRIGGER_R;

  if (r->buttons[1] & 0x10)  // Share
    gp->misc_buttons |= MISC_BUTTON_BACK;
  else
    gp->misc_buttons &= ~MISC_BUTTON_BACK;

  if (r->buttons[1] & 0x20)  // Options
    gp->misc_buttons |= MISC_BUTTON_HOME;
  else
    gp->misc_buttons &= ~MISC_BUTTON_HOME;

  if (r->buttons[1] & 0x40)  // Thumb L
    gp->buttons |= BUTTON_THUMB_L;
  else
    gp->buttons &= ~BUTTON_THUMB_L;

  if (r->buttons[1] & 0x80)  // Thumb R
    gp->buttons |= BUTTON_THUMB_R;
  else
    gp->buttons &= ~BUTTON_THUMB_R;

  if (r->buttons[2] & 0x01)  // PlayStation button
    gp->misc_buttons |= MISC_BUTTON_SYSTEM;
  else
    gp->misc_buttons &= ~MISC_BUTTON_SYSTEM;
}

void uni_hid_parser_ds5_parse_usage(uni_hid_device_t* d, hid_globals_t* globals,
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
          logi("DS5: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n",
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
          logi("DS5: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n",
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
          logi("DS5: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n",
               usage_page, usage, value);
          break;
      }
      break;
    }
    // unknown usage page
    default:
      logi("DS5: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n",
           usage_page, usage, value);
      break;
  }
}

void uni_hid_parser_ds5_set_led_color(struct uni_hid_device_s* d, uint8_t r,
                                      uint8_t g, uint8_t b) {
  UNUSED(d);
  UNUSED(r);
  UNUSED(g);
  UNUSED(b);
}

void uni_hid_parser_ds5_set_rumble(struct uni_hid_device_s* d, uint8_t value,
                                   uint8_t duration) {
  UNUSED(d);
  UNUSED(value);
  UNUSED(duration);
}

//
// Helpers
//
static ds5_instance_t* get_ds5_instance(uni_hid_device_t* d) {
  return (ds5_instance_t*)&d->parser_data[0];
}

static void ds5_send_output_report(uni_hid_device_t* d,
                                  ds5_output_report_t* out) {
  ds5_instance_t* ins = get_ds5_instance(d);

  // Highest 4-bit is a sequence number, which needs to be increased every
  // report. Lowest 4-bit is tag and can be zero for now.
  out->seq_tag = (ins->output_seq << 4) | 0x0;
  if (++ins->output_seq == 15) ins->output_seq = 0;

  /* CRC generation */
  uint8_t bthdr = 0xA2;
  uint32_t crc;

  crc = crc32_le(0xffffffff, &bthdr, 1);
  crc = ~crc32_le(crc, (uint8_t*)&out->report_id, sizeof(*out) - 5);
  out->crc = crc;


  uni_hid_device_send_intr_report(d, (uint8_t*)out, sizeof(*out));
}
