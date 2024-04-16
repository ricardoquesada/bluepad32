// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

// More info about DUALSHOCK 4 gamepad:
// https://manuals.playstation.net/document/en/ps4/basic/pn_controller.html

// Technical info taken from:
// https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c
// https://github.com/chrippa/ds4drv/blob/master/ds4drv/device.py

#include "parser/uni_hid_parser_ds4.h"

#include <assert.h>

#include "bt/uni_bt_defines.h"
#include "hid_usage.h"
#include "uni_config.h"
#include "uni_hid_device.h"
#include "uni_log.h"
#include "uni_utils.h"

#define DS4_FEATURE_REPORT_FIRMWARE_VERSION 0xa3
#define DS4_FEATURE_REPORT_FIRMWARE_VERSION_SIZE 49
#define DS4_FEATURE_REPORT_CALIBRATION 0x02
#define DS4_FEATURE_REPORT_CALIBRATION_SIZE 37
#define DS4_STATUS_BATTERY_CAPACITY GENMASK(3, 0)

// DualShock4 hardware limits
#define DS4_ACC_RES_PER_G 8192
#define DS4_ACC_RANGE (4 * DS4_ACC_RES_PER_G)
#define DS4_GYRO_RES_PER_DEG_S 1024
#define DS4_GYRO_RANGE (2048 * DS4_GYRO_RES_PER_DEG_S)

// When sending the FF report, which "features" should be set.
enum {
    DS4_FF_FLAG_RUMBLE = 1 << 0,
    DS4_FF_FLAG_LED_COLOR = 1 << 1,
    DS4_FF_FLAG_LED_BLINK = 1 << 2,
    DS4_FF_FLAG_BLINK_COLOR_RUMBLE = DS4_FF_FLAG_RUMBLE | DS4_FF_FLAG_LED_COLOR | DS4_FF_FLAG_LED_BLINK,
};

typedef enum {
    DS4_STATE_RUMBLE_DISABLED,
    DS4_STATE_RUMBLE_DELAYED,
    DS4_STATE_RUMBLE_IN_PROGRESS,
} ds4_state_rumble_t;

// Calibration data for motion sensors.
struct ds4_calibration_data {
    int16_t bias;
    int32_t sens_numer;
    int32_t sens_denom;
};

typedef struct {
    // Although technically, we can use one timer for delay and duration, easier to debug/maintain if we have two.
    btstack_timer_source_t rumble_timer_duration;
    btstack_timer_source_t rumble_timer_delayed_start;
    ds4_state_rumble_t rumble_state;

    // Used by delayed start
    uint16_t rumble_weak_magnitude;
    uint16_t rumble_strong_magnitude;
    uint16_t rumble_duration_ms;

    uint16_t fw_version;
    uint16_t hw_version;

    struct ds4_calibration_data gyro_calib_data[3];
    struct ds4_calibration_data accel_calib_data[3];

    // Prev Touchpad values, to convert them from absolute
    // coordinates into relative ones.
    int x_prev;
    int y_prev;
    bool prev_touch_active;

    // Prev LED color and rumble values.
    uint8_t prev_color_red;
    uint8_t prev_color_green;
    uint8_t prev_color_blue;
    uint8_t prev_rumble_weak_magnitude;
    uint8_t prev_rumble_strong_magnitude;
} ds4_instance_t;
_Static_assert(sizeof(ds4_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "DS4 instance too big");

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

// Used by gamepads that don't support 0x11 report, like 8BitDo Zero 2 in "macOS" mode.
// Notice that report 0x01 is a subset of 0x11.
typedef struct __attribute((packed)) {
    // Axis
    uint8_t x, y;
    uint8_t rx, ry;

    // Hat + buttons
    uint8_t buttons[3];

    // Brake & throttle
    uint8_t brake;
    uint8_t throttle;
} ds4_input_report_01_t;

typedef struct __attribute((packed)) {
    uint8_t contact;
    uint8_t x_lo;
    uint8_t x_hi : 4, y_lo : 4;
    uint8_t y_hi;
} ds4_touch_point_t;

typedef struct __attribute((packed)) {
    uint8_t timestamp;
    ds4_touch_point_t points[2];
} ds4_touch_report_t;

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
    uint8_t reserved2;

    uint8_t num_touch_reports;
    ds4_touch_report_t touches[4];

    uint8_t reserved3[2];
    uint32_t crc32;
} ds4_input_report_11_t;

