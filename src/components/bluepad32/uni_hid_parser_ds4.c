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
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_log.h"
#include "uni_utils.h"

#define DS4_FEATURE_REPORT_FIRMWARE_VERSION 0xa3
#define DS4_FEATURE_REPORT_FIRMWARE_VERSION_SIZE 49
#define DS4_FEATURE_REPORT_CALIBRATION 0x02
#define DS4_FEATURE_REPORT_CALIBRATION_SIZE 37
#define DS4_STATUS_BATTERY_CAPACITY GENMASK(3, 0)

typedef struct {
    btstack_timer_source_t rumble_timer;
    bool rumble_in_progress;
    uint16_t fw_version;
    uint16_t hw_version;
} ds4_instance_t;
_Static_assert(sizeof(ds4_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "DS4 intance too big");

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
_Static_assert(sizeof(ds4_output_report_t) == 79, "Invalid DS4 output report size");

typedef struct __attribute((packed)) {
    // Axis
    uint8_t x, y;
    uint8_t rx, ry;

    // Hat + buttons
    uint8_t buttons[3];

    // Brake & throttle
    uint8_t brake;
    uint8_t throttle;

    // Motion sensors
    uint16_t sensor_timestamp;
    uint8_t sensor_temperature;
    uint16_t gyro[3];   // x, y, z
    uint16_t accel[3];  // x, y, z
    uint8_t reserved[5];
    uint8_t status[2];

    // Add missing data
} ds4_input_report_t;

typedef struct __attribute((packed)) {
    uint8_t report_id;  // Must be DS4_FEATURE_REPORT_FIRMWARE_VERSION
    char string_date[11];
    uint8_t unk_0[5];  // All zeroes apparenlty
    char string_time[8];
    uint8_t unk_1[9];  // All zeroes apparenlty
    uint8_t unk_2;     // Value 1
    uint16_t hw_version;
    uint32_t unk_3;
    uint16_t fw_version;
    uint16_t unk_4;
    uint32_t crc32;
} ds4_feature_report_firmware_version_t;
_Static_assert(sizeof(ds4_feature_report_firmware_version_t) == DS4_FEATURE_REPORT_FIRMWARE_VERSION_SIZE,
               "Invalid size");

// When sending the FF report, which "features" should be set.
enum {
    DS4_FF_FLAG_RUMBLE = 1 << 0,
    DS4_FF_FLAG_LED_COLOR = 1 << 1,
    DS4_FF_FLAG_LED_BLINK = 1 << 2,
};

static ds4_instance_t* get_ds4_instance(uni_hid_device_t* d);
static void ds4_send_output_report(uni_hid_device_t* d, ds4_output_report_t* out);
static void ds4_request_calibration_report(uni_hid_device_t* d);
static void ds4_request_firmware_version_report(uni_hid_device_t* d);
static void ds4_send_enable_lightbar_report(uni_hid_device_t* d);
static void ds4_set_rumble_off(btstack_timer_source_t* ts);

void uni_hid_parser_ds4_setup(struct uni_hid_device_s* d) {
    ds4_instance_t* ins = get_ds4_instance(d);
    memset(ins, 0, sizeof(*ins));

    // Send in order:
    // - enable lightbar: enables light and enables report 0x11 on most devices
    // - calibration report: enbles report 0x11 on other reports
    ds4_send_enable_lightbar_report(d);
    ds4_request_calibration_report(d);
    uni_hid_device_set_ready_complete(d);

    // Don't add any timer. If Calibration report is not supported,
    // it is safe to asume that the fw_request won't be supported as well.
}

void uni_hid_parser_ds4_init_report(uni_hid_device_t* d) {
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));

    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_ds4_parse_feature_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    ds4_instance_t* ins = get_ds4_instance(d);
    uint8_t report_id = report[0];
    switch (report_id) {
        case DS4_FEATURE_REPORT_CALIBRATION:
            /* TODO: Don't ignore calibration */
            if (len != DS4_FEATURE_REPORT_CALIBRATION_SIZE) {
                loge("DS4: Unexpected calibration size: got %d, want: %d\n", len, DS4_FEATURE_REPORT_CALIBRATION_SIZE);
                /* fallthrough */
            }
            ds4_request_firmware_version_report(d);
            break;
        case DS4_FEATURE_REPORT_FIRMWARE_VERSION:
            if (len != DS4_FEATURE_REPORT_FIRMWARE_VERSION_SIZE) {
                loge("DS4: Unexpected firmware version size: got %d, want: %d\n", len,
                     DS4_FEATURE_REPORT_FIRMWARE_VERSION_SIZE);
                /* fallthrough */
            }
            ds4_feature_report_firmware_version_t* r = (ds4_feature_report_firmware_version_t*)report;

            ins->hw_version = r->hw_version;
            ins->fw_version = r->fw_version;
            logi("DS4: fw version: 0x%04x, hw version: 0x%04x\n", ins->fw_version, ins->hw_version);
            logi("DS4: Firmware build date: %s, %s\n", r->string_date, r->string_time);
            break;
        default:
            loge("DS4: Unexpected report id in feature report: 0x%02x\n", report_id);
            break;
    }
}

