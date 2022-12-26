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
#include "uni_common.h"
#include "uni_config.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_log.h"
#include "uni_utils.h"

#define DS5_FEATURE_REPORT_CALIBRATION 0x05
#define DS5_FEATURE_REPORT_CALIBRATION_SIZE 41
#define DS5_FEATURE_REPORT_PAIRING_INFO 0x09
#define DS5_FEATURE_REPORT_PAIRING_INFO_SIZE 20
#define DS5_FEATURE_REPORT_FIRMWARE_VERSION 0x20
#define DS5_FEATURE_REPORT_FIRMWARE_VERSION_SIZE 64
#define DS5_STATUS_BATTERY_CAPACITY GENMASK(3, 0)

enum {
    // Values for flag 0
    DS5_FLAG0_COMPATIBLE_VIBRATION = BIT(0),
    DS5_FLAG0_HAPTICS_SELECT = BIT(1),

    // Values for flag 1
    DS5_FLAG1_LIGHTBAR = BIT(2),
    DS5_FLAG1_PLAYER_LED = BIT(4),

    // Values for flag 2
    DS5_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE = BIT(1),

    // Values for lightbar_setup
    DS5_LIGHTBAR_SETUP_LIGHT_OUT = BIT(1),  // Fade light out
};

typedef enum {
    DS5_STATE_INITIAL,
    DS5_STATE_PAIRING_INFO_REQUEST,
    DS5_STATE_FIRMWARE_VERSION_REQUEST,
    DS5_STATE_CALIBRATION_REQUEST,
    DS5_STATE_READY,
} ds5_state_t;

typedef struct {
    btstack_timer_source_t rumble_timer;
    bool rumble_in_progress;
    uint8_t output_seq;
    ds5_state_t state;
    uint32_t hw_version;
    uint32_t fw_version;
} ds5_instance_t;
_Static_assert(sizeof(ds5_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "DS5 intance too big");

typedef struct __attribute((packed)) {
    // Bluetooth only
    uint8_t transaction_type;
    uint8_t report_id; /* 0x31 */
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

typedef struct __attribute((packed)) {
    uint8_t report_id;  // Must be DS5_FEATURE_REPORT_FIRMWARE_VERSION
    char string_date[11];
    char string_time[8];
    char unk_0[4];
    uint32_t hw_version;
    uint32_t fw_version;
    char unk_1[28];
    uint32_t crc32;
} ds5_feature_report_firmware_version_t;
_Static_assert(sizeof(ds5_feature_report_firmware_version_t) == DS5_FEATURE_REPORT_FIRMWARE_VERSION_SIZE,
               "Invalid size");

static ds5_instance_t* get_ds5_instance(uni_hid_device_t* d);
static void ds5_send_output_report(uni_hid_device_t* d, ds5_output_report_t* out);
static void ds5_send_enable_lightbar_report(uni_hid_device_t* d);
static void ds5_request_pairing_info_report(uni_hid_device_t* d);
static void ds5_request_firmware_version_report(uni_hid_device_t* d);
static void ds5_request_calibration_report(uni_hid_device_t* d);
static void ds5_set_rumble_off(btstack_timer_source_t* ts);

void uni_hid_parser_ds5_init_report(uni_hid_device_t* d) {
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));

    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_ds5_setup(uni_hid_device_t* d) {
    ds5_instance_t* ins = get_ds5_instance(d);
    memset(ins, 0, sizeof(*ins));
    ds5_request_pairing_info_report(d);
}

void uni_hid_parser_ds5_parse_feature_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    ds5_instance_t* ins = get_ds5_instance(d);
    uint8_t report_id = report[0];
    switch (report_id) {
        case DS5_FEATURE_REPORT_PAIRING_INFO:
            if (len != DS5_FEATURE_REPORT_PAIRING_INFO_SIZE) {
                loge("DS5: Unexpected pairing info size: got %d, want: %d\n", len,
                     DS5_FEATURE_REPORT_PAIRING_INFO_SIZE);
                /* fallthrough */
            }
            // report[0]: Report ID, in this case 9
            // report[1-6] has the DualSense Mac Address in reverse order
            // report[7,8,9]: unk
            // report[10-15]: has the host Mac Address in reverse order
            // report[16-19]: CRC32

            // Do nothing with Pairing Info.

            ds5_request_firmware_version_report(d);
            break;

        case DS5_FEATURE_REPORT_FIRMWARE_VERSION:
            if (len != DS5_FEATURE_REPORT_FIRMWARE_VERSION_SIZE) {
                loge("DS5: Unexpected firmware version size: got %d, want: %d\n", len,
                     DS5_FEATURE_REPORT_FIRMWARE_VERSION_SIZE);
                /* fallthrough */
            }
            ds5_feature_report_firmware_version_t* r = (ds5_feature_report_firmware_version_t*)report;
            ins->hw_version = r->hw_version;
            ins->fw_version = r->fw_version;

            // ASCII-z strings
            char date_z[sizeof(r->string_date) + 1] = {0};
            char time_z[sizeof(r->string_time) + 1] = {0};

            strncpy(date_z, r->string_date, sizeof(date_z) - 1);
            strncpy(time_z, r->string_time, sizeof(time_z) - 1);

            logi("DS5: fw version: 0x%08x, hw version: 0x%08x\n", ins->fw_version, ins->hw_version);
            logi("DS5: Firmware build date: %s, %s\n", date_z, time_z);

            ds5_request_calibration_report(d);
            break;

        case DS5_FEATURE_REPORT_CALIBRATION:
            /* TODO: Don't ignore calibration */
            if (len != DS5_FEATURE_REPORT_CALIBRATION_SIZE) {
                loge("DS5: Unexpected calibration size: got %d, want: %d\n", len, DS5_FEATURE_REPORT_CALIBRATION_SIZE);
                /* fallthrough */
            }
            ds5_send_enable_lightbar_report(d);
            break;

        default:
            loge("DS5: Unexpected report id in feature report: 0x%02x\n", report_id);
            break;
    }
}