typedef struct __attribute((packed)) {
    uint8_t report_id;  // Must be DS4_FEATURE_REPORT_FIRMWARE_VERSION
    char string_date[11];
    uint8_t unk_0[5];  // All zeroes apparently
    char string_time[8];
    uint8_t unk_1[9];  // All zeroes apparently
    uint8_t unk_2;     // Value 1
    uint16_t hw_version;
    uint32_t unk_3;
    uint16_t fw_version;
    uint16_t unk_4;
    uint32_t crc32;
} ds4_feature_report_firmware_version_t;
_Static_assert(sizeof(ds4_feature_report_firmware_version_t) == DS4_FEATURE_REPORT_FIRMWARE_VERSION_SIZE,
               "Invalid size");

typedef struct __attribute((packed)) {
    uint8_t report_id;  // Must be DS4_FEATURE_REPORT_CALIBRATION
    int16_t gyro_pitch_bias;
    int16_t gyro_yaw_bias;
    int16_t gyro_roll_bias;

    // USB has different order than BT
    int16_t gyro_pitch_plus;
    int16_t gyro_yaw_plus;
    int16_t gyro_roll_plus;
    int16_t gyro_pitch_minus;
    int16_t gyro_yaw_minus;
    int16_t gyro_roll_minus;

    int16_t gyro_speed_plus;
    int16_t gyro_speed_minus;

    int16_t acc_x_plus;
    int16_t acc_x_minus;
    int16_t acc_y_plus;
    int16_t acc_y_minus;
    int16_t acc_z_plus;
    int16_t acc_z_minus;
    char unk[2];
} ds4_feature_report_calibration_t;
_Static_assert(sizeof(ds4_feature_report_calibration_t) == DS4_FEATURE_REPORT_CALIBRATION_SIZE, "Invalid size");

static ds4_instance_t* get_ds4_instance(uni_hid_device_t* d);
static void ds4_send_output_report(uni_hid_device_t* d, ds4_output_report_t* out);
static void ds4_request_calibration_report(uni_hid_device_t* d);
static void ds4_request_firmware_version_report(uni_hid_device_t* d);
static void ds4_send_enable_lightbar_report(uni_hid_device_t* d);
static void on_ds4_set_rumble_on(btstack_timer_source_t* ts);
static void on_ds4_set_rumble_off(btstack_timer_source_t* ts);
static void ds4_stop_rumble_now(uni_hid_device_t* d);
static void ds4_play_dual_rumble_now(uni_hid_device_t* d,
                                     uint16_t duration_ms,
                                     uint8_t weak_magnitude,
                                     uint8_t strong_magnitude);
static void ds4_parse_mouse(uni_hid_device_t* d, const ds4_input_report_11_t* r);

void uni_hid_parser_ds4_setup(struct uni_hid_device_s* d) {
    ds4_instance_t* ins = get_ds4_instance(d);
    memset(ins, 0, sizeof(*ins));

    // Default values for Accel / Gyro calibration data, until calibration is supported.
    for (size_t i = 0; i < ARRAY_SIZE(ins->accel_calib_data); i++) {
        ins->gyro_calib_data[i].bias = 0;
        ins->gyro_calib_data[i].sens_numer = DS4_GYRO_RANGE;
        ins->gyro_calib_data[i].sens_denom = INT16_MAX;

        ins->accel_calib_data[i].bias = 0;
        ins->accel_calib_data[i].sens_numer = DS4_ACC_RANGE;
        ins->accel_calib_data[i].sens_denom = INT16_MAX;
    }

    // Send in order:
    // - enable lightbar: enables light and enables report 0x11 on most devices
    // - calibration report: enables report 0x11 on other reports
    ds4_send_enable_lightbar_report(d);
    ds4_request_calibration_report(d);
    if (!uni_hid_device_set_ready_complete(d))
        return;

    // Don't add any timer. If calibration report is not supported,
    // it is safe to assume that the fw_request won't be supported as well.

    // Only after the connection was accepted, we should create the virtual device.
    uni_hid_device_t* child = uni_hid_device_create_virtual(d);
    if (!child) {
        loge("DS4: Failed to create virtual device\n");
        return;
    }

    // You are a mouse
    uni_hid_device_set_cod(child, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE);

    // And set it as connected + ready.
    uni_hid_device_connect(child);
    if (!uni_hid_device_set_ready_complete(child)) {
        // Could happen that the platform rejects the virtual device.
        // E.g: Mouse not supported. If that's the case, break the link
        d->child = NULL;
    }
}

