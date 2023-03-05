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
    uint8_t player_leds;  // bitmap of LEDs
} psmove_instance_t;
_Static_assert(sizeof(psmove_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "PSMove intance too big");

// As defined here:
// https://github.com/ros-drivers/joystick_drivers/blob/52e8fcfb5619382a04756207b228fbc569f9a3ca/ps3joy/scripts/ps3joy_node.py#L276
typedef struct __attribute((packed)) {
    uint8_t transation_type;  // 0x52
    uint8_t report_id;        // 0x01
    uint8_t padding0;
    uint8_t motor_right_duration;  // 0xff == forever
    uint8_t motor_right_enabled;   // 0 or 1
    uint8_t motor_left_duration;   // 0xff == forever
    uint8_t motor_left_force;      // 0-255
    uint8_t padding1[4];
    uint8_t player_leds;
    uint8_t led4[5];  // 0xff, 0x27, 0x10, 0x00, 0x32,
    uint8_t led3[5];  // ditto above
    uint8_t led2[5];  // ditto above
    uint8_t led1[5];  // ditto above
    uint8_t reserved[5];
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
static void psmove_update_led(uni_hid_device_t* d, uint8_t player_leds);
static void psmove_send_output_report(uni_hid_device_t* d, psmove_output_report_t* out);

void uni_hid_parser_psmove_init_report(uni_hid_device_t* d) {
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));

    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_psmove_parse_input_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    // printf_hexdump(report, len);

    psmove_instance_t* ins = get_psmove_instance(d);
    if (ins->state == PSMOVE_FSM_REQUIRES_LED_UPDATE) {
        psmove_update_led(d, ins->player_leds);
    }

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
        ctl->gamepad.misc_buttons |= MISC_BUTTON_BACK;  // Select
    if (r->buttons[0] & 0x08)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_HOME;  // Start

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

void uni_hid_parser_psmove_set_player_leds(uni_hid_device_t* d, uint8_t leds) {
    psmove_instance_t* ins = get_psmove_instance(d);
    // Always update instance value. It could be used by rumble.
    ins->player_leds = leds;

    // It seems that if a LED update is sent before the "request stream report",
    // then the stream report is ignored.
    // Update LED after stream report is set.
    if (ins->state <= PSMOVE_FSM_REQUIRES_LED_UPDATE) {
        ins->state = PSMOVE_FSM_REQUIRES_LED_UPDATE;
        return;
    }
    psmove_update_led(d, leds);
}

void uni_hid_parser_psmove_set_rumble(uni_hid_device_t* d, uint8_t value, uint8_t duration) {
    if (duration == 0xff)
        duration = 0xfe;
    uint8_t right = !!value;
    uint8_t left = value;

    psmove_output_report_t out = {0};
    out.motor_right_duration = duration;
    out.motor_right_enabled = right;
    out.motor_left_duration = duration;
    out.motor_left_force = left;

    // Don't overwrite Player LEDs
    // LED cmd. LED1==2, LED2==4, etc...
    psmove_instance_t* ins = get_psmove_instance(d);
    out.player_leds = ins->player_leds << 1;

    psmove_send_output_report(d, &out);
}

void uni_hid_parser_psmove_setup(struct uni_hid_device_s* d) {
    psmove_instance_t* ins = get_psmove_instance(d);
    memset(ins, 0, sizeof(*ins));

    // Dual Shock 3 Sixasis requires a magic packet to be sent in order to enable reports. Taken from:
    // https://github.com/torvalds/linux/blob/1d1df41c5a33359a00e919d54eaebfb789711fdc/drivers/hid/hid-sony.c#L1684
    static uint8_t sixaxisEnableReports[] = {(HID_MESSAGE_TYPE_SET_REPORT << 4) | HID_REPORT_TYPE_FEATURE,
                                             0xf4,  // Report ID
                                             0x42,
                                             0x03,
                                             0x00,
                                             0x00};
    uni_hid_device_send_ctrl_report(d, (uint8_t*)&sixaxisEnableReports, sizeof(sixaxisEnableReports));

    // TODO: should set "ready_complete" once we receive an ack from psmove regaring report id 0xf4 (???)
    uni_hid_device_set_ready_complete(d);
}

//
// Helpers
//
static psmove_instance_t* get_psmove_instance(uni_hid_device_t* d) {
    return (psmove_instance_t*)&d->parser_data[0];
}

static void psmove_update_led(uni_hid_device_t* d, uint8_t player_leds) {
    psmove_instance_t* ins = get_psmove_instance(d);
    ins->state = PSMOVE_FSM_LED_UPDATED;

    psmove_output_report_t out = {0};

    // LED cmd. LED1==2, LED2==4, etc...
    out.player_leds = player_leds << 1;

    psmove_send_output_report(d, &out);
}

static void psmove_send_output_report(uni_hid_device_t* d, psmove_output_report_t* out) {
    out->transation_type = 0x52;  // SET_REPORT output
    out->report_id = 0x01;

    // Default LEDs configuration.
    static const uint8_t leds[] = {
        0xff, 0x27, 0x10, 0x00, 0x32,  // Led4
        0xff, 0x27, 0x10, 0x00, 0x32,  // Led3
        0xff, 0x27, 0x10, 0x00, 0x32,  // Led2
        0xff, 0x27, 0x10, 0x00, 0x32,  // Led1
    };
    memcpy(&out->led4, leds, sizeof(leds));

    uni_hid_device_send_ctrl_report(d, (uint8_t*)out, sizeof(*out));
}