void uni_hid_parser_ds5_parse_input_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (report[0] != 0x31) {
        loge("DS5: Unexpected report type: got 0x%02x, want: 0x31\n", report[0]);
        return;
    }
    if (len != 78) {
        loge("DS5: Unexpected report len: got %d, want: 78\n", len);
        return;
    }
    uni_controller_t* ctl = &d->controller;
    const ds5_input_report_t* r = (ds5_input_report_t*)&report[2];

    // Axis
    ctl->gamepad.axis_x = (r->x - 127) * 4;
    ctl->gamepad.axis_y = (r->y - 127) * 4;
    ctl->gamepad.axis_rx = (r->rx - 127) * 4;
    ctl->gamepad.axis_ry = (r->ry - 127) * 4;

    ctl->gamepad.brake = r->brake * 4;
    ctl->gamepad.throttle = r->throttle * 4;

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

    // Value goes from 0 to 10. Make it from 0 to 250.
    // The +1 is to avoid having a value of 0, which means "battery unavailable".
    ctl->battery = (r->status & DS5_STATUS_BATTERY_CAPACITY) * 25 + 1;
}

// uni_hid_parser_ds5_parse_usage() was removed since "stream" mode is the only one supported.
// If needed, the function is preserved in git history:
// https://gitlab.com/ricardoquesada/bluepad32/-/blob/c32598f39831fd8c2fa2f73ff3c1883049caafc2/src/main/uni_hid_parser_ds5.c#L213

void uni_hid_parser_ds5_set_player_leds(struct uni_hid_device_s* d, uint8_t value) {
    // PS5 has 5 player LEDS (instead of 4).
    // The player number is indicated by how many LEDs are on.
    // E.g: if two LEDs are On, it means gamepad is assigned to player 2.
    // And for player two, these are the LEDs that should be ON: -X-X-

    static const char led_values[] = {
        0x00,                               // No player
        BIT(2),                             // Player 1 (center LED)
        BIT(1) | BIT(3),                    // Player 2
        BIT(0) | BIT(2) | BIT(4),           // Player 3
        BIT(0) | BIT(1) | BIT(3) | BIT(4),  // Player 4
    };

    ds5_output_report_t out = {
        .player_leds = led_values[value % ARRAY_SIZE(led_values)],
        .valid_flag1 = DS5_FLAG1_PLAYER_LED,
    };

    ds5_send_output_report(d, &out);
}

void uni_hid_parser_ds5_set_lightbar_color(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b) {
    ds5_output_report_t out = {
        .lightbar_red = r,
        .lightbar_green = g,
        .lightbar_blue = b,
        .valid_flag1 = DS5_FLAG1_LIGHTBAR,
    };

    ds5_send_output_report(d, &out);
}

void uni_hid_parser_ds5_set_rumble(struct uni_hid_device_s* d, uint8_t value, uint8_t duration) {
    ds5_instance_t* ins = get_ds5_instance(d);
    if (ins->rumble_in_progress)
        return;

    ds5_output_report_t out = {
        .valid_flag0 = DS5_FLAG0_HAPTICS_SELECT | DS5_FLAG0_COMPATIBLE_VIBRATION,

        // Right motor: small force; left motor: big force
        .motor_right = value,
        .motor_left = value,
    };

    ds5_send_output_report(d, &out);

    // Set timer to turn off rumble
    ins->rumble_timer.process = &ds5_set_rumble_off;
    ins->rumble_timer.context = d;
    ins->rumble_in_progress = 1;
    int ms = duration * 4;  // duration: 256 ~= 1 second
    btstack_run_loop_set_timer(&ins->rumble_timer, ms);
    btstack_run_loop_add_timer(&ins->rumble_timer);
}

