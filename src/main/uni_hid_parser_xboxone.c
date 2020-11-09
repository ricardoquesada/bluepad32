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

// More info about Xbox One gamepad:
// https://support.xbox.com/en-US/xbox-one/accessories/xbox-one-wireless-controller

// Technical info taken from:
// https://github.com/atar-axis/xpadneo/blob/master/hid-xpadneo/src/hid-xpadneo.c

#include "uni_hid_parser_xboxone.h"

#include "hid_usage.h"
#include "uni_debug.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"

// Supported Xbox One firmware revisions.
// Probably there are more revisions, but I only found two in the "wild".
enum xboxone_firmware {
  // The one that came pre-installed, or close to it.
  XBOXONE_FIRMWARE_V3_1,
  // The one released in 2019-10
  XBOXONE_FIRMWARE_V4_8,
};

// xboxone_instance_t represents data used by the Wii driver instance.
typedef struct wii_instance_s {
  enum xboxone_firmware version;
} xboxone_instance_t;

static xboxone_instance_t* get_xboxone_instance(uni_hid_device_t* d);
static void rumble(uni_hid_device_t* d);
static void parse_usage_firmware_v3_1(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page, uint16_t usage,
                                      int32_t value);
static void parse_usage_firmware_v4_8(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page, uint16_t usage,
                                      int32_t value);

void uni_hid_parser_xboxone_setup(uni_hid_device_t* d) {
  xboxone_instance_t* ins = get_xboxone_instance(d);
  // FIXME: Parse HID descriptor and see if it supports 0xf buttons. Checking
  // for the len is a horrible hack.
  if (d->hid_descriptor_len > 330) {
    logi("Xbox one: Assuming it is firmware 4.8\n");
    ins->version = XBOXONE_FIRMWARE_V4_8;
  } else {
    // It is really firmware 4.8, it will be set later
    logi("Xbox one: Assuming it is firmware 3.1\n");
    ins->version = XBOXONE_FIRMWARE_V3_1;
  }
}

void uni_hid_parser_xboxone_init_report(uni_hid_device_t* d) {
  // Reset old state. Each report contains a full-state.
  d->gamepad.updated_states = 0;
}

void uni_hid_parser_xboxone_parse_usage(uni_hid_device_t* d,
                                        hid_globals_t* globals,
                                        uint16_t usage_page, uint16_t usage,
                                        int32_t value) {
  // print_parser_globals(globals);
  xboxone_instance_t* ins = get_xboxone_instance(d);
  if (ins->version == XBOXONE_FIRMWARE_V3_1) {
    parse_usage_firmware_v3_1(d, globals, usage_page, usage, value);
  } else {
    parse_usage_firmware_v4_8(d, globals, usage_page, usage, value);
  }
}