void uni_hid_parser_ds4_init_report(uni_hid_device_t* d) {
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));

    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;

    // If we have a virtual child, set it up as mouse
    if (d->child) {
        uni_controller_t* virtual_ctl = &d->child->controller;
        memset(virtual_ctl, 0, sizeof(*virtual_ctl));

        virtual_ctl->klass = UNI_CONTROLLER_CLASS_MOUSE;
    }
}

void uni_hid_parser_ds4_parse_feature_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    ds4_instance_t* ins = get_ds4_instance(d);
    uint8_t report_id = report[0];

    switch (report_id) {
        case DS4_FEATURE_REPORT_CALIBRATION: {
            int speed_2x;
            int range_2g;

            if (len != DS4_FEATURE_REPORT_CALIBRATION_SIZE) {
                loge("DS4: Unexpected calibration size: got %d, want: %d\n", len, DS4_FEATURE_REPORT_CALIBRATION_SIZE);
                /* fallthrough */
            }

            logi("DS4: Calibration report received\n");
            ds4_feature_report_calibration_t* r = (ds4_feature_report_calibration_t*)report;
            // Taken from Linux Kernel
            // Set gyroscope calibration and normalization parameters.
            // Data values will be normalized to 1/DS_GYRO_RES_PER_DEG_S degree/s.
            speed_2x = r->gyro_speed_plus + r->gyro_speed_minus;
            ins->gyro_calib_data[0].bias = 0;
            ins->gyro_calib_data[0].sens_numer = speed_2x * DS4_GYRO_RES_PER_DEG_S;
            ins->gyro_calib_data[0].sens_denom =
                abs(r->gyro_pitch_plus - r->gyro_pitch_bias) + abs(r->gyro_pitch_minus + r->gyro_pitch_bias);

            ins->gyro_calib_data[1].bias = 0;
            ins->gyro_calib_data[1].sens_numer = speed_2x * DS4_GYRO_RES_PER_DEG_S;
            ins->gyro_calib_data[1].sens_denom =
                abs(r->gyro_yaw_plus - r->gyro_yaw_bias) + abs(r->gyro_yaw_minus - r->gyro_yaw_bias);

            ins->gyro_calib_data[2].bias = 0;
            ins->gyro_calib_data[2].sens_numer = speed_2x * DS4_GYRO_RES_PER_DEG_S;
            ins->gyro_calib_data[2].sens_denom =
                abs(r->gyro_roll_plus - r->gyro_roll_bias) + abs(r->gyro_roll_minus - r->gyro_roll_bias);

            // Sanity check gyro calibration data. This is needed to prevent crashes
            // during report handling of virtual, clone or broken devices not implementing
            // calibration data properly.
            for (size_t i = 0; i < ARRAY_SIZE(ins->gyro_calib_data); i++) {
                if (ins->gyro_calib_data[i].sens_denom == 0) {
                    loge("Invalid gyro calibration data for axis (%d), disabling calibration for axis = %d\n", i);
                    ins->gyro_calib_data[i].bias = 0;
                    ins->gyro_calib_data[i].sens_numer = DS4_GYRO_RANGE;
                    ins->gyro_calib_data[i].sens_denom = INT16_MAX;
                }
            }

            // Set accelerometer calibration and normalization parameters.
            // Data values will be normalized to 1/DS_ACC_RES_PER_G g.
            range_2g = r->acc_x_plus - r->acc_x_minus;
            ins->accel_calib_data[0].bias = r->acc_x_plus - range_2g / 2;
            ins->accel_calib_data[0].sens_numer = 2 * DS4_ACC_RES_PER_G;
            ins->accel_calib_data[0].sens_denom = range_2g;

            range_2g = r->acc_y_plus - r->acc_y_minus;
            ins->accel_calib_data[1].bias = r->acc_y_plus - range_2g / 2;
            ins->accel_calib_data[1].sens_numer = 2 * DS4_ACC_RES_PER_G;
            ins->accel_calib_data[1].sens_denom = range_2g;

            range_2g = r->acc_z_plus - r->acc_z_minus;
            ins->accel_calib_data[2].bias = r->acc_z_plus - range_2g / 2;
            ins->accel_calib_data[2].sens_numer = 2 * DS4_ACC_RES_PER_G;
            ins->accel_calib_data[2].sens_denom = range_2g;

            // Sanity check accelerometer calibration data. This is needed to prevent crashes
            // during report handling of virtual, clone or broken devices not implementing calibration
            // data properly.
            for (size_t i = 0; i < ARRAY_SIZE(ins->accel_calib_data); i++) {
                if (ins->accel_calib_data[i].sens_denom == 0) {
                    loge("Invalid accelerometer calibration data for axis (%d), disabling calibration for axis=%d\n",
                         i);
                    ins->accel_calib_data[i].bias = 0;
                    ins->accel_calib_data[i].sens_numer = DS4_ACC_RANGE;
                    ins->accel_calib_data[i].sens_denom = INT16_MAX;
                }
            }
            ds4_request_firmware_version_report(d);
            break;
        }
        case DS4_FEATURE_REPORT_FIRMWARE_VERSION: {
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
        }
        default:
            loge("DS4: Unexpected report id in feature report: 0x%02x\n", report_id);
            break;
    }
}

