/****************************************************************************
Dual Shock 3 hid support for Unijoysticle from Marcelo Lorenzati
mlorenzati@gmail.com

Source of info:
https://github.com/libretro/RetroArch/blob/5166eebcaf82ad784fe4856791880014d78807bd/input/common/hid/device_ds3.c#L180
http://wiki.ros.org/ps3joy
https://github.com/ros-drivers/joystick_drivers/blob/master/ps3joy/scripts/ps3joy_node.py
https://github.com/felis/USB_Host_Shield_2.0/wiki/PS3-Information#Bluetooth
https://github.com/jvpernis/esp32-ps3

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

#include "uni_hid_parser_ds3.h"

#include <string.h>

#include "hid_usage.h"
#include "uni_config.h"
#include "uni_debug.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"

// Required steps to determine what kind of extensions are supported.
typedef enum ds3_fsm {
  DS3_FSM_0,                    // Uninitialized
  DS3_FSM_REQUIRES_LED_UPDATE,  // Requires LED to be updated
  DS3_FSM_LED_UPDATED,          // LED updated
} ds3_fsm_t;

// ds3_instance_t represents data used by the DS3 driver instance.
typedef struct ds3_instance_s {
  ds3_fsm_t state;
} ds3_instance_t;

static ds3_instance_t* get_ds3_instance(uni_hid_device_t* d);
static void update_led(uni_hid_device_t* d);

enum ps3_packet_index {
  ps3_packet_index_buttons_raw = 2,

  ps3_packet_index_analog_stick_lx = 6,
  ps3_packet_index_analog_stick_ly = 7,
  ps3_packet_index_analog_stick_rx = 8,
  ps3_packet_index_analog_stick_ry = 9,

  ps3_packet_index_analog_button_up = 14,
  ps3_packet_index_analog_button_right = 15,
  ps3_packet_index_analog_button_down = 16,
  ps3_packet_index_analog_button_left = 17,

  ps3_packet_index_analog_button_l2 = 18,
  ps3_packet_index_analog_button_r2 = 19,
  ps3_packet_index_analog_button_l1 = 20,
  ps3_packet_index_analog_button_r1 = 21,

  ps3_packet_index_analog_button_triangle = 22,
  ps3_packet_index_analog_button_circle = 23,
  ps3_packet_index_analog_button_cross = 24,
  ps3_packet_index_analog_button_square = 25,

  ps3_packet_index_status = 29,

  ps3_packet_index_sensor_accelerometer_x = 41,
  ps3_packet_index_sensor_accelerometer_y = 43,
  ps3_packet_index_sensor_accelerometer_z = 45,
  ps3_packet_index_sensor_gyroscope_z = 47
};

enum ps3_button_mask {
  ps3_button_mask_select = 1 << 0,
  ps3_button_mask_l3 = 1 << 1,
  ps3_button_mask_r3 = 1 << 2,
  ps3_button_mask_start = 1 << 3,

  ps3_button_mask_up = 1 << 4,
  ps3_button_mask_right = 1 << 5,
  ps3_button_mask_down = 1 << 6,
  ps3_button_mask_left = 1 << 7,

  ps3_button_mask_l2 = 1 << 8,
  ps3_button_mask_r2 = 1 << 9,
  ps3_button_mask_l1 = 1 << 10,
  ps3_button_mask_r1 = 1 << 11,

  ps3_button_mask_triangle = 1 << 12,
  ps3_button_mask_circle = 1 << 13,
  ps3_button_mask_cross = 1 << 14,
  ps3_button_mask_square = 1 << 15,

  ps3_button_mask_ps = 1 << 16
};

void uni_hid_parser_ds3_init_report(uni_hid_device_t* d) {
  // Reset old state. Each report contains a full-state.
  memset(&d->gamepad, 0, sizeof(d->gamepad));
}

void uni_hid_parser_ds3_parse_raw(uni_hid_device_t* d, const uint8_t* report,
                                  uint16_t len) {
  UNUSED(len);

  ds3_instance_t* ins = get_ds3_instance(d);
  if (ins->state == DS3_FSM_REQUIRES_LED_UPDATE) {
    update_led(d);
  }

  uni_gamepad_t* gp = &d->gamepad;

  // Mapping buttons as on-off trigger buttons
  uint32_t ps3_buttons_raw =
      *((uint32_t*)&report[ps3_packet_index_buttons_raw]);
  uint32_t ps3_dpad_buttons[] = {ps3_button_mask_up, ps3_button_mask_down,
                                 ps3_button_mask_left, ps3_button_mask_right};
  uint32_t dpad_buttons[] = {DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT};
#define DPAD_SIZE (int)(sizeof(dpad_buttons) / sizeof(dpad_buttons[0]))

  uint32_t ps3_fire_buttons[] = {
      ps3_button_mask_triangle, ps3_button_mask_circle, ps3_button_mask_cross,
      ps3_button_mask_square,   ps3_button_mask_l1,     ps3_button_mask_r1,
      ps3_button_mask_l2,       ps3_button_mask_r2,     ps3_button_mask_l3,
      ps3_button_mask_r3};
  uint32_t fire_buttons[] = {
      BUTTON_X,          BUTTON_A,          BUTTON_B,         BUTTON_Y,
      BUTTON_SHOULDER_L, BUTTON_SHOULDER_R, BUTTON_TRIGGER_L, BUTTON_TRIGGER_R,
      BUTTON_THUMB_L,    BUTTON_THUMB_R};
#define FIRE_BUTTONS_SIZE (int)(sizeof(fire_buttons) / sizeof(fire_buttons[0]))

  uint32_t ps3_system_buttons[] = {ps3_button_mask_ps, ps3_button_mask_start,
                                   ps3_button_mask_select};
  uint32_t system_buttons[] = {MISC_BUTTON_SYSTEM, MISC_BUTTON_HOME,
                               MISC_BUTTON_BACK};
#define SYSTEM_BUTTONS_SIZE \
  (int)(sizeof(system_buttons) / sizeof(system_buttons[0]))

  // DPad
  for (int i = 0; i < DPAD_SIZE; i++) {
    if (ps3_buttons_raw & ps3_dpad_buttons[i])
      gp->dpad |= (dpad_buttons[i]);
    else
      gp->dpad &= ~(dpad_buttons[i]);
  }
  gp->updated_states |= GAMEPAD_STATE_DPAD;

  // Fire Buttons
  for (int i = 0; i < FIRE_BUTTONS_SIZE; i++) {
    if (ps3_buttons_raw & ps3_fire_buttons[i])
      gp->buttons |= (fire_buttons[i]);
    else
      gp->buttons &= ~(fire_buttons[i]);
  }

  gp->updated_states |= GAMEPAD_STATE_BUTTON_X | GAMEPAD_STATE_BUTTON_Y |
                        GAMEPAD_STATE_BUTTON_A | GAMEPAD_STATE_BUTTON_B;
  gp->updated_states |=
      GAMEPAD_STATE_BUTTON_TRIGGER_L | GAMEPAD_STATE_BUTTON_TRIGGER_R |
      GAMEPAD_STATE_BUTTON_SHOULDER_L | GAMEPAD_STATE_BUTTON_SHOULDER_R |
      GAMEPAD_STATE_BUTTON_THUMB_L | GAMEPAD_STATE_BUTTON_THUMB_R;

  // System Buttons
  for (int i = 0; i < SYSTEM_BUTTONS_SIZE; i++) {
    if (ps3_buttons_raw & ps3_system_buttons[i])
      gp->misc_buttons |= (system_buttons[i]);
    else
      gp->misc_buttons &= ~(system_buttons[i]);
  }
  gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM |
                        GAMEPAD_STATE_MISC_BUTTON_HOME |
                        GAMEPAD_STATE_MISC_BUTTON_BACK;

  // Update Axis data
  const uint16_t int_offset = 0x80;

  gp->axis_x =
      ((int16_t)report[ps3_packet_index_analog_stick_lx] - int_offset) * 2;
  gp->axis_y =
      ((int16_t)report[ps3_packet_index_analog_stick_ly] - int_offset) * 2;
  gp->axis_rx =
      ((int16_t)report[ps3_packet_index_analog_stick_rx] - int_offset) * 2;
  gp->axis_ry =
      ((int16_t)report[ps3_packet_index_analog_stick_ry] - int_offset) * 2;

  gp->updated_states |= (GAMEPAD_STATE_AXIS_X | GAMEPAD_STATE_AXIS_Y |
                         GAMEPAD_STATE_AXIS_RX | GAMEPAD_STATE_AXIS_RY);

  // Update Break and accelerator
  gp->brake = report[ps3_packet_index_analog_button_l2] * 4;
  gp->accelerator = report[ps3_packet_index_analog_button_r2] * 4;
  gp->updated_states |= GAMEPAD_STATE_BRAKE | GAMEPAD_STATE_ACCELERATOR;
}

void uni_hid_parser_ds3_update_led(uni_hid_device_t* d) {
  ds3_instance_t* ins = get_ds3_instance(d);

  // It seems that if a LED update is sent before the "request stream report",
  // then the stream report is ignored.
  // Update LED after stream report is set.
  if (ins->state <= DS3_FSM_REQUIRES_LED_UPDATE) {
    ins->state = DS3_FSM_REQUIRES_LED_UPDATE;
    return;
  }
  update_led(d);
}

void uni_hid_parser_ds3_setup(struct uni_hid_device_s* d) {
  // Dual Shock 3 Sixasis requires a magic packet to be sent in order to
  // enable reports Taken from:
  // https://github.com/ros-drivers/joystick_drivers/blob/52e8fcfb5619382a04756207b228fbc569f9a3ca/ps3joy/scripts/ps3joy_node.py#L299
  static uint8_t sixaxisEnableReports[] = {
      0x53,  // Transaction type: SET_REPORT Feature
      0xf4,  // Report ID
      0x42, 0x03, 0x00, 0x00};
  uni_hid_device_send_ctrl_report(d, (uint8_t*)&sixaxisEnableReports,
                                  sizeof(sixaxisEnableReports));
}

//
// Helpers
//
static ds3_instance_t* get_ds3_instance(uni_hid_device_t* d) {
  return (ds3_instance_t*)&d->data[0];
}

static void update_led(uni_hid_device_t* d) {
  ds3_instance_t* ins = get_ds3_instance(d);
  ins->state = DS3_FSM_LED_UPDATED;

  // Dual Shock 3 Control Packet, as defined in
  // https://github.com/ros-drivers/joystick_drivers/blob/52e8fcfb5619382a04756207b228fbc569f9a3ca/ps3joy/scripts/ps3joy_node.py#L276
  uint8_t control_packet[] = {
      0x52,  // Transaction type: SET_REPORT Output
      0x01,  // Report ID
      0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,                          // LED cmd
      0xff, 0x27, 0x10, 0x00, 0x32,  // LED 4
      0xff, 0x27, 0x10, 0x00, 0x32,  // LED 3
      0xff, 0x27, 0x10, 0x00, 0x32,  // LED 2
      0xff, 0x27, 0x10, 0x00, 0x32,  // LED 1
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  enum ps3_led_mask {
    ps3_led_mask_led1 = 1 << 1,
    ps3_led_mask_led2 = 1 << 2,
    ps3_led_mask_led3 = 1 << 3,
    ps3_led_mask_led4 = 1 << 4,
  };

#define LED_OFFSET 11
  control_packet[LED_OFFSET] = 0;

  if ((d->joystick_port & JOYSTICK_PORT_A) != 0) {
    control_packet[LED_OFFSET] |= ps3_led_mask_led1;
  }

  if ((d->joystick_port & JOYSTICK_PORT_B) != 0) {
    control_packet[LED_OFFSET] |= ps3_led_mask_led2;
  }

  uni_hid_device_send_ctrl_report(d, (uint8_t*)&control_packet,
                                  sizeof(control_packet));
}