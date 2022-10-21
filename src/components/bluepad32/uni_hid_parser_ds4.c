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
// https://github.com/felis/USB_Host_Shield_2.0/blob/master/PS4Parser.h

#include "uni_hid_parser_ds4.h"

#include <assert.h>

#include "hid_usage.h"
#include "uni_config.h"
#include "uni_debug.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_utils.h"

#define DS4_FEATURE_REPORT_FIRMWARE_VERSION 0xa3
#define DS4_FEATURE_REPORT_FIRMWARE_VERSION_SIZE 49
#define DS4_FEATURE_REPORT_CALIBRATION 0x02
#define DS4_FEATURE_REPORT_CALIBRATION_SIZE 37

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

    // Add missing data
} ds4_input_report_t;

//Couldn't get this format to work so stuck with buttons[3]
// typedef union __attribute__((packed)) {
//         struct {
//                 uint8_t dpad : 4;
//                 uint8_t square : 1;
//                 uint8_t cross : 1;
//                 uint8_t circle : 1;
//                 uint8_t triangle : 1;

//                 uint8_t l1 : 1;
//                 uint8_t r1 : 1;
//                 uint8_t l2 : 1;
//                 uint8_t r2 : 1;
//                 uint8_t share : 1;
//                 uint8_t options : 1;
//                 uint8_t l3 : 1;
//                 uint8_t r3 : 1;

//                 uint8_t ps : 1;
//                 uint8_t touchpad : 1;
//                 uint8_t reportCounter : 6;
//         } __attribute__((packed));
//         uint32_t val : 24;
// } PS4Buttons;

typedef struct __attribute((packed)) {
        uint8_t dummy; // I can not figure out what this data is for, it seems to change randomly, maybe a timestamp?
        // struct {
        //         uint8_t counter : 7; // Increments every time a finger is touching the touchpad
        //         uint8_t touching : 1; // The top bit is cleared if the finger is touching the touchpad
        //         uint16_t x : 12;
        //         uint16_t y : 12;
        // } __attribute__((packed))
        uni_finger_t finger[2]; // 0 = first finger, 1 = second finger
} touchpadXY_t;

typedef struct __attribute((packed)) {
        uint8_t battery : 4;
        uint8_t usb : 1;
        uint8_t audio : 1;
        uint8_t mic : 1;
        uint8_t unknown : 1; // Extension port?
} PS4Status_t;