static void parse_usage_firmware_v3_1(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page, uint16_t usage,
                                      int32_t value) {
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
          logi(
              "Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n",
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
          logi(
              "Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, "
              "value=0x%x\n",
              usage_page, usage, value);
          break;
      }
      break;

    case HID_USAGE_PAGE_BUTTON: {
      switch (usage) {
        case 0x01:  // Button A
          if (value)
            gp->buttons |= BUTTON_A;
          else
            gp->buttons &= ~BUTTON_A;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_A;
          break;
        case 0x02:  // Button B
          if (value)
            gp->buttons |= BUTTON_B;
          else
            gp->buttons &= ~BUTTON_B;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_B;
          break;
        case 0x03:  // Button X
          if (value)
            gp->buttons |= BUTTON_X;
          else
            gp->buttons &= ~BUTTON_X;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_X;
          break;
        case 0x04:  // Button Y
          if (value)
            gp->buttons |= BUTTON_Y;
          else
            gp->buttons &= ~BUTTON_Y;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_Y;
          break;
        case 0x05:  // Button Left
          if (value)
            gp->buttons |= BUTTON_SHOULDER_L;
          else
            gp->buttons &= ~BUTTON_SHOULDER_L;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_L;
          break;
        case 0x06:  // Button Right
          if (value)
            gp->buttons |= BUTTON_SHOULDER_R;
          else
            gp->buttons &= ~BUTTON_SHOULDER_R;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_R;
          break;
        case 0x07:  // View button
          if (value)
            gp->misc_buttons |= MISC_BUTTON_BACK;
          else
            gp->misc_buttons &= ~MISC_BUTTON_BACK;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_BACK;
          break;
        case 0x08:  // Menu button
          if (value)
            gp->misc_buttons |= MISC_BUTTON_HOME;
          else
            gp->misc_buttons &= ~MISC_BUTTON_HOME;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_HOME;
          break;
        case 0x09:  // Thumb left
          if (value)
            gp->buttons |= BUTTON_THUMB_L;
          else
            gp->buttons &= ~BUTTON_THUMB_L;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_THUMB_L;
          break;
        case 0x0a:  // Thumb right
          if (value)
            gp->buttons |= BUTTON_THUMB_R;
          else
            gp->buttons &= ~BUTTON_THUMB_R;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_THUMB_R;
          break;
        case 0x0f: {
          // Only available in firmware v4.8.
          xboxone_instance_t* ins = get_xboxone_instance(d);
          ins->version = XBOXONE_FIRMWARE_V4_8;
          logi("Xbox one: Firmware 4.8 detected\n");
          break;
        }
        default:
          logi(
              "Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, "
              "value=0x%x\n",
              usage_page, usage, value);
          break;
      }
      break;
    }

    case HID_USAGE_PAGE_CONSUMER:
      // New in Xbox One firmware v4.8
      switch (usage) {
        case 0x0224:
          break;
        default:
          logi(
              "Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, "
              "value=0x%x\n",
              usage_page, usage, value);
          break;
      }
      break;

    // unknown usage page
    default:
      logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n",
           usage_page, usage, value);
      break;
  }
}