void uni_hid_parser_ds4_parse_input_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (report[0] != 0x11) {
        loge("DS4: Unexpected report type: got 0x%02x, want: 0x11\n", report[0]);
        // printf_hexdump(report, len);
        return;
    }
    if (len != 78) {
        loge("DS4: Unexpected report len: got %d, want: 78\n", len);
        return;
    }

    uni_controller_t* ctl = &d->controller;
    const ds4_input_report_t* r = (ds4_input_report_t*)&report[3];

    // Axis
    ctl->gamepad.axis_x = (r->x - 127) * 4;
    ctl->gamepad.axis_y = (r->y - 127) * 4;
    ctl->gamepad.axis_rx = (r->rx - 127) * 4;
    ctl->gamepad.axis_ry = (r->ry - 127) * 4;

    // Hat
    uint8_t value = r->buttons[0] & 0xf;
    if (value > 7)
        value = 0xff; /* Center 0, 0 */
    ctl->gamepad.dpad = uni_hid_parser_hat_to_dpad(value);

    // Buttons
    // TODO: ds4, ds5 have these buttons in common. Refactor.
    if (r->buttons[0] & 0x10)
        ctl->gamepad.buttons |= BUTTON_X;  // West
    if (r->buttons[0] & 0x20)
        ctl->gamepad.buttons |= BUTTON_A;  // South
    if (r->buttons[0] & 0x40)
        ctl->gamepad.buttons |= BUTTON_B;  // East
    if (r->buttons[0] & 0x80)
        ctl->gamepad.buttons |= BUTTON_Y;  // North
    if (r->buttons[1] & 0x01)
        ctl->gamepad.buttons |= BUTTON_SHOULDER_L;  // L1
    if (r->buttons[1] & 0x02)
        ctl->gamepad.buttons |= BUTTON_SHOULDER_R;  // R1
    if (r->buttons[1] & 0x04)
        ctl->gamepad.buttons |= BUTTON_TRIGGER_L;  // L2
    if (r->buttons[1] & 0x08)
        ctl->gamepad.buttons |= BUTTON_TRIGGER_R;  // R2
    if (r->buttons[1] & 0x10)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_BACK;  // Share
    if (r->buttons[1] & 0x20)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_HOME;  // Options
    if (r->buttons[1] & 0x40)
        ctl->gamepad.buttons |= BUTTON_THUMB_L;  // Thumb L
    if (r->buttons[1] & 0x80)
        ctl->gamepad.buttons |= BUTTON_THUMB_R;  // Thumb R
    if (r->buttons[2] & 0x01)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;  // PS

    // Brake & throttle
    ctl->gamepad.brake = r->brake * 4;
    ctl->gamepad.throttle = r->throttle * 4;

    // Value goes from 0 to 10. Make it from 0 to 250.
    // The +1 is to avoid having a value of 0, which means "battery unavailable".
    ctl->battery = (r->status[0] & DS4_STATUS_BATTERY_CAPACITY) * 25 + 1;
}

// uni_hid_parser_ds4_parse_usage() was removed since "stream" mode is the only
// one supported. If needed, the function is preserved in git history:
// https://gitlab.com/ricardoquesada/bluepad32/-/blob/c32598f39831fd8c2fa2f73ff3c1883049caafc2/src/main/uni_hid_parser_ds4.c#L185

void uni_hid_parser_ds4_set_lightbar_color(uni_hid_device_t* d, uint8_t r, uint8_t g, uint8_t b) {
    ds4_output_report_t out = {
        .flags = DS4_FF_FLAG_LED_COLOR,  // blink + LED + motor
        .led_red = r,
        .led_green = g,
        .led_blue = b,
    };

    ds4_send_output_report(d, &out);
}

