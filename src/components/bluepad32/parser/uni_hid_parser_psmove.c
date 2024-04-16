// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

/*
 * Information based on PS Move API project:
 * https://github.com/thp/psmoveapi
 */

#include "parser/uni_hid_parser_psmove.h"

#include <string.h>

#include "controller/uni_controller.h"
#include "hid_usage.h"
#include "uni_config.h"
#include "uni_hid_device.h"
#include "uni_log.h"

#define ZCM1_PID 0x03d5
#define ZCM2_PID 0x0c5e

// Required steps to determine what kind of extensions are supported.
typedef enum psmove_fsm {
    PSMOVE_FSM_0,                    // Uninitialized
    PSMOVE_FSM_REQUIRES_LED_UPDATE,  // Requires LED to be updated
    PSMOVE_FSM_LED_UPDATED,          // LED updated
} psmove_fsm_t;

typedef enum psmove_model {
    PSMOVE_MODEL_UNK,
    PSMOVE_MODEL_ZCM1,
    PSMOVE_MODEL_ZCM2,
} psmove_model_t;

typedef enum {
    PSMOVE_STATE_RUMBLE_DISABLED,
    PSMOVE_STATE_RUMBLE_DELAYED,
    PSMOVE_STATE_RUMBLE_IN_PROGRESS,
} psmove_state_rumble_t;

// psmove_instance_t represents data used by the psmove driver instance.
typedef struct psmove_instance_s {
    psmove_model_t model;
    psmove_fsm_t state;
    uint8_t led_rgb[3];

    btstack_timer_source_t rumble_timer_duration;
    btstack_timer_source_t rumble_timer_delayed_start;
    psmove_state_rumble_t rumble_state;

    // Used by delayed start
    uint16_t rumble_magnitude;
    uint16_t rumble_duration_ms;
} psmove_instance_t;
_Static_assert(sizeof(psmove_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "PSMove intance too big");

// As defined here:
// https://github.com/thp/psmoveapi/blob/master/src/psmove.c#L123
typedef struct __attribute((packed)) {
    /* Report related */
    uint8_t transaction_type; /* type of transaction: should be 0xa2 */
    uint8_t report_id;        /* report Id: should be 0x06 */

    /* Data related */
    uint8_t _zero;       /* must be zero */
    uint8_t led_rgb[3];  /* RGB for LED */
    uint8_t rumble2;     /* unknown, should be 0x00 for now */
    uint8_t rumble;      /* rumble value, 0x00..0xff */
    uint8_t _padding[2]; /* must be zero */
} psmove_output_report_t;

typedef struct __attribute((packed)) {
    uint8_t report_id;

    // Dpad, main buttons, misc buttons
    uint8_t buttons[3];

    uint8_t unk1;

    // Trigger
    uint8_t trigger;
    uint8_t trigger2; /* second frame */

    uint8_t unk2[4]; /* Has 7F as value */

    uint8_t time_high; /* high byte of timestamp */
    uint8_t battery;   /* battery level; 0x05 = max, 0xEE = USB charging */

    uint16_t accel_x; /* accelerometer X value */
    uint16_t accel_y; /* accelerometer Y value */
    uint16_t accel_z; /* accelerometer Z value */

    uint16_t accel_x2; /* second frame */
    uint16_t accel_y2;
    uint16_t accel_z2;

    uint16_t gyro_x; /* gyro x */
    uint16_t gyro_y; /* gyro y */
    uint16_t gyro_z; /* gyro z */

    uint16_t gyro_x2; /* second frame */
    uint16_t gyro_y2;
    uint16_t gyro_z2;

    uint16_t temperature; /* temperature and magnetomer X */
} psmove_input_common_report_t;

typedef struct __attribute((packed)) {
    psmove_input_common_report_t common;

    uint8_t magnetometer[4]; /* magnetometer X, Y, Z */
    uint8_t time_low;        /* low byte of timestamp */
    uint8_t extdata[5];      /* external device data (EXT port) */
} psmove_input_zcm1_report_t;

typedef struct __attribute((packed)) {
    psmove_input_common_report_t common;

    uint16_t time;
    uint16_t unk;
    uint8_t time_low; /* low byte of timestamp */
} psmove_input_zcm2_report_t;

static psmove_instance_t* get_psmove_instance(uni_hid_device_t* d);
static void psmove_send_output_report(uni_hid_device_t* d, psmove_output_report_t* out);
static void on_psmove_set_rumble_on(btstack_timer_source_t* ts);
static void on_psmove_set_rumble_off(btstack_timer_source_t* ts);
static void psmove_play_dual_rumble_now(uni_hid_device_t* d, uint16_t duration_ms, uint8_t magnitude);

void uni_hid_parser_psmove_init_report(uni_hid_device_t* d) {
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));

    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_psmove_parse_input_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    // printf_hexdump(report, len);

    // FIXME: parse ZCM1 / ZCM2 parts. For the moment only the "common" is being parsed.

    // Report len should be 45, at least in PS Move DS3
    if (len < sizeof(psmove_input_common_report_t)) {
        loge("psmove: Invalid report lenght, got: %d\n want: >= %d", len, sizeof(psmove_input_common_report_t));
        return;
    }

    const psmove_input_common_report_t* r = (psmove_input_common_report_t*)report;

    if (r->report_id != 0x01) {
        loge("psmove: Unexpected report_id, got: 0x%02x, want: 0x01\n", r->report_id);
        return;
    }

    uni_controller_t* ctl = &d->controller;

    // Buttons
    if (r->buttons[0] & 0x01)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;  // Select
    if (r->buttons[0] & 0x08)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_START;  // Start

    if (r->buttons[1] & 0x01)
        ctl->gamepad.buttons |= BUTTON_TRIGGER_L;  // L2
    if (r->buttons[1] & 0x02)
        ctl->gamepad.buttons |= BUTTON_TRIGGER_R;  // R2
    if (r->buttons[1] & 0x04)
        ctl->gamepad.buttons |= BUTTON_SHOULDER_L;  // L1
    if (r->buttons[1] & 0x08)
        ctl->gamepad.buttons |= BUTTON_SHOULDER_R;  // R1
    if (r->buttons[1] & 0x10)
        ctl->gamepad.buttons |= BUTTON_Y;  // West
    if (r->buttons[1] & 0x20)
        ctl->gamepad.buttons |= BUTTON_B;  // South
    if (r->buttons[1] & 0x40)
        ctl->gamepad.buttons |= BUTTON_A;  // East
    if (r->buttons[1] & 0x80)
        ctl->gamepad.buttons |= BUTTON_X;  // North

    if (r->buttons[2] & 0x01)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;  // PS
    if (r->buttons[2] & 0x08)
        ctl->gamepad.buttons |= BUTTON_THUMB_R;  // Move. TODO: Where should we map it ?
    if (r->buttons[2] & 0x10)
        ctl->gamepad.buttons |= BUTTON_TRIGGER_R;  // Trigger

    ctl->gamepad.throttle = r->trigger * 4;

    ctl->gamepad.accel[0] = r->accel_x;
    ctl->gamepad.accel[1] = r->accel_y;
    ctl->gamepad.accel[2] = r->accel_z;

    ctl->gamepad.gyro[0] = r->gyro_x;
    ctl->gamepad.gyro[1] = r->gyro_y;
    ctl->gamepad.gyro[2] = r->gyro_z;

    if (r->battery <= 5)
        ctl->battery = r->battery * 51;
}

