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

#include <assert.h>

#include "hid_usage.h"
#include "uni_config.h"
#include "uni_debug.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_utils.h"

enum {
  // Values for flag 0
  DS5_FLAG0_COMPATIBLE_VIBRATION = 1 << 0,
  DS5_FLAG0_HAPTICS_SELECT = 1 << 1,

  // Values for flag 1
  DS5_FLAG1_LIGHTBAR = 1 << 2,
  DS5_FLAG1_PLAYER_LED = 1 << 4,

  // Values for flag 2
  DS5_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE = 1 << 1,

  // Values for lightbar_setup
  DS5_LIGHTBAR_SETUP_LIGHT_OUT = 1 << 1,
};

typedef struct {
  // Must be first element
  btstack_timer_source_t ts;
  uni_hid_device_t* hid_device;
  bool rumble_in_progress;
  uint8_t output_seq;
} ds5_instance_t;
_Static_assert(sizeof(ds5_instance_t) < HID_DEVICE_MAX_PARSER_DATA,
               "DS5 intance too big");

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
  uint32_t crc32;
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
static void ds5_send_output_report(uni_hid_device_t* d,
                                   ds5_output_report_t* out);
static void ds5_set_rumble_off(btstack_timer_source_t* ts);

void uni_hid_parser_ds5_init_report(uni_hid_device_t* d) {
  uni_gamepad_t* gp = &d->gamepad;
  memset(gp, 0, sizeof(*gp));

  // Only report 0x31 is supported which is a "full report". It is safe to set
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

void uni_hid_parser_ds5_setup(uni_hid_device_t* d) {
  ds5_instance_t* ins = get_ds5_instance(d);
  memset(ins, 0, sizeof(*ins));
  ins->hid_device = d;  // Used by rumble callbacks

  // Enable lightbar.
  // Also, sending an output report enables input report 0x31.
  ds5_output_report_t out = {0};

  out.valid_flag2 = DS5_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE;
  out.lightbar_setup = DS5_LIGHTBAR_SETUP_LIGHT_OUT;
  ds5_send_output_report(d, &out);
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
}

// uni_hid_parser_ds5_parse_usage() was removed since "stream" mode is the only
// one supported. If needed, the function is preserved in git history:
// https://gitlab.com/ricardoquesada/bluepad32/-/blob/c32598f39831fd8c2fa2f73ff3c1883049caafc2/src/main/uni_hid_parser_ds5.c#L213

void uni_hid_parser_ds5_set_player_leds(struct uni_hid_device_s* d,
                                        uint8_t value) {
  ds5_output_report_t out = {0};

  out.player_leds = value;
  out.valid_flag1 = DS5_FLAG1_PLAYER_LED;

  ds5_send_output_report(d, &out);
}

void uni_hid_parser_ds5_set_lightbar_color(struct uni_hid_device_s* d,
                                           uint8_t r, uint8_t g, uint8_t b) {
  ds5_output_report_t out = {0};

  out.lightbar_red = r;
  out.lightbar_green = g;
  out.lightbar_blue = b;

  out.valid_flag1 = DS5_FLAG1_LIGHTBAR;

  ds5_send_output_report(d, &out);
}

void uni_hid_parser_ds5_set_rumble(struct uni_hid_device_s* d, uint8_t value,
                                   uint8_t duration) {
  ds5_instance_t* ins = get_ds5_instance(d);
  if (ins->rumble_in_progress) return;

  ds5_output_report_t out = {0};

  out.valid_flag0 = DS5_FLAG0_HAPTICS_SELECT | DS5_FLAG0_COMPATIBLE_VIBRATION;

  // Right motor: small force; left motor: big force
  out.motor_right = value;
  out.motor_left = value;

  ds5_send_output_report(d, &out);

  // Set timer to turn off rumble
  ins->ts.process = &ds5_set_rumble_off;
  ins->rumble_in_progress = 1;
  int ms = duration * 4;  // duration: 256 ~= 1 second
  btstack_run_loop_set_timer(&ins->ts, ms);
  btstack_run_loop_add_timer(&ins->ts);
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

  out->transaction_type = 0xa2;  // DATA | TYPE_OUTPUT
  out->report_id = 0x31;         // taken from HID descriptor
  out->tag = 0x10;               // Magic number must be set to 0x10

  // Highest 4-bit is a sequence number, which needs to be increased every
  // report. Lowest 4-bit is tag and can be zero for now.
  out->seq_tag = (ins->output_seq << 4) | 0x0;
  if (++ins->output_seq == 15) ins->output_seq = 0;

  /* CRC generation */
  uint8_t bthdr = 0xa2;
  uint32_t crc32 = crc32_le(0xffffffff, &bthdr, 1);
  crc32 = ~crc32_le(crc32, (uint8_t*)&out->report_id, sizeof(*out) - 5);
  out->crc32 = crc32;

  uni_hid_device_send_intr_report(d, (uint8_t*)out, sizeof(*out));
}

static void ds5_set_rumble_off(btstack_timer_source_t* ts) {
  loge("DS5: rumble-off\n");
  ds5_instance_t* ins = (ds5_instance_t*)ts;

  // No need to protect it with a mutex since it runs in the same main thread
  assert(ins->rumble_in_progress);
  ins->rumble_in_progress = 0;

  ds5_output_report_t out = {0};
  out.valid_flag0 = DS5_FLAG0_COMPATIBLE_VIBRATION | DS5_FLAG0_HAPTICS_SELECT;

  ds5_send_output_report(ins->hid_device, &out);
}