//Brought these structs from the felis/USB_Host_Shield: https://github.com/felis/USB_Host_Shield_2.0/blob/master/PS4Parser.h
typedef struct __attribute((packed)) {
        /* Button and joystick values */
        uint8_t hatValue[4];
        
        // PS4Buttons btn; //This union struct is not formatted right for this compiler yet
        // Hat + buttons
        uint8_t buttons[3];

        uint8_t trigger[2];

        /* Gyro and accelerometer values */
        uint8_t dummy[3]; // First two looks random, while the third one might be some kind of status - it increments once in a while
        int16_t gyroY, gyroZ, gyroX;
        int16_t accX, accZ, accY;

        uint8_t dummy2[5];
        uint8_t status; //Could use PS4Status_t but wasn't doing any processing on ESP side for this byte
        uint8_t dummy3[3];

        /* The rest is data for the touchpad */
        touchpadXY_t xy[3]; // It looks like it sends out three coordinates each time, this might be because the microcontroller inside the PS4 controller is much faster than the Bluetooth connection.
                          // The last data is read from the last position in the array while the oldest measurement is from the first position.
                          // The first position will also keep it's value after the finger is released, while the other two will set them to zero.
                          // Note that if you read fast enough from the device, then only the first one will contain any data.

        // The last three bytes are always: 0x00, 0x80, 0x00
} PS4Data_t;

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
    uni_gamepad_t* gp = &d->gamepad;
    memset(gp, 0, sizeof(*gp));

    // Only report 0x11 is supported which is a "full report". It is safe to set
    // the reported states just once, here:
    gp->updated_states = GAMEPAD_STATE_AXIS_X | GAMEPAD_STATE_AXIS_Y | GAMEPAD_STATE_AXIS_RX | GAMEPAD_STATE_AXIS_RY;
    gp->updated_states |= GAMEPAD_STATE_BRAKE | GAMEPAD_STATE_THROTTLE;
    gp->updated_states |= GAMEPAD_STATE_DPAD;
    gp->updated_states |=
        GAMEPAD_STATE_BUTTON_X | GAMEPAD_STATE_BUTTON_Y | GAMEPAD_STATE_BUTTON_A | GAMEPAD_STATE_BUTTON_B;
    gp->updated_states |= GAMEPAD_STATE_BUTTON_TRIGGER_L | GAMEPAD_STATE_BUTTON_TRIGGER_R |
                          GAMEPAD_STATE_BUTTON_SHOULDER_L | GAMEPAD_STATE_BUTTON_SHOULDER_R;
    gp->updated_states |= GAMEPAD_STATE_BUTTON_THUMB_L | GAMEPAD_STATE_BUTTON_THUMB_R;
    gp->updated_states |=
        GAMEPAD_STATE_MISC_BUTTON_BACK | GAMEPAD_STATE_MISC_BUTTON_HOME | GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
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
    uni_gamepad_t* gp = &d->gamepad;
    const PS4Data_t* r = (PS4Data_t*)&report[3];

    // Axis
    gp->LeftHatX = (r->hatValue[0]);
    gp->LeftHatY = (r->hatValue[1]);
    gp->RightHatX = (r->hatValue[2]);
    gp->RightHatY = (r->hatValue[3]);
    
    // Brake & throttle
    gp->LTrigger = r->trigger[0];
    gp->RTrigger = r->trigger[1];
   
    //Buttons
    gp->buttons2[0] = r->buttons[0];
    gp->buttons2[1] = r->buttons[1];
    gp->buttons2[2] = r->buttons[2] & 0x03; //Tossing out the report Counter, Only care about PS and Touchpad Button
    
    //Gyro/Accelerometer Data
    gp->gyroX = r->gyroX;
    gp->gyroY = r->gyroY;
    gp->gyroZ = r->gyroZ;
    gp->accX = r->accX;
    gp->accY = r->accY;
    gp->accZ = r->accZ;

    // //Touchpad Data
    gp->finger[0] = r->xy[0].finger[0];
    gp->finger[1] = r->xy[0].finger[1];

    // //Status Byte: Battery, Aud, Mic
    gp->status = r->status;

    // const ds4_input_report_t* r = (ds4_input_report_t*)&report[3];

    // Axis
    // gp->axis_x = (r->hatValue[0] - 127) * 4;
    // gp->axis_y = (r->hatValue[1] - 127) * 4;
    // gp->axis_rx = (r->hatValue[2] - 127) * 4;
    // gp->axis_ry = (r->hatValue[3] - 127) * 4;

    // Hat
    // uint8_t value = r->buttons[0] & 0xf;
    // if (value > 7)
    //     value = 0xff; /* Center 0, 0 */
    // gp->dpad = uni_hid_parser_hat_to_dpad(value);

    // Buttons
    // TODO: ds4, ds5 have these buttons in common. Refactor.
    // if (r->buttons[0] & 0x10)
    //     gp->buttons |= BUTTON_X;  // West
    // if (r->buttons[0] & 0x20)
    //     gp->buttons |= BUTTON_A;  // South
    // if (r->buttons[0] & 0x40)
    //     gp->buttons |= BUTTON_B;  // East
    // if (r->buttons[0] & 0x80)
    //     gp->buttons |= BUTTON_Y;  // North
    // if (r->buttons[1] & 0x01)
    //     gp->buttons |= BUTTON_SHOULDER_L;  // L1
    // if (r->buttons[1] & 0x02)
    //     gp->buttons |= BUTTON_SHOULDER_R;  // R1
    // if (r->buttons[1] & 0x04)
    //     gp->buttons |= BUTTON_TRIGGER_L;  // L2
    // if (r->buttons[1] & 0x08)
    //     gp->buttons |= BUTTON_TRIGGER_R;  // R2
    // if (r->buttons[1] & 0x10)
    //     gp->misc_buttons |= MISC_BUTTON_BACK;  // Share
    // if (r->buttons[1] & 0x20)
    //     gp->misc_buttons |= MISC_BUTTON_HOME;  // Options
    // if (r->buttons[1] & 0x40)
    //     gp->buttons |= BUTTON_THUMB_L;  // Thumb L
    // if (r->buttons[1] & 0x80)
    //     gp->buttons |= BUTTON_THUMB_R;  // Thumb R
    // if (r->buttons[2] & 0x01)
    //     gp->misc_buttons |= MISC_BUTTON_SYSTEM;  // PS
    // if (r->buttons[2] & 0x02)
    //     gp->misc_buttons |= MISC_BUTTON_TOUCHPAD;  // Touchpad Button //I'm not using but this looks like the format for general use

    // Brake & throttle
    // gp->brake = r->trigger[0] * 4;
    // gp->throttle = r->trigger[1] * 4;  
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
