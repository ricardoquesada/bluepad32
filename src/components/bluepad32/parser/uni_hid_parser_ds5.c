// SPDX-License-Identifier: Apache-2.0, MIT
// Copyright 2019 Ricardo Quesada
// 2021-2022 John "Nielk1" Klein
// http://retro.moe/unijoysticle2

// Technical info taken from:
// https://aur.archlinux.org/cgit/aur.git/tree/hid-playstation.c?h=hid-playstation-dkms
// https://gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db
// https://controllers.fandom.com/wiki/Sony_DualSense

#include "parser/uni_hid_parser_ds5.h"

#include <assert.h>

#include "bt/uni_bt_defines.h"
#include "uni_config.h"
#include "uni_hid_device.h"
#include "uni_log.h"
#include "uni_utils.h"

#define DS5_FEATURE_REPORT_CALIBRATION 0x05
#define DS5_FEATURE_REPORT_CALIBRATION_SIZE 41
#define DS5_FEATURE_REPORT_PAIRING_INFO 0x09
#define DS5_FEATURE_REPORT_PAIRING_INFO_SIZE 20
#define DS5_FEATURE_REPORT_FIRMWARE_VERSION 0x20
#define DS5_FEATURE_REPORT_FIRMWARE_VERSION_SIZE 64
#define DS5_STATUS_BATTERY_CAPACITY GENMASK(3, 0)

// DualSense hardware limits.
#define DS5_ACC_RES_PER_G 8192
#define DS5_ACC_RANGE (4 * DS5_ACC_RES_PER_G)
#define DS5_GYRO_RES_PER_DEG_S 1024
#define DS5_GYRO_RANGE (2048 * DS5_GYRO_RES_PER_DEG_S)

#define DS5_FEATURE_VERSION(major, minor) ((major & 0xff) << 8 | (minor & 0xff))

// Edge has different features than non-edge
#define DS5_PID 0x0ce6
#define DS5_EDGE_PID 0x0df2

enum {
    // Values for flag0
    DS5_FLAG0_COMPATIBLE_VIBRATION = BIT(0),
    DS5_FLAG0_HAPTICS_SELECT = BIT(1),
    DS5_FLAG0_FFB_RIGHT = BIT(2),
    DS5_FLAG0_FFB_LEFT = BIT(3),

    // Values for flag1
    DS5_FLAG1_MIC_MUTE_LED_CONTROL_ENABLE = BIT(0),
    DS5_FLAG1_POWER_SAVE_CONTROL_ENABLE = BIT(1),
    DS5_FLAG1_LIGHTBAR_CONTROL_ENABLE = BIT(2),
    DS5_FLAG1_RELEASE_LEDS = BIT(3),
    DS5_FLAG1_PLAYER_LED_CONTROL_ENABLE = BIT(4),