//
// Helpers
//
static ds5_instance_t* get_ds5_instance(uni_hid_device_t* d) {
    return (ds5_instance_t*)&d->parser_data[0];
}

static void ds5_send_output_report(uni_hid_device_t* d, ds5_output_report_t* out) {
    ds5_instance_t* ins = get_ds5_instance(d);

    out->transaction_type = (HID_MESSAGE_TYPE_DATA << 4) | HID_REPORT_TYPE_OUTPUT;
    out->report_id = 0x31;  // taken from HID descriptor
    out->tag = 0x10;        // Magic number must be set to 0x10

    // Highest 4-bit is a sequence number, which needs to be increased every
    // report. Lowest 4-bit is tag and can be zero for now.
    out->seq_tag = (ins->output_seq << 4) | 0x0;
    if (++ins->output_seq == 15)
        ins->output_seq = 0;

    out->crc32 = ~uni_crc32_le(0xffffffff, (uint8_t*)out, sizeof(*out) - 4);

    uni_hid_device_send_intr_report(d, (uint8_t*)out, sizeof(*out));
}

static void ds5_set_rumble_off(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    ds5_instance_t* ins = get_ds5_instance(d);

    // No need to protect it with a mutex since it runs in the same main thread
    assert(ins->rumble_in_progress);
    ins->rumble_in_progress = 0;

    ds5_output_report_t out = {
        .valid_flag0 = DS5_FLAG0_COMPATIBLE_VIBRATION | DS5_FLAG0_HAPTICS_SELECT,
    };

    ds5_send_output_report(d, &out);
}

static void ds5_request_calibration_report(uni_hid_device_t* d) {
    ds5_instance_t* ins = get_ds5_instance(d);
    ins->state = DS5_STATE_CALIBRATION_REQUEST;

    // Mimic kernel behavior: request calibration report
    // Hopefully fixes: https://gitlab.com/ricardoquesada/bluepad32/-/issues/2
    static uint8_t report[] = {
        ((HID_MESSAGE_TYPE_GET_REPORT << 4) | HID_REPORT_TYPE_FEATURE),
        DS5_FEATURE_REPORT_CALIBRATION,
    };
    uni_hid_device_send_ctrl_report(d, (uint8_t*)report, sizeof(report));
}

static void ds5_request_pairing_info_report(uni_hid_device_t* d) {
    ds5_instance_t* ins = get_ds5_instance(d);
    ins->state = DS5_STATE_PAIRING_INFO_REQUEST;

    static uint8_t report[] = {
        ((HID_MESSAGE_TYPE_GET_REPORT << 4) | HID_REPORT_TYPE_FEATURE),
        DS5_FEATURE_REPORT_PAIRING_INFO,
    };
    uni_hid_device_send_ctrl_report(d, (uint8_t*)report, sizeof(report));
}

static void ds5_request_firmware_version_report(uni_hid_device_t* d) {
    ds5_instance_t* ins = get_ds5_instance(d);
    ins->state = DS5_STATE_FIRMWARE_VERSION_REQUEST;

    static uint8_t report[] = {
        ((HID_MESSAGE_TYPE_GET_REPORT << 4) | HID_REPORT_TYPE_FEATURE),
        DS5_FEATURE_REPORT_FIRMWARE_VERSION,
    };
    uni_hid_device_send_ctrl_report(d, (uint8_t*)report, sizeof(report));
}

static void ds5_send_enable_lightbar_report(uni_hid_device_t* d) {
    // Enable lightbar, and set it to blue
    // Also, sending an output report enables input report 0x31.
    ds5_output_report_t out = {
        .lightbar_blue = 255,
        .valid_flag1 = DS5_FLAG1_LIGHTBAR,

        .valid_flag2 = DS5_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE,
        .lightbar_setup = DS5_LIGHTBAR_SETUP_LIGHT_OUT,
    };
    ds5_send_output_report(d, &out);

    ds5_instance_t* ins = get_ds5_instance(d);
    ins->state = DS5_STATE_READY;
    uni_hid_device_set_ready_complete(d);
}

void uni_hid_parser_ds5_device_dump(uni_hid_device_t* d) {
    ds5_instance_t* ins = get_ds5_instance(d);
    logi("\tDS5: FW version %#x, HW version %#x\n", ins->fw_version, ins->hw_version);
}