static void ds4_parse_input_report_01(uni_hid_device_t* d, const ds4_input_report_01_t* r) {
    uni_controller_t* ctl = &d->controller;

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
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;  // Share
    if (r->buttons[1] & 0x20)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_START;  // Options
    if (r->buttons[1] & 0x40)
        ctl->gamepad.buttons |= BUTTON_THUMB_L;  // Thumb L
    if (r->buttons[1] & 0x80)
        ctl->gamepad.buttons |= BUTTON_THUMB_R;  // Thumb R
    if (r->buttons[2] & 0x01)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;  // PS

    // Brake & throttle
    ctl->gamepad.brake = r->brake * 4;
    ctl->gamepad.throttle = r->throttle * 4;
}

static void ds4_parse_input_report_11(uni_hid_device_t* d, const ds4_input_report_11_t* r) {
    ds4_instance_t* ins = get_ds4_instance(d);
    uni_controller_t* ctl = &d->controller;

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
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;  // Share
    if (r->buttons[1] & 0x20)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_START;  // Options
    if (r->buttons[1] & 0x40)
        ctl->gamepad.buttons |= BUTTON_THUMB_L;  // Thumb L
    if (r->buttons[1] & 0x80)
        ctl->gamepad.buttons |= BUTTON_THUMB_R;  // Thumb R
    if (r->buttons[2] & 0x01)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;  // PS

    // Brake & throttle
    ctl->gamepad.brake = r->brake * 4;
    ctl->gamepad.throttle = r->throttle * 4;

    // Gyro
    for (size_t i = 0; i < ARRAY_SIZE(r->gyro); i++) {
        int32_t raw_data = (int16_t)r->gyro[i];
        int32_t calib_data =
            mult_frac(ins->gyro_calib_data[i].sens_numer, raw_data, ins->gyro_calib_data[i].sens_denom);
        ctl->gamepad.gyro[i] = calib_data;
    }

    // Accel
    for (size_t i = 0; i < ARRAY_SIZE(r->accel); i++) {
        int32_t raw_data = (int16_t)r->accel[i];
        int32_t calib_data =
            mult_frac(ins->accel_calib_data[i].sens_numer, raw_data, ins->accel_calib_data[i].sens_denom);
        ctl->gamepad.accel[i] = calib_data;
    }

    // Value goes from 0 to 10. Make it from 0 to 250.
    // The +1 is to avoid having a value of 0, which means "battery unavailable".
    ctl->battery = (r->status[0] & DS4_STATUS_BATTERY_CAPACITY) * 25 + 1;

    if (d->child) {
        ds4_parse_mouse(d->child, r);
    }
}