void uni_hid_parser_ds4_set_rumble(uni_hid_device_t* d, uint8_t value, uint8_t duration) {
    ds4_instance_t* ins = get_ds4_instance(d);
    if (ins->rumble_in_progress)
        return;

    ds4_output_report_t out = {
        .flags = DS4_FF_FLAG_RUMBLE,
        // Right motor: small force; left motor: big force
        .motor_right = value,
        .motor_left = value,
    };
    ds4_send_output_report(d, &out);

    // Set timer to turn off rumble
    ins->rumble_timer.process = &ds4_set_rumble_off;
    ins->rumble_timer.context = d;
    ins->rumble_in_progress = 1;
    int ms = duration * 4;  // duration: 256 ~= 1 second
    btstack_run_loop_set_timer(&ins->rumble_timer, ms);
    btstack_run_loop_add_timer(&ins->rumble_timer);
}

//
// Helpers
//
static ds4_instance_t* get_ds4_instance(uni_hid_device_t* d) {
    return (ds4_instance_t*)&d->parser_data[0];
}

static void ds4_send_output_report(uni_hid_device_t* d, ds4_output_report_t* out) {
    out->transaction_type = (HID_MESSAGE_TYPE_DATA << 4) | HID_REPORT_TYPE_OUTPUT;
    out->report_id = 0x11;  // taken from HID descriptor
    out->unk0[0] = 0xc4;    // HID alone + poll interval
    out->crc32 = ~uni_crc32_le(0xffffffff, (uint8_t*)out, sizeof(*out) - 4);

    uni_hid_device_send_intr_report(d, (uint8_t*)out, sizeof(*out));
}

static void ds4_set_rumble_off(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    ds4_instance_t* ins = get_ds4_instance(d);
    // No need to protect it with a mutex since it runs in the same main thread
    assert(ins->rumble_in_progress);
    ins->rumble_in_progress = 0;

    ds4_output_report_t out = {
        .flags = DS4_FF_FLAG_RUMBLE,
    };
    ds4_send_output_report(d, &out);
}

static void ds4_request_calibration_report(uni_hid_device_t* d) {
    // From Linux drivers/hid/hid-sony.c:
    // The default behavior of the DUALSHOCK 4 is to send reports using
    // report type 1 when running over Bluetooth. However, when feature
    // report 2 (CALIBRATION_REQUEST) is requested during the controller initialization, it starts
    // sending input reports in report 17.

    // Enable stream mode. Some models, like the CUH-ZCT2G require to explicitly request stream mode.
    static uint8_t report[] = {
        ((HID_MESSAGE_TYPE_GET_REPORT << 4) | HID_REPORT_TYPE_FEATURE),
        DS4_FEATURE_REPORT_CALIBRATION,
    };
    uni_hid_device_send_ctrl_report(d, (uint8_t*)report, sizeof(report));
}

static void ds4_request_firmware_version_report(uni_hid_device_t* d) {
    logi("DS4: ds4_request_firmware_version_report()\n");

    static uint8_t report[] = {
        ((HID_MESSAGE_TYPE_GET_REPORT << 4) | HID_REPORT_TYPE_FEATURE),
        DS4_FEATURE_REPORT_FIRMWARE_VERSION,
    };
    uni_hid_device_send_ctrl_report(d, (uint8_t*)report, sizeof(report));
}

static void ds4_send_enable_lightbar_report(uni_hid_device_t* d) {
    logi("DS4: ds4_send_enable_lightbar_report()\n");
    // Also turns off blinking, LED and rumble.
    ds4_output_report_t out = {
        // blink + LED + motor
        .flags = DS4_FF_FLAG_RUMBLE | DS4_FF_FLAG_LED_COLOR | DS4_FF_FLAG_LED_BLINK,

        // Default LED color: Blue
        .led_red = 0x00,
        .led_green = 0x00,
        .led_blue = 0x40,
    };
    ds4_send_output_report(d, &out);
}

void uni_hid_parser_ds4_device_dump(uni_hid_device_t* d) {
    ds4_instance_t* ins = get_ds4_instance(d);
    logi("\tDS4: FW version %#x, HW version %#x\n", ins->fw_version, ins->hw_version);
}
