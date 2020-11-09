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

#include "uni_hid_parser.h"

#include "hid_usage.h"
#include "uni_debug.h"
#include "uni_gamepad.h"
#include "uni_hid_device.h"

// HID Usage Tables:
// https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf

void uni_hid_parser(uni_hid_device_t* d, const uint8_t* report,
                    uint16_t report_len) {
  btstack_hid_parser_t parser;

  uni_report_parser_t* rp = &d->report_parser;

  // Certain devices like iCade might not set "init_report".
  if (rp->init_report) rp->init_report(d);

  // Certain devices like Nintendo Wii U Pro doesn't support HID descriptor.
  // For those kind of devices, just send the raw report.
  if (rp->parse_raw) {
    rp->parse_raw(d, report, report_len);
  }

  // Devices that suport regular HID reports. Basically everyone except the
  // Nintendo Wii U.
  if (rp->parse_usage) {
    btstack_hid_parser_init(&parser, d->hid_descriptor, d->hid_descriptor_len,
                            HID_REPORT_TYPE_INPUT, report, report_len);
    while (btstack_hid_parser_has_more(&parser)) {
      uint16_t usage_page;
      uint16_t usage;
      int32_t value;
      hid_globals_t globals;

      // Save globals, otherwise they are going to get destroyed by
      // btstack_hid_parser_get_field()
      globals.logical_minimum = parser.global_logical_minimum;
      globals.logical_maximum = parser.global_logical_maximum;
      globals.report_count = parser.global_report_count;
      globals.report_id = parser.global_report_id;
      globals.report_size = parser.global_report_size;
      globals.usage_page = parser.global_usage_page;

      btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);

      logd("usage_page = 0x%04x, usage = 0x%04x, value = 0x%x\n", usage_page,
           usage, value);
      rp->parse_usage(d, &globals, usage_page, usage, value);
    }
  }
}

// static uint32_t next_pot(uint32_t n) {
//     n--;
//     n |= n >> 1;
//     n |= n >> 2;
//     n |= n >> 4;
//     n |= n >> 8;
//     n |= n >> 16;
//     return n + 1;
// }

// Converts a possible value between (0, x) to (-x/2, x/2), and normalizes it
// between -512 and 511.
int32_t uni_hid_parser_process_axis(hid_globals_t* globals, uint32_t value) {
  int32_t max = globals->logical_maximum;
  int32_t min = globals->logical_minimum;

  // Amazon Fire 1st Gen reports max value as unsigned (0xff == 255) but the
  // spec says they are signed. So the parser correctly treats it as -1 (0xff).
  if (max == -1) {
    max = (1 << globals->report_size) - 1;
  }

  // Get the range: how big can be the number
  int32_t range = (max - min) + 1;
  // range = next_pot(range);

  // First we "center" the value, meaning that 0 is when the axis is not used.
  int32_t centered = value - range / 2 - min;

  // Then we normalize between -512 and 511
  int32_t normalized = centered * AXIS_NORMALIZE_RANGE / range;
  logd(
      "original = %d, centered = %d, normalized = %d (range = %d, min=%d, "
      "max=%d)\n",
      value, centered, normalized, range, min, max);

  return normalized;
}

// Converts a possible value between (0, x) to (0, 1023)
int32_t uni_hid_parser_process_pedal(hid_globals_t* globals, uint32_t value) {
  int32_t max = globals->logical_maximum;
  int32_t min = globals->logical_minimum;

  // Amazon Fire 1st Gen reports max value as unsigned (0xff == 255) but the
  // spec says they are signed. So the parser correctly treats it as -1 (0xff).
  if (max == -1) {
    max = (1 << globals->report_size) - 1;
  }

  // Get the range: how big can be the number
  int32_t range = (max - min) + 1;
  int32_t normalized = value * AXIS_NORMALIZE_RANGE / range;
  logd("original = %d, normalized = %d (range = %d, min=%d, max=%d)\n", value,
       normalized, range, min, max);

  return normalized;
}

uint8_t uni_hid_parser_process_hat(hid_globals_t* globals, uint32_t value) {
  int32_t v = (int32_t)value;
  // Assumes if value is outside valid range, then it is a "null value"
  if (v < globals->logical_minimum || v > globals->logical_maximum) return 0xff;
  // 0 should be the first value for hat, meaning that 0 is the "up" position.
  return v - globals->logical_minimum;
}

void uni_hid_parser_process_dpad(uint16_t usage, uint32_t value,
                                 uint8_t* dpad) {
  switch (usage) {
    case HID_USAGE_DPAD_UP:
      if (value)
        *dpad |= DPAD_UP;
      else
        *dpad &= ~DPAD_UP;
      break;
    case HID_USAGE_DPAD_DOWN:
      if (value)
        *dpad |= DPAD_DOWN;
      else
        *dpad &= ~DPAD_DOWN;
      break;
    case HID_USAGE_DPAD_RIGHT:
      if (value)
        *dpad |= DPAD_RIGHT;
      else
        *dpad &= ~DPAD_RIGHT;
      break;
    case HID_USAGE_DPAD_LEFT:
      if (value)
        *dpad |= DPAD_LEFT;
      else
        *dpad &= ~DPAD_LEFT;
      break;
    default:
      logi("Unsupported DPAD usage: 0x%02x", usage);
      break;
  }
}

uint8_t uni_hid_parser_hat_to_dpad(uint8_t hat) {
  uint8_t dpad = 0;
  switch (hat) {
    case 0xff:
    case 0x08:
      // joy.up = joy.down = joy.left = joy.right = 0;
      break;
    case 0:
      dpad = DPAD_UP;
      break;
    case 1:
      dpad = DPAD_UP | DPAD_RIGHT;
      break;
    case 2:
      dpad = DPAD_RIGHT;
      break;
    case 3:
      dpad = DPAD_RIGHT | DPAD_DOWN;
      break;
    case 4:
      dpad = DPAD_DOWN;
      break;
    case 5:
      dpad = DPAD_DOWN | DPAD_LEFT;
      break;
    case 6:
      dpad = DPAD_LEFT;
      break;
    case 7:
      dpad = DPAD_LEFT | DPAD_UP;
      break;
    default:
      loge("Error parsing hat value: 0x%02x\n", hat);
      break;
  }
  return dpad;
}