    // Values for flag2
    DS5_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE = BIT(1),
    DS5_FLAG2_COMPATIBLE_VIBRATION2 = BIT(2),

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

enum {
    DS5_ADAPTIVE_TRIGGER_EFFECT_INVALID = 0x00,
    DS5_ADAPTIVE_TRIGGER_EFFECT_OFF = 0x05,
    DS5_ADAPTIVE_TRIGGER_EFFECT_FEEDBACK = 0x21,
    DS5_ADAPTIVE_TRIGGER_EFFECT_WEAPON = 0x25,
    DS5_ADAPTIVE_TRIGGER_EFFECT_VIBRATION = 0x26,
};

typedef enum {
    DS5_STATE_RUMBLE_DISABLED,
    DS5_STATE_RUMBLE_DELAYED,
    DS5_STATE_RUMBLE_IN_PROGRESS,
} ds5_state_rumble_t;

// Calibration data for motion sensors.
struct ds5_calibration_data {
    int16_t bias;
    int32_t sens_numer;
    int32_t sens_denom;
};

typedef struct {
    // Although technically, we can use one timer for delay and duration, easier to debug/maintain if we have two.
    btstack_timer_source_t rumble_timer_duration;
    btstack_timer_source_t rumble_timer_delayed_start;
    ds5_state_rumble_t rumble_state;

    // Used by delayed start
    uint16_t rumble_weak_magnitude;
    uint16_t rumble_strong_magnitude;
    uint16_t rumble_duration_ms;

    uint8_t output_seq;
    ds5_state_t state;
    uint32_t hw_version;
    uint32_t fw_version;
    uint16_t update_version;
    bool use_vibration2;

    struct ds5_calibration_data gyro_calib_data[3];
    struct ds5_calibration_data accel_calib_data[3];

    // Prev Touchpad values, to convert them from absolute
    // coordinates into relative ones.
    int x_prev;
    int y_prev;
    bool prev_touch_active;

} ds5_instance_t;
_Static_assert(sizeof(ds5_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "DS5 instance too big");

typedef struct __attribute((packed)) {
    // Bluetooth only
    uint8_t transaction_type;
    uint8_t report_id; /* 0x31 */
    uint8_t seq_tag;
    uint8_t tag;

    // Common to Bluetooth and USB (although we don't support USB).
    uint8_t valid_flag0;
    uint8_t valid_flag1;

    // For DualShock 4 compatibility mode.
    uint8_t motor_right;  // small force
    uint8_t motor_left;   // big force

    // Audio controls
    uint8_t reserved1[4];
    uint8_t mute_button_led;

    uint8_t power_save_control;
    uint8_t right_trigger_ffb[11];
    uint8_t left_trigger_ffb[11];
    uint8_t reserved2[6];

    // LEDs and lightbar
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
    char unk_1[12];
    uint16_t update_version;
    char unk_2[14];
    uint32_t crc32;
} ds5_feature_report_firmware_version_t;
_Static_assert(sizeof(ds5_feature_report_firmware_version_t) == DS5_FEATURE_REPORT_FIRMWARE_VERSION_SIZE,
               "Invalid size");

typedef struct __attribute((packed)) {
    uint8_t report_id;  // Must be DS5_FEATURE_REPORT_CALIBRATION
    int16_t gyro_pitch_bias;
    int16_t gyro_yaw_bias;
    int16_t gyro_roll_bias;
    int16_t gyro_pitch_plus;
    int16_t gyro_pitch_minus;
    int16_t gyro_yaw_plus;
    int16_t gyro_yaw_minus;
    int16_t gyro_roll_plus;
    int16_t gyro_roll_minus;
    int16_t gyro_speed_plus;
    int16_t gyro_speed_minus;
    int16_t acc_x_plus;
    int16_t acc_x_minus;
    int16_t acc_y_plus;
    int16_t acc_y_minus;
    int16_t acc_z_plus;
    int16_t acc_z_minus;
    char unk[6];
} ds5_feature_report_calibration_t;
_Static_assert(sizeof(ds5_feature_report_calibration_t) == DS5_FEATURE_REPORT_CALIBRATION_SIZE, "Invalid size");

static ds5_instance_t* get_ds5_instance(uni_hid_device_t* d);
static void ds5_send_output_report(uni_hid_device_t* d, ds5_output_report_t* out);
static void ds5_send_enable_lightbar_report(uni_hid_device_t* d);
static void ds5_request_pairing_info_report(uni_hid_device_t* d);
static void ds5_request_firmware_version_report(uni_hid_device_t* d);
static void ds5_request_calibration_report(uni_hid_device_t* d);
static void on_ds5_set_rumble_on(btstack_timer_source_t* ts);
static void on_ds5_set_rumble_off(btstack_timer_source_t* ts);
static void ds5_stop_rumble_now(uni_hid_device_t* d);
static void ds5_play_dual_rumble_now(uni_hid_device_t* d,
                                     uint16_t duration_ms,
                                     uint8_t weak_magnitude,
                                     uint8_t strong_magnitude);
static void ds5_parse_mouse(uni_hid_device_t* d, const uint8_t* report, uint16_t len);

ds5_adaptive_trigger_effect_t ds5_new_adaptive_trigger_effect_off(void) {
    ds5_adaptive_trigger_effect_t out;
    out.effect = DS5_ADAPTIVE_TRIGGER_EFFECT_OFF;

    for (uint8_t i = 0; i < 10; i++)
        out.data[i] = 0;

    return out;
}

ds5_adaptive_trigger_effect_t ds5_new_adaptive_trigger_effect_feedback(uint8_t position, uint8_t strength) {
    ds5_adaptive_trigger_effect_t out;

    out.effect = DS5_ADAPTIVE_TRIGGER_EFFECT_INVALID;

    if (position > 9) {
        loge("DS5: Invalid position %d, expected =< 9\n", position);
        return out;
    }

    if (strength > 8) {
        loge("DS5: Invalid strength %d, expected =< 8\n", strength);
        return out;
    }

    out.effect = DS5_ADAPTIVE_TRIGGER_EFFECT_FEEDBACK;

    uint8_t force_value = (strength - 1) & 0x07;  // only 3 bits used
    uint32_t force_zones = 0;
    uint16_t active_zones = 0;

    for (uint8_t i = position; i <= 9; i++) {
        force_zones |= force_value << (i * 3);  // each force value occupies 3 bits x 10 zones
        active_zones |= (uint16_t)(1 << i);     // zone mask
    }

    out.data[0] = (active_zones >> 0) & 0xFF;
    out.data[1] = (active_zones >> 8) & 0xFF;
    out.data[2] = (force_zones >> 0) & 0xFF;
    out.data[3] = (force_zones >> 8) & 0xFF;
    out.data[4] = (force_zones >> 16) & 0xFF;
    out.data[5] = (force_zones >> 24) & 0xFF;
    for (int i = 6; i < 10; i++)
        out.data[i] = 0;

    return out;
}

ds5_adaptive_trigger_effect_t ds5_new_adaptive_trigger_effect_weapon(uint8_t start_position,
                                                                     uint8_t end_position,
                                                                     uint8_t strength) {
    ds5_adaptive_trigger_effect_t out;

    out.effect = DS5_ADAPTIVE_TRIGGER_EFFECT_INVALID;

    if (start_position < 2 || start_position > 7) {
        loge("DS5: Invalid start_position %d, expected 2 <= start_position <= 7\n", start_position);
        return out;
    }
    if (end_position <= start_position || end_position > 8) {
        loge("DS5: Invalid end_position %d, expected start_position < end_position <= 8\n", end_position);
        return out;
    }
    if (strength > 8) {
        loge("DS5: Invalid strength %d, expected <= 8\n", end_position);
        return out;
    }

    out.effect = DS5_ADAPTIVE_TRIGGER_EFFECT_WEAPON;

    uint16_t start_and_stop_zones = (1 << start_position) | (1 << end_position);

    out.data[0] = (start_and_stop_zones >> 0) & 0xFF;
    out.data[1] = (start_and_stop_zones >> 8) & 0xFF;
    out.data[2] = strength - 1;
    for (int i = 3; i < 10; i++)
        out.data[i] = 0;

    return out;
}

ds5_adaptive_trigger_effect_t ds5_new_adaptive_trigger_effect_vibration(uint8_t position,
                                                                        uint8_t amplitude,
                                                                        uint8_t frequency) {
    ds5_adaptive_trigger_effect_t out;

    out.effect = DS5_ADAPTIVE_TRIGGER_EFFECT_INVALID;

    if (position > 9) {
        loge("DS5: Invalid position %d, expected <= 9\n", position);
        return out;
    }
    if (amplitude > 8) {
        loge("DS5: Invalid amplitude %d, expected <= 8\n", position);
        return out;
    }

    out.effect = DS5_ADAPTIVE_TRIGGER_EFFECT_VIBRATION;

    uint8_t strength_value = (amplitude - 1) & 0x07;  // only 3 bits used
    uint32_t amplitude_zones = 0;
    uint16_t active_zones = 0;

    for (uint8_t i = position; i <= 9; i++) {
        amplitude_zones |= (uint32_t)strength_value << (i * 3);  // each force value occupies 3 bits x 10 zones
        active_zones |= (uint16_t)(1 << i);                      // zone mask
    }

    out.data[0] = (active_zones >> 0) & 0xFF;
    out.data[1] = (active_zones >> 8) & 0xFF;
    out.data[2] = (amplitude_zones >> 0) & 0xFF;
    out.data[3] = (amplitude_zones >> 8) & 0xFF;
    out.data[4] = (amplitude_zones >> 16) & 0xFF;
    out.data[5] = (amplitude_zones >> 24) & 0xFF;
    out.data[6] = 0;
    out.data[7] = 0;
    out.data[8] = frequency;
    out.data[9] = 0;

    return out;
}

void ds5_set_adaptive_trigger_effect(struct uni_hid_device_s* d,
                                     ds5_adaptive_trigger_type_t type,
                                     const ds5_adaptive_trigger_effect_t* effect) {
    if (effect->effect == DS5_ADAPTIVE_TRIGGER_EFFECT_INVALID) {
        loge("DS5: Invalid trigger effect\n");
        return;
    }

    ds5_output_report_t out = {.valid_flag0 =
                                   (type == UNI_ADAPTIVE_TRIGGER_TYPE_LEFT) ? DS5_FLAG0_FFB_LEFT : DS5_FLAG0_FFB_RIGHT};

    if (type == UNI_ADAPTIVE_TRIGGER_TYPE_LEFT) {
        memcpy(out.left_trigger_ffb, effect, sizeof(*effect));
    } else if (type == UNI_ADAPTIVE_TRIGGER_TYPE_RIGHT) {
        memcpy(out.right_trigger_ffb, effect, sizeof(*effect));
    } else {
        loge("DS5: Invalid trigger type: %d\n", type);
        return;
    }

    // logi("Has set valid flag %d, also, %d, and, %d", out.valid_flag0, out.left_trigger_ffb, out.right_trigger_ffb);
    ds5_send_output_report(d, &out);
}

void uni_hid_parser_ds5_init_report(uni_hid_device_t* d) {
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

void uni_hid_parser_ds5_setup(uni_hid_device_t* d) {
    ds5_instance_t* ins = get_ds5_instance(d);
    memset(ins, 0, sizeof(*ins));

    // Default values for Accel / Gyro calibration data, until calibration is supported.
    for (size_t i = 0; i < ARRAY_SIZE(ins->accel_calib_data); i++) {
        ins->gyro_calib_data[i].bias = 0;
        ins->gyro_calib_data[i].sens_numer = DS5_GYRO_RANGE;
        ins->gyro_calib_data[i].sens_denom = INT16_MAX;

        ins->accel_calib_data[i].bias = 0;
        ins->accel_calib_data[i].sens_numer = DS5_ACC_RANGE;
        ins->accel_calib_data[i].sens_denom = INT16_MAX;
    }

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

        case DS5_FEATURE_REPORT_FIRMWARE_VERSION: {
            if (len != DS5_FEATURE_REPORT_FIRMWARE_VERSION_SIZE) {
                loge("DS5: Unexpected firmware version size: got %d, want: %d\n", len,
                     DS5_FEATURE_REPORT_FIRMWARE_VERSION_SIZE);
                /* fallthrough */
            }
            ds5_feature_report_firmware_version_t* r = (ds5_feature_report_firmware_version_t*)report;
            ins->hw_version = r->hw_version;
            ins->fw_version = r->fw_version;
            ins->update_version = r->update_version;

            // ASCII-z strings
            char date_z[sizeof(r->string_date) + 1] = {0};
            char time_z[sizeof(r->string_time) + 1] = {0};

            strncpy(date_z, r->string_date, sizeof(date_z) - 1);
            strncpy(time_z, r->string_time, sizeof(time_z) - 1);

            // Supported in DualSense Edge, and in new regular DualSense.
            // "Vibration2" is the new way to rumble. The old one was emulating classic controllers.
            ins->use_vibration2 = (d->product_id == DS5_EDGE_PID || ins->update_version >= DS5_FEATURE_VERSION(2, 21));

            uni_hid_parser_ds5_device_dump(d);
            logi("\tDS5: Firmware build date: %s, %s\n", date_z, time_z);

            ds5_request_calibration_report(d);
            break;
        }

        case DS5_FEATURE_REPORT_CALIBRATION: {
            int speed_2x;
            int range_2g;

            if (len != DS5_FEATURE_REPORT_CALIBRATION_SIZE) {
                loge("DS5: Unexpected calibration size: got %d, want: %d\n", len, DS5_FEATURE_REPORT_CALIBRATION_SIZE);
                /* fallthrough */
            }
            logi("DS5: Calibration report received\n");
            ds5_feature_report_calibration_t* r = (ds5_feature_report_calibration_t*)report;

            // Taken from Linux Kernel
            // Set gyroscope calibration and normalization parameters.
            // Data values will be normalized to 1/DS_GYRO_RES_PER_DEG_S degree/s.
            speed_2x = r->gyro_speed_plus + r->gyro_speed_minus;
            ins->gyro_calib_data[0].bias = 0;
            ins->gyro_calib_data[0].sens_numer = speed_2x * DS5_GYRO_RES_PER_DEG_S;
            ins->gyro_calib_data[0].sens_denom =
                abs(r->gyro_pitch_plus - r->gyro_pitch_bias) + abs(r->gyro_pitch_minus + r->gyro_pitch_bias);

            ins->gyro_calib_data[1].bias = 0;
            ins->gyro_calib_data[1].sens_numer = speed_2x * DS5_GYRO_RES_PER_DEG_S;
            ins->gyro_calib_data[1].sens_denom =
                abs(r->gyro_yaw_plus - r->gyro_yaw_bias) + abs(r->gyro_yaw_minus - r->gyro_yaw_bias);

            ins->gyro_calib_data[2].bias = 0;
            ins->gyro_calib_data[2].sens_numer = speed_2x * DS5_GYRO_RES_PER_DEG_S;
            ins->gyro_calib_data[2].sens_denom =
                abs(r->gyro_roll_plus - r->gyro_roll_bias) + abs(r->gyro_roll_minus - r->gyro_roll_bias);

            // Sanity check gyro calibration data. This is needed to prevent crashes
            // during report handling of virtual, clone or broken devices not implementing
            // calibration data properly.
            for (size_t i = 0; i < ARRAY_SIZE(ins->gyro_calib_data); i++) {
                if (ins->gyro_calib_data[i].sens_denom == 0) {
                    loge("Invalid gyro calibration data for axis (%d), disabling calibration for axis = %d\n", i);
                    ins->gyro_calib_data[i].bias = 0;
                    ins->gyro_calib_data[i].sens_numer = DS5_GYRO_RANGE;
                    ins->gyro_calib_data[i].sens_denom = INT16_MAX;
                }
            }

            // Set accelerometer calibration and normalization parameters.
            // Data values will be normalized to 1/DS_ACC_RES_PER_G g.
            range_2g = r->acc_x_plus - r->acc_x_minus;
            ins->accel_calib_data[0].bias = r->acc_x_plus - range_2g / 2;
            ins->accel_calib_data[0].sens_numer = 2 * DS5_ACC_RES_PER_G;
            ins->accel_calib_data[0].sens_denom = range_2g;

            range_2g = r->acc_y_plus - r->acc_y_minus;
            ins->accel_calib_data[1].bias = r->acc_y_plus - range_2g / 2;
            ins->accel_calib_data[1].sens_numer = 2 * DS5_ACC_RES_PER_G;
            ins->accel_calib_data[1].sens_denom = range_2g;

            range_2g = r->acc_z_plus - r->acc_z_minus;
            ins->accel_calib_data[2].bias = r->acc_z_plus - range_2g / 2;
            ins->accel_calib_data[2].sens_numer = 2 * DS5_ACC_RES_PER_G;
            ins->accel_calib_data[2].sens_denom = range_2g;

            // Sanity check accelerometer calibration data. This is needed to prevent crashes
            // during report handling of virtual, clone or broken devices not implementing calibration
            // data properly.
            for (size_t i = 0; i < ARRAY_SIZE(ins->accel_calib_data); i++) {
                if (ins->accel_calib_data[i].sens_denom == 0) {
                    loge("Invalid accelerometer calibration data for axis (%d), disabling calibration for axis=%d\n",
                         i);
                    ins->accel_calib_data[i].bias = 0;
                    ins->accel_calib_data[i].sens_numer = DS5_ACC_RANGE;
                    ins->accel_calib_data[i].sens_denom = INT16_MAX;
                }
            }

            ds5_send_enable_lightbar_report(d);
            break;
        }

        default:
            loge("DS5: Unexpected report id in feature report: 0x%02x\n", report_id);
            break;
    }
}

void uni_hid_parser_ds5_parse_input_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    ds5_instance_t* ins = get_ds5_instance(d);

    // Don't process reports until state is ready. Prevents possible div-by-0 on calibration
    // and ignores other warnings.
    if (ins->state != DS5_STATE_READY)
        return;

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
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;  // Share
    if (r->buttons[1] & 0x20)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_START;  // Options
    if (r->buttons[1] & 0x40)
        ctl->gamepad.buttons |= BUTTON_THUMB_L;  // Thumb L
    if (r->buttons[1] & 0x80)
        ctl->gamepad.buttons |= BUTTON_THUMB_R;  // Thumb R
    if (r->buttons[2] & 0x01)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;  // PS
    if (r->buttons[2] & 0x04)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_CAPTURE;  // "mute" button

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
    ctl->battery = (r->status & DS5_STATUS_BATTERY_CAPACITY) * 25 + 1;

    if (d->child) {
        ds5_parse_mouse(d->child, report, len);
    }
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
        .valid_flag1 = DS5_FLAG1_PLAYER_LED_CONTROL_ENABLE,
    };