// v4.8 is almost identical to the Android mappings.
static void parse_usage_firmware_v4_8(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page, uint16_t usage,
                                      int32_t value) {
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
          logi(
              "Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n",
              usage_page, usage, value);
          break;
      }
      break;

    case HID_USAGE_PAGE_SIMULATION_CONTROLS:
      switch (usage) {
        case 0xc4:  // Accelerator
          gp->accelerator = uni_hid_parser_process_pedal(globals, value);
          gp->updated_states |= GAMEPAD_STATE_ACCELERATOR;
          break;
        case 0xc5:  // Brake
          gp->brake = uni_hid_parser_process_pedal(globals, value);
          gp->updated_states |= GAMEPAD_STATE_BRAKE;
          break;
        default:
          logi(
              "Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, "
              "value=0x%x\n",
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
          logi(
              "Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, "
              "value=0x%x\n",
              usage_page, usage, value);
          break;
      }
      break;

    case HID_USAGE_PAGE_BUTTON: {
      switch (usage) {
        case 0x01:  // Button A
          if (value)
            gp->buttons |= BUTTON_A;
          else
            gp->buttons &= ~BUTTON_A;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_A;
          break;
        case 0x02:  // Button B
          if (value)
            gp->buttons |= BUTTON_B;
          else
            gp->buttons &= ~BUTTON_B;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_B;
          break;
        case 0x03:  // Unused
          break;
        case 0x04:  // Button X
          if (value)
            gp->buttons |= BUTTON_X;
          else
            gp->buttons &= ~BUTTON_X;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_X;
          break;
        case 0x05:  // Button Y
          if (value)
            gp->buttons |= BUTTON_Y;
          else
            gp->buttons &= ~BUTTON_Y;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_Y;
          break;
        case 0x06:  // Unused
          break;
        case 0x07:  // Shoulder Left
          if (value)
            gp->buttons |= BUTTON_SHOULDER_L;
          else
            gp->buttons &= ~BUTTON_SHOULDER_L;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_L;
          break;
        case 0x08:  // Shoulder Right
          if (value)
            gp->buttons |= BUTTON_SHOULDER_R;
          else
            gp->buttons &= ~BUTTON_SHOULDER_R;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_SHOULDER_R;
          break;
        case 0x09:  // Unused
        case 0x0a:  // Unused
        case 0x0b:  // Unused
          break;
        case 0x0c:  // Burger button
          if (value)
            gp->misc_buttons |= MISC_BUTTON_HOME;
          else
            gp->misc_buttons &= ~MISC_BUTTON_HOME;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_HOME;
          break;
        case 0x0d:  // Xbox button
          if (value)
            gp->misc_buttons |= MISC_BUTTON_SYSTEM;
          else
            gp->misc_buttons &= ~MISC_BUTTON_SYSTEM;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
          break;
        case 0x0e:  // Thumb Left
          if (value)
            gp->buttons |= BUTTON_THUMB_L;
          else
            gp->buttons &= ~BUTTON_THUMB_L;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_THUMB_L;
          break;
        case 0x0f:  // Thumb Right
          if (value)
            gp->buttons |= BUTTON_THUMB_R;
          else
            gp->buttons &= ~BUTTON_THUMB_R;
          gp->updated_states |= GAMEPAD_STATE_BUTTON_THUMB_R;
          break;
        default:
          logi(
              "Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, "
              "value=0x%x\n",
              usage_page, usage, value);
          break;
      }
      break;
    }

    case HID_USAGE_PAGE_CONSUMER:
      switch (usage) {
        case 0x0224:  // Back
          if (value)
            gp->misc_buttons |= MISC_BUTTON_BACK;
          else
            gp->misc_buttons &= ~MISC_BUTTON_BACK;
          gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_BACK;
          break;
        default:
          logi(
              "Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, "
              "value=0x%x\n",
              usage_page, usage, value);
          break;
      }
      break;

    // unknown usage page
    default:
      logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n",
           usage_page, usage, value);
      break;
  }
}

void uni_hid_parser_xboxone_update_led(uni_hid_device_t* d) {
  if (d == NULL) {
    loge("Xbox One: Invalid device\n");
    return;
  }
  rumble(d);
}

static void rumble(uni_hid_device_t* d) {
  // Actuators for the force feedback (FF).
  enum {
    FF_RIGHT = 1 << 0,
    FF_LEFT = 1 << 1,
    FF_TRIGGER_RIGHT = 1 << 2,
    FF_TRIGGER_LEFT = 1 << 3,
  };

  struct ff_report {
    // Report related
    uint8_t transaction_type;  // type of transaction
    uint8_t report_id;         // report id
    // Force-feedback related
    uint8_t enable_actuators;    // LSB 0-3 for each actuator
    uint8_t force_left_trigger;  // HID descriptor says that it goes from 0-100
    uint8_t force_right_trigger;
    uint8_t force_left;
    uint8_t force_right;
    uint8_t duration;  // unknown unit, 255 is ~second
    uint8_t start_delay;
    uint8_t loop_count;  // how many times "duration" is repeated
  } __attribute__((__packed__));

  struct ff_report ff = {
      .transaction_type = 0xa2,  // HIDP_TRANS_DATA | HIDP_DATA_RTYPE_OUPUT
      .report_id = 0x03,         // taken from HID descriptor
      .enable_actuators = FF_TRIGGER_LEFT | FF_TRIGGER_RIGHT,
      .force_left_trigger = 25,
      .force_right_trigger = 25,
      .force_left = 25,
      .force_right = 25,
      .duration = 12,
      .start_delay = 0,
      .loop_count = 0,
  };

  uni_hid_device_send_intr_report(d, (uint8_t*)&ff, sizeof(ff));
}

//
// Helpers
//
xboxone_instance_t* get_xboxone_instance(uni_hid_device_t* d) {
  return (xboxone_instance_t*)&d->data[0];
}