void uni_hid_parser_ds4_parse_input_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (report[0] == 0x11 && len == 78) {
        const ds4_input_report_11_t* r = (ds4_input_report_11_t*)&report[3];
        ds4_parse_input_report_11(d, r);
    } else if (report[0] == 0x01 && len == 10) {
        const ds4_input_report_01_t* r = (ds4_input_report_01_t*)&report[1];
        ds4_parse_input_report_01(d, r);
    } else {
        loge("DS4: Unexpected report type and len: report id=0x%02x, len=%d\n", report[0], len);
    }
    return;
}

// uni_hid_parser_ds4_parse_usage() was removed since "stream" mode is the only
// one supported. If needed, the function is preserved in git history:
// https://gitlab.com/ricardoquesada/bluepad32/-/blob/c32598f39831fd8c2fa2f73ff3c1883049caafc2/src/main/uni_hid_parser_ds4.c#L185

void uni_hid_parser_ds4_set_lightbar_color(uni_hid_device_t* d, uint8_t r, uint8_t g, uint8_t b) {
    ds4_instance_t* ins = get_ds4_instance(d);

    // Сache the previous LED color values
    ins->prev_color_red = r;
    ins->prev_color_green = g;
    ins->prev_color_blue = b;

    ds4_output_report_t out = {
        .flags = DS4_FF_FLAG_BLINK_COLOR_RUMBLE,  // blink + LED + motor
        .led_red = r,
        .led_green = g,
        .led_blue = b,
        // Prev rumble value to keep the rumble from turning off
        .motor_right = ins->prev_rumble_weak_magnitude,
        .motor_left = ins->prev_rumble_strong_magnitude,
    };

    ds4_send_output_report(d, &out);
}

void uni_hid_parser_ds4_play_dual_rumble(struct uni_hid_device_s* d,
                                         uint16_t start_delay_ms,
                                         uint16_t duration_ms,
                                         uint8_t weak_magnitude,
                                         uint8_t strong_magnitude) {
    if (d == NULL) {
        loge("DS4: Invalid device\n");
        return;
    }

    ds4_instance_t* ins = get_ds4_instance(d);
    switch (ins->rumble_state) {
        case DS4_STATE_RUMBLE_DELAYED:
            btstack_run_loop_remove_timer(&ins->rumble_timer_delayed_start);
            break;
        case DS4_STATE_RUMBLE_IN_PROGRESS:
            btstack_run_loop_remove_timer(&ins->rumble_timer_duration);
            break;
        default:
            // Do nothing
            break;
    }

    if (start_delay_ms == 0) {
        ds4_play_dual_rumble_now(d, duration_ms, weak_magnitude, strong_magnitude);
    } else {
        // Set timer to have a delayed start
        ins->rumble_timer_delayed_start.process = &on_ds4_set_rumble_on;
        ins->rumble_timer_delayed_start.context = d;
        ins->rumble_state = DS4_STATE_RUMBLE_DELAYED;
        ins->rumble_duration_ms = duration_ms;
        ins->rumble_strong_magnitude = strong_magnitude;
        ins->rumble_weak_magnitude = weak_magnitude;

        btstack_run_loop_set_timer(&ins->rumble_timer_delayed_start, start_delay_ms);
        btstack_run_loop_add_timer(&ins->rumble_timer_delayed_start);
    }
}