    ds5_send_output_report(d, &out);
}

void uni_hid_parser_ds5_set_lightbar_color(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b) {
    ds5_output_report_t out = {
        .lightbar_red = r,
        .lightbar_green = g,
        .lightbar_blue = b,
        .valid_flag1 = DS5_FLAG1_LIGHTBAR_CONTROL_ENABLE,
    };

    ds5_send_output_report(d, &out);
}

void uni_hid_parser_ds5_play_dual_rumble(struct uni_hid_device_s* d,
                                         uint16_t start_delay_ms,
                                         uint16_t duration_ms,
                                         uint8_t weak_magnitude,
                                         uint8_t strong_magnitude) {
    if (d == NULL) {
        loge("DS5: Invalid device\n");
        return;
    }

    ds5_instance_t* ins = get_ds5_instance(d);
    switch (ins->rumble_state) {
        case DS5_STATE_RUMBLE_DELAYED:
            btstack_run_loop_remove_timer(&ins->rumble_timer_delayed_start);
            break;
        case DS5_STATE_RUMBLE_IN_PROGRESS:
            btstack_run_loop_remove_timer(&ins->rumble_timer_duration);
            break;
        default:
            // Do nothing
            break;
    }

    if (start_delay_ms == 0) {
        ds5_play_dual_rumble_now(d, duration_ms, weak_magnitude, strong_magnitude);
    } else {
        // Set timer to have a delayed start
        ins->rumble_timer_delayed_start.process = &on_ds5_set_rumble_on;
        ins->rumble_timer_delayed_start.context = d;
        ins->rumble_state = DS5_STATE_RUMBLE_DELAYED;
        ins->rumble_duration_ms = duration_ms;
        ins->rumble_strong_magnitude = strong_magnitude;
        ins->rumble_weak_magnitude = weak_magnitude;

        btstack_run_loop_set_timer(&ins->rumble_timer_delayed_start, start_delay_ms);
        btstack_run_loop_add_timer(&ins->rumble_timer_delayed_start);
    }
}

