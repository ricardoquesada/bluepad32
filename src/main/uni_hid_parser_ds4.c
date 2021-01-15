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
_Static_assert(sizeof(ds4_instance_t) < HID_DEVICE_MAX_PARSER_DATA,
               "DS4 intance too big");

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
  uint32_t crc32;
} ds4_output_report_t;

typedef struct __attribute((packed)) {
  // Axis
  uint8_t x, y;
  uint8_t rx, ry;

  // Hat + buttons
  uint8_t buttons[3];

  // Brake & throttle
  uint8_t brake;
  uint8_t throttle;

  // Add missing data
} ds4_input_report_t;

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
  const ds4_input_report_t* r = (ds4_input_report_t*)&report[3];

  // Axis
  gp->axis_x = (r->x - 127) * 4;
  gp->axis_y = (r->y - 127) * 4;
  gp->axis_rx = (r->rx - 127) * 4;
  gp->axis_ry = (r->ry - 127) * 4;

  // Hat
  uint8_t value = r->buttons[0] & 0xf;
  if (value > 7) value = 0xff; /* Center 0, 0 */
  gp->dpad = uni_hid_parser_hat_to_dpad(value);

  // Buttons
  // TODO: ds4, ds5 have these buttons in common. Refactor.
  if (r->buttons[0] & 0x10) gp->buttons |= BUTTON_X;                 // West
  if (r->buttons[0] & 0x20) gp->buttons |= BUTTON_A;                 // South
  if (r->buttons[0] & 0x40) gp->buttons |= BUTTON_B;                 // East
  if (r->buttons[0] & 0x80) gp->buttons |= BUTTON_Y;                 // North
  if (r->buttons[1] & 0x01) gp->buttons |= BUTTON_SHOULDER_L;        // L1
  if (r->buttons[1] & 0x02) gp->buttons |= BUTTON_SHOULDER_R;        // R1
  if (r->buttons[1] & 0x04) gp->buttons |= BUTTON_TRIGGER_L;         // L2
  if (r->buttons[1] & 0x08) gp->buttons |= BUTTON_TRIGGER_R;         // R2
  if (r->buttons[1] & 0x10) gp->misc_buttons |= MISC_BUTTON_BACK;    // Share
  if (r->buttons[1] & 0x20) gp->misc_buttons |= MISC_BUTTON_HOME;    // Options
  if (r->buttons[1] & 0x40) gp->buttons |= BUTTON_THUMB_L;           // Thumb L
  if (r->buttons[1] & 0x80) gp->buttons |= BUTTON_THUMB_R;           // Thumb R
  if (r->buttons[2] & 0x01) gp->misc_buttons |= MISC_BUTTON_SYSTEM;  // PS

  // Brake & throttle
  gp->brake = r->brake * 4;
  gp->accelerator = r->throttle * 4;
}

// uni_hid_parser_ds4_parse_usage() was removed since "stream" mode is the only
// one supported. If needed, the function is preserved in git history:
// https://gitlab.com/ricardoquesada/bluepad32/-/blob/c32598f39831fd8c2fa2f73ff3c1883049caafc2/src/main/uni_hid_parser_ds4.c#L185

void uni_hid_parser_ds4_set_lightbar_color(uni_hid_device_t* d, uint8_t r,
                                           uint8_t g, uint8_t b) {
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
  uint32_t crc32 = crc32_le(0xffffffff, &bthdr, 1);
  crc32 = ~crc32_le(crc32, (uint8_t*)&out->report_id, sizeof(*out) - 5);
  out->crc32 = crc32;

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