void uni_hid_parser_psmove_play_dual_rumble(struct uni_hid_device_s* d,
                                            uint16_t start_delay_ms,
                                            uint16_t duration_ms,
                                            uint8_t weak_magnitude,
                                            uint8_t strong_magnitude) {
    if (d == NULL) {
        loge("psmove: Invalid device\n");
        return;
    }

    uint8_t magnitude = btstack_max(weak_magnitude, strong_magnitude);

    psmove_instance_t* ins = get_psmove_instance(d);
    switch (ins->rumble_state) {
        case PSMOVE_STATE_RUMBLE_DELAYED:
            btstack_run_loop_remove_timer(&ins->rumble_timer_delayed_start);
            break;
        case PSMOVE_STATE_RUMBLE_IN_PROGRESS:
            btstack_run_loop_remove_timer(&ins->rumble_timer_duration);
            break;
        default:
            // Do nothing
            break;
    }

    if (start_delay_ms == 0) {
        psmove_play_dual_rumble_now(d, duration_ms, magnitude);
    } else {
        // Set timer to have a delayed start
        ins->rumble_timer_delayed_start.process = &on_psmove_set_rumble_on;
        ins->rumble_timer_delayed_start.context = d;
        ins->rumble_state = PSMOVE_STATE_RUMBLE_DELAYED;
        ins->rumble_duration_ms = duration_ms;
        ins->rumble_magnitude = magnitude;

        btstack_run_loop_set_timer(&ins->rumble_timer_delayed_start, start_delay_ms);
        btstack_run_loop_add_timer(&ins->rumble_timer_delayed_start);
    }
}