void uni_hid_parser_ds5_device_dump(uni_hid_device_t* d) {
    ds5_instance_t* ins = get_ds5_instance(d);
    logi("\tDS5: FW version: %#x, HW version: %#x, update version: %#x, use vibration2: %d\n", ins->fw_version,
         ins->hw_version, ins->update_version, ins->use_vibration2);
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

static void ds5_stop_rumble_now(uni_hid_device_t* d) {
    ds5_instance_t* ins = get_ds5_instance(d);

    // No need to protect it with a mutex since it runs in the same main thread
    assert(ins->rumble_state != DS5_STATE_RUMBLE_DISABLED);
    ins->rumble_state = DS5_STATE_RUMBLE_DISABLED;

    ds5_output_report_t out = {
        .valid_flag0 = DS5_FLAG0_HAPTICS_SELECT,
    };

    if (ins->use_vibration2)
        out.valid_flag2 |= DS5_FLAG2_COMPATIBLE_VIBRATION2;
    else
        out.valid_flag0 |= DS5_FLAG0_COMPATIBLE_VIBRATION;

    ds5_send_output_report(d, &out);
}

static void ds5_play_dual_rumble_now(uni_hid_device_t* d,
                                     uint16_t duration_ms,
                                     uint8_t weak_magnitude,
                                     uint8_t strong_magnitude) {
    ds5_instance_t* ins = get_ds5_instance(d);

    if (duration_ms == 0) {
        if (ins->rumble_state != DS5_STATE_RUMBLE_DISABLED)
            ds5_stop_rumble_now(d);
        return;
    }

    ds5_output_report_t out = {
        .valid_flag0 = DS5_FLAG0_HAPTICS_SELECT,

        // Right motor: small force; left motor: big force
        .motor_right = weak_magnitude,
        .motor_left = strong_magnitude,
    };

    if (ins->use_vibration2)
        out.valid_flag2 |= DS5_FLAG2_COMPATIBLE_VIBRATION2;
    else
        out.valid_flag0 |= DS5_FLAG0_COMPATIBLE_VIBRATION;

    ds5_send_output_report(d, &out);

    // Set timer to turn off rumble
    ins->rumble_timer_duration.process = &on_ds5_set_rumble_off;
    ins->rumble_timer_duration.context = d;
    ins->rumble_state = DS5_STATE_RUMBLE_IN_PROGRESS;
    btstack_run_loop_set_timer(&ins->rumble_timer_duration, duration_ms);
    btstack_run_loop_add_timer(&ins->rumble_timer_duration);
}

static void on_ds5_set_rumble_on(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    ds5_instance_t* ins = get_ds5_instance(d);

    ds5_play_dual_rumble_now(d, ins->rumble_duration_ms, ins->rumble_weak_magnitude, ins->rumble_strong_magnitude);
}

static void on_ds5_set_rumble_off(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    ds5_stop_rumble_now(d);
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
        .valid_flag1 = DS5_FLAG1_LIGHTBAR_CONTROL_ENABLE,

        .valid_flag2 = DS5_FLAG2_LIGHTBAR_SETUP_CONTROL_ENABLE,
        .lightbar_setup = DS5_LIGHTBAR_SETUP_LIGHT_OUT,
    };
    ds5_send_output_report(d, &out);

    // Set as ready
    ds5_instance_t* ins = get_ds5_instance(d);
    ins->state = DS5_STATE_READY;
    if (!uni_hid_device_set_ready_complete(d)) {
        return;
    }

    // Only after the connection was accepted, we should create the virtual device.
    uni_hid_device_t* child = uni_hid_device_create_virtual(d);
    if (!child) {
        loge("DS5: Failed to create virtual device\n");
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

static void ds5_parse_mouse(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    ARG_UNUSED(len);

    ds5_instance_t* ins = get_ds5_instance(d);

    // We can safely assume that device is connected and report is valid; otherwise
    // this function should have not been called.

    uni_controller_t* ctl = &d->controller;
    const ds5_input_report_t* r = (ds5_input_report_t*)&report[2];

    int x = (r->points[0].x_hi << 8) + r->points[0].x_lo;
    int y = (r->points[0].y_hi << 4) + r->points[0].y_lo;

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
        // Touchpad range: 1920 x 1080
        if (x < 1440)
            ctl->mouse.buttons |= UNI_MOUSE_BUTTON_LEFT;
        else
            ctl->mouse.buttons |= UNI_MOUSE_BUTTON_RIGHT;
        // TODO: Support middle button.
    }

    // Previous delta only if we are touching the touchpad.
    ins->prev_touch_active = !(r->points[0].contact & BIT(7));

    // Update prev regardless of whether it is valid.
    ins->x_prev = x;
    ins->y_prev = y;

    uni_hid_device_process_controller(d);
}