void uni_hid_parser_ds4_device_dump(uni_hid_device_t* d) {
    ds4_instance_t* ins = get_ds4_instance(d);
    logi("\tDS4: FW version %#x, HW version %#x\n", ins->fw_version, ins->hw_version);
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

static void ds4_stop_rumble_now(uni_hid_device_t* d) {
    ds4_instance_t* ins = get_ds4_instance(d);
    // No need to protect it with a mutex since it runs in the same main thread
    assert(ins->rumble_state == DS4_STATE_RUMBLE_IN_PROGRESS);
    ins->rumble_state = DS4_STATE_RUMBLE_DISABLED;

    // Сache the previous rumble value
    ins->prev_rumble_weak_magnitude = 0;
    ins->prev_rumble_strong_magnitude = 0;

    ds4_output_report_t out = {
        .flags = DS4_FF_FLAG_BLINK_COLOR_RUMBLE,  // blink + LED + motor
        // Prev LED color values to keep the LED color from turning off
        .led_red = ins->prev_color_red,
        .led_green = ins->prev_color_green,
        .led_blue = ins->prev_color_blue,
    };
    ds4_send_output_report(d, &out);
}

static void ds4_play_dual_rumble_now(uni_hid_device_t* d,
                                     uint16_t duration_ms,
                                     uint8_t weak_magnitude,
                                     uint8_t strong_magnitude) {
    ds4_instance_t* ins = get_ds4_instance(d);

    if (duration_ms == 0) {
        if (ins->rumble_state != DS4_STATE_RUMBLE_DISABLED)
            ds4_stop_rumble_now(d);
        return;
    }

    // Сache the previous rumble value
    ins->prev_rumble_weak_magnitude = weak_magnitude;
    ins->prev_rumble_strong_magnitude = strong_magnitude;

    ds4_output_report_t out = {
        .flags = DS4_FF_FLAG_BLINK_COLOR_RUMBLE,  // blink + LED + motor
        // Right motor: small force; left motor: big force
        .motor_right = weak_magnitude,
        .motor_left = strong_magnitude,
        // Prev LED color values to keep the LED color from turning off
        .led_red = ins->prev_color_red,
        .led_green = ins->prev_color_green,
        .led_blue = ins->prev_color_blue,
    };
    ds4_send_output_report(d, &out);

    // Set timer to turn off rumble
    ins->rumble_timer_duration.process = &on_ds4_set_rumble_off;
    ins->rumble_timer_duration.context = d;
    ins->rumble_state = DS4_STATE_RUMBLE_IN_PROGRESS;
    btstack_run_loop_set_timer(&ins->rumble_timer_duration, duration_ms);
    btstack_run_loop_add_timer(&ins->rumble_timer_duration);
}

static void on_ds4_set_rumble_on(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    ds4_instance_t* ins = get_ds4_instance(d);

    ds4_play_dual_rumble_now(d, ins->rumble_duration_ms, ins->rumble_weak_magnitude, ins->rumble_strong_magnitude);
}

static void on_ds4_set_rumble_off(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    ds4_stop_rumble_now(d);
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

    ds4_instance_t* ins = get_ds4_instance(d);

    // Cache the previous LED color and rumble values
    ins->prev_color_red = 0x00;
    ins->prev_color_green = 0x00;
    ins->prev_color_blue = 0x40;
    ins->prev_rumble_weak_magnitude = 0x00;
    ins->prev_rumble_strong_magnitude = 0x00;

    // Also turns off blinking, LED and rumble.
    ds4_output_report_t out = {
        // blink + LED + motor
        .flags = DS4_FF_FLAG_BLINK_COLOR_RUMBLE,
        // Default LED color: Blue
        .led_red = ins->prev_color_red,
        .led_green = ins->prev_color_green,
        .led_blue = ins->prev_color_blue,
    };
    ds4_send_output_report(d, &out);
}

static void ds4_parse_mouse(uni_hid_device_t* d, const ds4_input_report_11_t* r) {
    ds4_instance_t* ins = get_ds4_instance(d);

    // We can safely assume that device is connected and report is valid; otherwise
    // this function should have not been called.

    if (r->num_touch_reports < 1) {
        ins->prev_touch_active = false;
        return;
    }

    uni_controller_t* ctl = &d->controller;
    const ds4_touch_point_t* point = &r->touches[0].points[0];

    int x = (point->x_hi << 8) + point->x_lo;
    int y = (point->y_hi << 4) + point->y_lo;

    if (ins->prev_touch_active) {
        ctl->mouse.delta_x = x - ins->x_prev;
        ctl->mouse.delta_y = y - ins->y_prev;
    } else {
        ctl->mouse.delta_x = 0;
        ctl->mouse.delta_y = 0;
    }

    // "Click" on Touchpad
    if (r->buttons[2] & 0x02) {
        // Touchpad is divided in 0.75 (left) + 0.25 (right)
        // Touchpad range: 1920 x 942
        if (x < 1440)
            ctl->mouse.buttons |= UNI_MOUSE_BUTTON_LEFT;
        else
            ctl->mouse.buttons |= UNI_MOUSE_BUTTON_RIGHT;
        // TODO: Support middle button.
    }

    // Previous delta only if we are touching the touchpad.
    ins->prev_touch_active = !(point->contact & BIT(7));

    // Update prev regardless of whether it is valid.
    ins->x_prev = x;
    ins->y_prev = y;

    uni_hid_device_process_controller(d);
}