void uni_hid_parser_psmove_set_lightbar_color(uni_hid_device_t* d, uint8_t r, uint8_t g, uint8_t b) {
    psmove_instance_t* ins = get_psmove_instance(d);
    ins->led_rgb[0] = r;
    ins->led_rgb[1] = g;
    ins->led_rgb[2] = b;

    psmove_output_report_t out = {
        .report_id = 0x06,
        .led_rgb[0] = r,
        .led_rgb[1] = g,
        .led_rgb[2] = b,
        .rumble = ins->rumble_magnitude,
    };
    psmove_send_output_report(d, &out);
}

void uni_hid_parser_psmove_setup(struct uni_hid_device_s* d) {
    psmove_instance_t* ins = get_psmove_instance(d);
    memset(ins, 0, sizeof(*ins));

    switch (d->product_id) {
        case ZCM1_PID:
            ins->model = PSMOVE_MODEL_ZCM1;
            logi("psmove: Detected ZCM1 model\n");
            break;
        case ZCM2_PID:
            ins->model = PSMOVE_MODEL_ZCM2;
            logi("psmove: Detected ZCM2 model\n");
            break;
        default:
            loge("psmove: Unknown PSMove PID = %#x, assuming ZCM1\n", ins->model);
            ins->model = PSMOVE_MODEL_ZCM1;
            break;
    }

    uni_hid_device_set_ready_complete(d);
}

//
// Helpers
//
static psmove_instance_t* get_psmove_instance(uni_hid_device_t* d) {
    return (psmove_instance_t*)&d->parser_data[0];
}

static void psmove_stop_rumble_now(uni_hid_device_t* d) {
    psmove_instance_t* ins = get_psmove_instance(d);

    // No need to protect it with a mutex since it runs in the same main thread
    assert(ins->rumble_state == PSMOVE_STATE_RUMBLE_IN_PROGRESS);
    ins->rumble_state = PSMOVE_STATE_RUMBLE_DISABLED;
    ins->rumble_magnitude = 0;

    psmove_output_report_t out = {
        .report_id = 0x06,
        // Don't overwrite LED RGB
        .led_rgb[0] = ins->led_rgb[0],
        .led_rgb[1] = ins->led_rgb[1],
        .led_rgb[2] = ins->led_rgb[2],
        .rumble = 0,
    };

    psmove_send_output_report(d, &out);
}

static void psmove_play_dual_rumble_now(uni_hid_device_t* d, uint16_t duration_ms, uint8_t magnitude) {
    psmove_instance_t* ins = get_psmove_instance(d);

    if (duration_ms == 0) {
        if (ins->rumble_state == PSMOVE_STATE_RUMBLE_IN_PROGRESS)
            psmove_stop_rumble_now(d);
        return;
    }

    psmove_output_report_t out = {
        .report_id = 0x06,
        // Don't overwrite LED RGB
        .led_rgb[0] = ins->led_rgb[0],
        .led_rgb[1] = ins->led_rgb[1],
        .led_rgb[2] = ins->led_rgb[2],
        .rumble = magnitude,
    };

    // Cache it until rumble is off. Might be used by LEDs
    ins->rumble_magnitude = magnitude;

    psmove_send_output_report(d, &out);

    // Set timer to turn off rumble
    ins->rumble_timer_duration.process = &on_psmove_set_rumble_off;
    ins->rumble_timer_duration.context = d;
    ins->rumble_state = PSMOVE_STATE_RUMBLE_IN_PROGRESS;
    btstack_run_loop_set_timer(&ins->rumble_timer_duration, duration_ms);
    btstack_run_loop_add_timer(&ins->rumble_timer_duration);
}

static void on_psmove_set_rumble_on(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    psmove_instance_t* ins = get_psmove_instance(d);

    psmove_play_dual_rumble_now(d, ins->rumble_duration_ms, ins->rumble_magnitude);
}

static void on_psmove_set_rumble_off(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    psmove_stop_rumble_now(d);
}

static void psmove_send_output_report(uni_hid_device_t* d, psmove_output_report_t* out) {
    /* Should be 0xa2 */
    out->transaction_type = (HID_MESSAGE_TYPE_DATA << 4) | HID_REPORT_TYPE_OUTPUT;

    // uni_hid_device_send_ctrl_report(d, (uint8_t*)out, sizeof(*out));
    uni_hid_device_send_intr_report(d, (uint8_t*)out, sizeof(*out));
}
