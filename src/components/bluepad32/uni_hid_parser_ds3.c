/****************************************************************************
Original DS3 support by Marcelo Lorenzati mlorenzati@gmail.com
LEDs, rumble support + additional fixes by Ricardo Quesada

Technical info taken from:
https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c
https://github.com/libretro/RetroArch/blob/5166eebcaf82ad784fe4856791880014d78807bd/input/common/hid/device_ds3.c#L180
http://wiki.ros.org/ps3joy
https://github.com/ros-drivers/joystick_drivers/blob/master/ps3joy/scripts/ps3joy_node.py
https://github.com/felis/USB_Host_Shield_2.0/wiki/PS3-Information#Bluetooth
https://github.com/jvpernis/esp32-ps3

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
#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_log.h"

static const uint16_t DUALSHOCK3_VID = 0x054c;  // Sony
static const uint16_t DUALSHOCK3_PID = 0x0268;  // DualShock 3

// Required steps to determine what kind of extensions are supported.
typedef enum ds3_fsm {
    DS3_FSM_0,                    // Uninitialized
    DS3_FSM_REQUIRES_LED_UPDATE,  // Requires LED to be updated
    DS3_FSM_LED_UPDATED,          // LED updated
} ds3_fsm_t;

// ds3_instance_t represents data used by the DS3 driver instance.
typedef struct ds3_instance_s {
    ds3_fsm_t state;
    uint8_t player_leds;  // bitmap of LEDs
} ds3_instance_t;
_Static_assert(sizeof(ds3_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "DS3 intance too big");

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
} ds3_output_report_t;

typedef struct __attribute((packed)) {
    uint8_t report_id;
    uint8_t unk0;

    // Dpad, main buttons, misc buttons
    uint8_t buttons[3];

    uint8_t unk1;

    // Axis
    uint8_t x, y;  // 0-255, 128-middle
    uint8_t rx, ry;

    uint8_t unk2[4];

    // All these buttons have "pressure", a value that goes from 0 - 255

    uint8_t dpad_up;     // 0-255
    uint8_t dpad_right;  // 0-255
    uint8_t dpad_down;   // 0-255
    uint8_t dpad_left;   // 0-255

    uint8_t brake;     // l2
    uint8_t throttle;  // r2
    uint8_t l1;
    uint8_t r1;

    uint8_t north;  // triangle
    uint8_t east;   // circle
    uint8_t south;  // cross
    uint8_t west;   // square

    // Don't care about the following values, added for completeness.

    uint8_t unk3[3];

    uint8_t charge;  // charging status ? 02 = charge, 03 = normal
    uint8_t battery_status;
    uint8_t connection;  // connection type

    uint8_t unk4[9];

    uint16_t accel_x;
    uint16_t accel_y;
    uint16_t accel_z;
    uint16_t gyro_x;
} ds3_input_report_t;

static ds3_instance_t* get_ds3_instance(uni_hid_device_t* d);
static void ds3_update_led(uni_hid_device_t* d, uint8_t player_leds);
static void ds3_send_output_report(uni_hid_device_t* d, ds3_output_report_t* out);

void uni_hid_parser_ds3_init_report(uni_hid_device_t* d) {
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));

    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_ds3_parse_input_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    ds3_instance_t* ins = get_ds3_instance(d);
    if (ins->state == DS3_FSM_REQUIRES_LED_UPDATE) {
        ds3_update_led(d, ins->player_leds);
    }

    // Report len should be 49, at least in DS3. Not sure whether clones or older
    // sixaxis version report th same lenght. To be safe, only query about the
    // data that is going to be used.
    if (len < 30) {
        loge("ds3: Invalid report lenght, got: %d\n want: >= 29", len);
        return;
    }

    ds3_input_report_t* r = (ds3_input_report_t*)report;

    if (r->report_id != 0x01) {
        loge("ds3: Unexpected report_id, got: 0x%02x, want: 0x01\n", r->report_id);
        return;
    }

    uni_controller_t* ctl = &d->controller;

    // Axis
    ctl->gamepad.axis_x = (r->x - 127) * 4;
    ctl->gamepad.axis_y = (r->y - 127) * 4;
    ctl->gamepad.axis_rx = (r->rx - 127) * 4;
    ctl->gamepad.axis_ry = (r->ry - 127) * 4;

    // Brake & throttle
    ctl->gamepad.brake = r->brake * 4;
    ctl->gamepad.throttle = r->throttle * 4;

    // Buttons
    if (r->buttons[0] & 0x01)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_BACK;  // Select
    if (r->buttons[0] & 0x02)
        ctl->gamepad.buttons |= BUTTON_THUMB_L;  // Thumb L
    if (r->buttons[0] & 0x04)
        ctl->gamepad.buttons |= BUTTON_THUMB_R;  // Thumb R
    if (r->buttons[0] & 0x08)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_HOME;  // Start
    if (r->buttons[0] & 0x10)
        ctl->gamepad.dpad |= DPAD_UP;  // Dpad up
    if (r->buttons[0] & 0x20)
        ctl->gamepad.dpad |= DPAD_RIGHT;  // Dpad right
    if (r->buttons[0] & 0x40)
        ctl->gamepad.dpad |= DPAD_DOWN;  // Dpad down
    if (r->buttons[0] & 0x80)
        ctl->gamepad.dpad |= DPAD_LEFT;  // Dpad left
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
}

void uni_hid_parser_ds3_set_player_leds(uni_hid_device_t* d, uint8_t leds) {
    ds3_instance_t* ins = get_ds3_instance(d);
    // Always update instance value. It could be used by rumble.
    ins->player_leds = leds;

    // It seems that if a LED update is sent before the "request stream report",
    // then the stream report is ignored.
    // Update LED after stream report is set.
    if (ins->state <= DS3_FSM_REQUIRES_LED_UPDATE) {
        ins->state = DS3_FSM_REQUIRES_LED_UPDATE;
        return;
    }
    ds3_update_led(d, leds);
}

void uni_hid_parser_ds3_set_rumble(uni_hid_device_t* d, uint8_t value, uint8_t duration) {
    if (duration == 0xff)
        duration = 0xfe;
    uint8_t right = !!value;
    uint8_t left = value;

    ds3_output_report_t out = {0};
    out.motor_right_duration = duration;
    out.motor_right_enabled = right;
    out.motor_left_duration = duration;
    out.motor_left_force = left;

    // Don't overwrite Player LEDs
    // LED cmd. LED1==2, LED2==4, etc...
    ds3_instance_t* ins = get_ds3_instance(d);
    out.player_leds = ins->player_leds << 1;

    ds3_send_output_report(d, &out);
}

void uni_hid_parser_ds3_setup(struct uni_hid_device_s* d) {
    // Dual Shock 3 Sixasis requires a magic packet to be sent in order to enable reports. Taken from:
    // https://github.com/torvalds/linux/blob/1d1df41c5a33359a00e919d54eaebfb789711fdc/drivers/hid/hid-sony.c#L1684
    static uint8_t sixaxisEnableReports[] = {(HID_MESSAGE_TYPE_SET_REPORT << 4) | HID_REPORT_TYPE_FEATURE,
                                             0xf4,  // Report ID
                                             0x42,
                                             0x03,
                                             0x00,
                                             0x00};
    uni_hid_device_send_ctrl_report(d, (uint8_t*)&sixaxisEnableReports, sizeof(sixaxisEnableReports));

    // TODO: should set "ready_complete" once we receive an ack from DS3 regaring report id 0xf4 (???)
    uni_hid_device_set_ready_complete(d);
}

bool uni_hid_parser_ds3_does_name_match(struct uni_hid_device_s* d, const char* name) {
    // Matching names like:
    // - "PLAYSTATION(R)3 Controller"
    // - "PLAYSTATION(R)3Conteroller-PANHAI"
    if (strncmp("PLAYSTATION(R)3", name, 15) != 0)
        return false;

    // Fake PID/VID
    uni_hid_device_set_vendor_id(d, DUALSHOCK3_VID);
    uni_hid_device_set_product_id(d, DUALSHOCK3_PID);
    return true;
}

//
// Helpers
//
static ds3_instance_t* get_ds3_instance(uni_hid_device_t* d) {
    return (ds3_instance_t*)&d->parser_data[0];
}

static void ds3_update_led(uni_hid_device_t* d, uint8_t player_leds) {
    ds3_instance_t* ins = get_ds3_instance(d);
    ins->state = DS3_FSM_LED_UPDATED;

    ds3_output_report_t out = {0};

    // LED cmd. LED1==2, LED2==4, etc...
    out.player_leds = player_leds << 1;

    ds3_send_output_report(d, &out);
}

static void ds3_send_output_report(uni_hid_device_t* d, ds3_output_report_t* out) {
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
