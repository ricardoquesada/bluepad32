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

// More info about Xbox One gamepad:
// https://support.xbox.com/en-US/xbox-one/accessories/xbox-one-wireless-controller

// Technical info taken from:
// https://github.com/atar-axis/xpadneo/blob/master/hid-xpadneo/src/hid-xpadneo.c

#include "uni_hid_parser_xboxone.h"

#include "hid_usage.h"
#include "uni_controller.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_log.h"

// Supported Xbox One firmware revisions.
// Probably there are more revisions, but I only found two in the "wild".
enum xboxone_firmware {
    // The one that came pre-installed, or close to it.
    XBOXONE_FIRMWARE_V3_1,
    // The one released in 2019-10
    XBOXONE_FIRMWARE_V4_8,
};

// xboxone_instance_t represents data used by the Wii driver instance.
typedef struct wii_instance_s {
    enum xboxone_firmware version;
} xboxone_instance_t;
_Static_assert(sizeof(xboxone_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "Xbox one intance too big");

static xboxone_instance_t* get_xboxone_instance(uni_hid_device_t* d);
static void parse_usage_firmware_v3_1(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page,
                                      uint16_t usage,
                                      int32_t value);
static void parse_usage_firmware_v4_8(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page,
                                      uint16_t usage,
                                      int32_t value);

void uni_hid_parser_xboxone_setup(uni_hid_device_t* d) {
    xboxone_instance_t* ins = get_xboxone_instance(d);
    // FIXME: Parse HID descriptor and see if it supports 0xf buttons. Checking
    // for the len is a horrible hack.
    if (d->hid_descriptor_len > 330) {
        logi("Xbox one: Assuming it is firmware 4.8\n");
        ins->version = XBOXONE_FIRMWARE_V4_8;
    } else {
        // It is really firmware 4.8, it will be set later
        logi("Xbox one: Assuming it is firmware 3.1\n");
        ins->version = XBOXONE_FIRMWARE_V3_1;
    }

    uni_hid_device_set_ready_complete(d);
}

void uni_hid_parser_xboxone_init_report(uni_hid_device_t* d) {
    // Reset old state. Each report contains a full-state.
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));

    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_xboxone_parse_usage(uni_hid_device_t* d,
                                        hid_globals_t* globals,
                                        uint16_t usage_page,
                                        uint16_t usage,
                                        int32_t value) {
    xboxone_instance_t* ins = get_xboxone_instance(d);
    if (ins->version == XBOXONE_FIRMWARE_V3_1) {
        parse_usage_firmware_v3_1(d, globals, usage_page, usage, value);
    } else {
        parse_usage_firmware_v4_8(d, globals, usage_page, usage, value);
    }
}

