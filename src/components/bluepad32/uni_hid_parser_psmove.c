/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2023 Ricardo Quesada

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

/*
 * Information based on PS Move API project:
 * https://github.com/thp/psmoveapi
 */

#include "uni_hid_parser_psmove.h"

#include <string.h>

#include "hid_usage.h"
#include "uni_config.h"
#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_log.h"

// Required steps to determine what kind of extensions are supported.
typedef enum psmove_fsm {
    PSMOVE_FSM_0,                    // Uninitialized
    PSMOVE_FSM_REQUIRES_LED_UPDATE,  // Requires LED to be updated
    PSMOVE_FSM_LED_UPDATED,          // LED updated
} psmove_fsm_t;

// psmove_instance_t represents data used by the psmove driver instance.
typedef struct psmove_instance_s {
    psmove_fsm_t state;
    uint8_t led_rgb[3];
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

    uint8_t magnetometer[4]; /* magnetometer X, Y, Z */
    uint8_t time_low;        /* low byte of timestamp */
    uint8_t extdata[5];      /* external device data (EXT port) */

} psmove_input_report_t;

static psmove_instance_t* get_psmove_instance(uni_hid_device_t* d);
static void psmove_send_output_report(uni_hid_device_t* d, psmove_output_report_t* out);

void uni_hid_parser_psmove_init_report(uni_hid_device_t* d) {
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));

    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_psmove_parse_input_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    // printf_hexdump(report, len);

    // Report len should be 45, at least in PS Move DS3
    if (len < sizeof(psmove_input_report_t)) {
        loge("psmove: Invalid report lenght, got: %d\n want: >= %d", len, sizeof(psmove_input_report_t));
        return;
    }

    const psmove_input_report_t* r = (psmove_input_report_t*)report;

    if (r->report_id != 0x01) {
        loge("ds3: Unexpected report_id, got: 0x%02x, want: 0x01\n", r->report_id);
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

void uni_hid_parser_psmove_set_rumble(uni_hid_device_t* d, uint8_t value, uint8_t duration) {
    ARG_UNUSED(duration);

    psmove_instance_t* ins = get_psmove_instance(d);
    psmove_output_report_t out = {
        .report_id = 0x06,
        // Don't overwrite LED RGB
        .led_rgb[0] = ins->led_rgb[0],
        .led_rgb[1] = ins->led_rgb[1],
        .led_rgb[2] = ins->led_rgb[2],
        .rumble = value,
    };

    psmove_send_output_report(d, &out);
}

void uni_hid_parser_psmove_set_lightbar_color(uni_hid_device_t* d, uint8_t r, uint8_t g, uint8_t b) {
    psmove_output_report_t out = {
        .report_id = 0x06,
        .led_rgb[0] = r,
        .led_rgb[1] = g,
        .led_rgb[2] = b,
    };
    psmove_send_output_report(d, &out);
}

void uni_hid_parser_psmove_setup(struct uni_hid_device_s* d) {
    psmove_instance_t* ins = get_psmove_instance(d);
    memset(ins, 0, sizeof(*ins));

    uni_hid_device_set_ready_complete(d);
}

//
// Helpers
//
static psmove_instance_t* get_psmove_instance(uni_hid_device_t* d) {
    return (psmove_instance_t*)&d->parser_data[0];
}

static void psmove_send_output_report(uni_hid_device_t* d, psmove_output_report_t* out) {
    /* Should be 0xa2 */
    out->transaction_type = (HID_MESSAGE_TYPE_DATA << 4) | HID_REPORT_TYPE_OUTPUT;

    // uni_hid_device_send_ctrl_report(d, (uint8_t*)out, sizeof(*out));
    uni_hid_device_send_intr_report(d, (uint8_t*)out, sizeof(*out));
}