static void parse_usage_firmware_v3_1(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page,
                                      uint16_t usage,
                                      int32_t value) {
    uint8_t hat;
    uni_controller_t* ctl = &d->controller;

    switch (usage_page) {
        case HID_USAGE_PAGE_GENERIC_DESKTOP:
            switch (usage) {
                case HID_USAGE_AXIS_X:
                    ctl->gamepad.axis_x = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_Y:
                    ctl->gamepad.axis_y = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_Z:
                    ctl->gamepad.brake = uni_hid_parser_process_pedal(globals, value);
                    break;
                case HID_USAGE_AXIS_RX:
                    ctl->gamepad.axis_rx = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_RY:
                    ctl->gamepad.axis_ry = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_RZ:
                    ctl->gamepad.throttle = uni_hid_parser_process_pedal(globals, value);
                    break;
                case HID_USAGE_HAT:
                    hat = uni_hid_parser_process_hat(globals, value);
                    ctl->gamepad.dpad = uni_hid_parser_hat_to_dpad(hat);
                    break;
                case HID_USAGE_SYSTEM_MAIN_MENU:
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
                    break;
                case HID_USAGE_DPAD_UP:
                case HID_USAGE_DPAD_DOWN:
                case HID_USAGE_DPAD_RIGHT:
                case HID_USAGE_DPAD_LEFT:
                    uni_hid_parser_process_dpad(usage, value, &ctl->gamepad.dpad);
                    break;
                default:
                    logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS:
            switch (usage) {
                case HID_USAGE_BATTERY_STRENGTH:
                    ctl->battery = value;
                    break;
                default:
                    logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_BUTTON: {
            switch (usage) {
                case 0x01:  // Button A
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_A;
                    break;
                case 0x02:  // Button B
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_B;
                    break;
                case 0x03:  // Button X
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_X;
                    break;
                case 0x04:  // Button Y
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_Y;
                    break;
                case 0x05:  // Button Left
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_SHOULDER_L;
                    break;
                case 0x06:  // Button Right
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_SHOULDER_R;
                    break;
                case 0x07:  // View button
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_BACK;
                    break;
                case 0x08:  // Menu button
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_HOME;
                    break;
                case 0x09:  // Thumb left
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_THUMB_L;
                    break;
                case 0x0a:  // Thumb right
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_THUMB_R;
                    break;
                case 0x0f: {
                    // Only available in firmware v4.8.
                    xboxone_instance_t* ins = get_xboxone_instance(d);
                    ins->version = XBOXONE_FIRMWARE_V4_8;
                    logi("Xbox one: Firmware 4.8 detected\n");
                    break;
                }
                default:
                    logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }

        case HID_USAGE_PAGE_CONSUMER:
            // New in Xbox One firmware v4.8
            switch (usage) {
                case HID_USAGE_RECORD:  // FW 5.15.5
                    break;
                case HID_USAGE_AC_BACK:
                    break;
                default:
                    logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        // unknown usage page
        default:
            logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
            break;
    }
}

// v4.8 is almost identical to the Android mappings.
static void parse_usage_firmware_v4_8(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page,
                                      uint16_t usage,
                                      int32_t value) {
    uint8_t hat;
    uni_controller_t* ctl = &d->controller;

    switch (usage_page) {
        case HID_USAGE_PAGE_GENERIC_DESKTOP:
            switch (usage) {
                case HID_USAGE_AXIS_X:
                    ctl->gamepad.axis_x = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_Y:
                    ctl->gamepad.axis_y = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_Z:
                    ctl->gamepad.axis_rx = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_AXIS_RZ:
                    ctl->gamepad.axis_ry = uni_hid_parser_process_axis(globals, value);
                    break;
                case HID_USAGE_HAT:
                    hat = uni_hid_parser_process_hat(globals, value);
                    ctl->gamepad.dpad = uni_hid_parser_hat_to_dpad(hat);
                    break;
                case HID_USAGE_SYSTEM_MAIN_MENU:
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
                    break;
                case HID_USAGE_DPAD_UP:
                case HID_USAGE_DPAD_DOWN:
                case HID_USAGE_DPAD_RIGHT:
                case HID_USAGE_DPAD_LEFT:
                    uni_hid_parser_process_dpad(usage, value, &ctl->gamepad.dpad);
                    break;
                default:
                    logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_SIMULATION_CONTROLS:
            switch (usage) {
                case 0xc4:  // Accelerator
                    ctl->gamepad.throttle = uni_hid_parser_process_pedal(globals, value);
                    break;
                case 0xc5:  // Brake
                    ctl->gamepad.brake = uni_hid_parser_process_pedal(globals, value);
                    break;
                default:
                    logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS:
            switch (usage) {
                case HID_USAGE_BATTERY_STRENGTH:
                    ctl->battery = value;
                    break;
                default:
                    logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_BUTTON: {
            switch (usage) {
                case 0x01:  // Button A
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_A;
                    break;
                case 0x02:  // Button B
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_B;
                    break;
                case 0x03:  // Unused
                    break;
                case 0x04:  // Button X
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_X;
                    break;
                case 0x05:  // Button Y
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_Y;
                    break;
                case 0x06:  // Unused
                    break;
                case 0x07:  // Shoulder Left
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_SHOULDER_L;
                    break;
                case 0x08:  // Shoulder Right
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_SHOULDER_R;
                    break;
                case 0x09:  // Unused
                case 0x0a:  // Unused
                case 0x0b:  // Unused
                    break;
                case 0x0c:  // Burger button
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_HOME;
                    break;
                case 0x0d:  // Xbox button
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
                    break;
                case 0x0e:  // Thumb Left
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_THUMB_L;
                    break;
                case 0x0f:  // Thumb Right
                    if (value)
                        ctl->gamepad.buttons |= BUTTON_THUMB_R;
                    break;
                default:
                    logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }

        case HID_USAGE_PAGE_CONSUMER:
            switch (usage) {
                case HID_USAGE_RECORD:
                    // FW 5.15.5
                    break;
                case HID_USAGE_AC_BACK:  // Back
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_BACK;
                    break;
                default:
                    logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        // unknown usage page
        default:
            logi("Xbox One: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
            break;
    }
}

void uni_hid_parser_xboxone_set_rumble(uni_hid_device_t* d, uint8_t value, uint8_t duration) {
    if (d == NULL) {
        loge("Xbox One: Invalid device\n");
        return;
    }

    // Actuators for the force feedback (FF).
    enum {
        FF_RIGHT = 1 << 0,
        FF_LEFT = 1 << 1,
        FF_TRIGGER_RIGHT = 1 << 2,
        FF_TRIGGER_LEFT = 1 << 3,
    };

    struct ff_report {
        // Report related
        uint8_t transaction_type;  // type of transaction
        uint8_t report_id;         // report id
        // Force-feedback related
        uint8_t enable_actuators;    // LSB 0-3 for each actuator
        uint8_t force_left_trigger;  // HID descriptor says that it goes from 0-100
        uint8_t force_right_trigger;
        uint8_t force_left;
        uint8_t force_right;
        uint8_t duration;  // unknown unit, 255 is ~second
        uint8_t start_delay;
        uint8_t loop_count;  // how many times "duration" is repeated
    } __attribute__((packed));

    // TODO: It seems that the max value is 127. Double check
    value /= 2;

    struct ff_report ff = {
        .transaction_type = (HID_MESSAGE_TYPE_DATA << 4) | HID_REPORT_TYPE_OUTPUT,
        .report_id = 0x03,  // taken from HID descriptor
        .enable_actuators = FF_RIGHT | FF_LEFT | FF_TRIGGER_LEFT | FF_TRIGGER_RIGHT,
        .force_left_trigger = value,
        .force_right_trigger = value,
        // Don't enable force_left/force_right actuators.
        // They keep vibrating forever on some 8BitDo controllers
        // https://gitlab.com/ricardoquesada/unijoysticle2/-/issues/10
        .force_left = 0,
        .force_right = 0,
        .duration = duration,
        .start_delay = 0,
        .loop_count = 0,
    };

    uni_hid_device_send_intr_report(d, (uint8_t*)&ff, sizeof(ff));
}

//
// Helpers
//
xboxone_instance_t* get_xboxone_instance(uni_hid_device_t* d) {
    return (xboxone_instance_t*)&d->parser_data[0];
}
