// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

// More info about Xbox One gamepad:
// https://support.xbox.com/en-US/xbox-one/accessories/xbox-one-wireless-controller

// Technical info taken from:
// https://github.com/atar-axis/xpadneo/blob/master/hid-xpadneo/src/hid-xpadneo.c

#include "parser/uni_hid_parser_xboxone.h"

#include "controller/uni_controller.h"
#include "hid_usage.h"
#include "uni_hid_device.h"
#include "uni_log.h"

// Xbox doesn't report trigger buttons. Instead, it reports throttle/brake.
// This threshold represents the minimum value of throttle/brake to "report"
// the trigger buttons.
#define TRIGGER_BUTTON_THRESHOLD 32

#define XBOX_RUMBLE_REPORT_ID 0x03

#define BLE_RETRY_MS 50

static const uint16_t XBOX_WIRELESS_VID = 0x045e;  // Microsoft
static const uint16_t XBOX_WIRELESS_PID = 0x02e0;  // Xbox One (Bluetooth)

static const uint8_t xbox_hid_descriptor_4_8_fw[] = {
    0x05, 0x01, 0x09, 0x05, 0xa1, 0x01, 0x85, 0x01, 0x09, 0x01, 0xa1, 0x00, 0x09, 0x30, 0x09, 0x31, 0x15, 0x00, 0x27,
    0xff, 0xff, 0x00, 0x00, 0x95, 0x02, 0x75, 0x10, 0x81, 0x02, 0xc0, 0x09, 0x01, 0xa1, 0x00, 0x09, 0x32, 0x09, 0x35,
    0x15, 0x00, 0x27, 0xff, 0xff, 0x00, 0x00, 0x95, 0x02, 0x75, 0x10, 0x81, 0x02, 0xc0, 0x05, 0x02, 0x09, 0xc5, 0x15,
    0x00, 0x26, 0xff, 0x03, 0x95, 0x01, 0x75, 0x0a, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x06, 0x95, 0x01, 0x81,
    0x03, 0x05, 0x02, 0x09, 0xc4, 0x15, 0x00, 0x26, 0xff, 0x03, 0x95, 0x01, 0x75, 0x0a, 0x81, 0x02, 0x15, 0x00, 0x25,
    0x00, 0x75, 0x06, 0x95, 0x01, 0x81, 0x03, 0x05, 0x01, 0x09, 0x39, 0x15, 0x01, 0x25, 0x08, 0x35, 0x00, 0x46, 0x3b,
    0x01, 0x66, 0x14, 0x00, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42, 0x75, 0x04, 0x95, 0x01, 0x15, 0x00, 0x25, 0x00, 0x35,
    0x00, 0x45, 0x00, 0x65, 0x00, 0x81, 0x03, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0f, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
    0x95, 0x0f, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x01, 0x95, 0x01, 0x81, 0x03, 0x05, 0x0c, 0x0a, 0x24, 0x02,
    0x15, 0x00, 0x25, 0x01, 0x95, 0x01, 0x75, 0x01, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x07, 0x95, 0x01, 0x81,
    0x03, 0x05, 0x0c, 0x09, 0x01, 0x85, 0x02, 0xa1, 0x01, 0x05, 0x0c, 0x0a, 0x23, 0x02, 0x15, 0x00, 0x25, 0x01, 0x95,
    0x01, 0x75, 0x01, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x07, 0x95, 0x01, 0x81, 0x03, 0xc0, 0x05, 0x0f, 0x09,
    0x21, 0x85, 0x03, 0xa1, 0x02, 0x09, 0x97, 0x15, 0x00, 0x25, 0x01, 0x75, 0x04, 0x95, 0x01, 0x91, 0x02, 0x15, 0x00,
    0x25, 0x00, 0x75, 0x04, 0x95, 0x01, 0x91, 0x03, 0x09, 0x70, 0x15, 0x00, 0x25, 0x64, 0x75, 0x08, 0x95, 0x04, 0x91,
    0x02, 0x09, 0x50, 0x66, 0x01, 0x10, 0x55, 0x0e, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02,
    0x09, 0xa7, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02, 0x65, 0x00, 0x55, 0x00, 0x09, 0x7c,
    0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02, 0xc0, 0x05, 0x06, 0x09, 0x20, 0x85, 0x04, 0x15,
    0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x01, 0x81, 0x02, 0xc0,
};

// Supported Xbox One firmware revisions.
// Probably there are more revisions, but I only found two in the "wild".
enum xboxone_firmware {
    XBOXONE_FIRMWARE_V3_1,  // The one that came pre-installed, or close to it.
    XBOXONE_FIRMWARE_V4_8,  // The one released in 2019-10
    XBOXONE_FIRMWARE_V5,    // BLE version
};

// Actuators for the force feedback (FF).
enum {
    XBOXONE_FF_WEAK = BIT(0),
    XBOXONE_FF_STRONG = BIT(1),
    XBOXONE_FF_TRIGGER_RIGHT = BIT(2),
    XBOXONE_FF_TRIGGER_LEFT = BIT(3),
};

typedef enum {
    XBOXONE_STATE_RUMBLE_DISABLED,
    XBOXONE_STATE_RUMBLE_DELAYED,
    XBOXONE_STATE_RUMBLE_IN_PROGRESS,
} xboxone_state_rumble_t;

typedef enum {
    XBOXONE_RETRY_CMD_RUMBLE_ON,
    XBOXONE_RETRY_CMD_RUMBLE_OFF,
} xboxone_retry_cmd_t;

struct xboxone_ff_report {
    // Report related
    uint8_t transaction_type;  // type of transaction
    uint8_t report_id;         // report id, should be XBOX_RUMBLE_REPORT_ID

    // Force-feedback related
    uint8_t enable_actuators;        // LSB 0-3 for each actuator
    uint8_t magnitude_left_trigger;  // HID descriptor says that it goes from 0-100
    uint8_t magnitude_right_trigger;
    uint8_t magnitude_strong;
    uint8_t magnitude_weak;
    uint8_t duration_10ms;
    uint8_t start_delay_10ms;
    uint8_t loop_count;  // how many times "duration" is repeated
} __attribute__((packed));

// xboxone_instance_t represents data used by the Xbox driver instance.
typedef struct xboxone_instance_s {
    enum xboxone_firmware version;
    // Cannot use the Xbox duration field because 8BitDo controllers keep rumbling forever.
    // So we use a timer to cancel the rumbling after "duration".
    // https://gitlab.com/ricardoquesada/unijoysticle2/-/issues/10
    // https://github.com/ricardoquesada/bluepad32/issues/85
    // We also use a delayed timer, instead of the internal Xbox delay. More compatible.
    btstack_timer_source_t rumble_timer_duration;
    btstack_timer_source_t rumble_timer_delayed_start;
    xboxone_state_rumble_t rumble_state;

    // Used by delayed start
    uint16_t rumble_duration_ms;
    uint8_t rumble_trigger_left;
    uint8_t rumble_trigger_right;
    uint8_t rumble_weak_magnitude;
    uint8_t rumble_strong_magnitude;

} xboxone_instance_t;
_Static_assert(sizeof(xboxone_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "Xbox one instance too big");

static xboxone_instance_t* get_xboxone_instance(uni_hid_device_t* d);
static void on_xboxone_set_rumble_on(btstack_timer_source_t* ts);
static void on_xboxone_set_rumble_off(btstack_timer_source_t* ts);
static void xboxone_stop_rumble_now(uni_hid_device_t* d);
static void xboxone_play_quad_rumble_now(uni_hid_device_t* d,
                                         uint16_t duration_ms,
                                         uint8_t trigger_left,
                                         uint8_t trigger_right,
                                         uint8_t weak_magnitude,
                                         uint8_t strong_magnitude);
static void xboxone_retry_cmd(uni_hid_device_t* d, xboxone_retry_cmd_t cmd);
static void parse_usage_firmware_v3_1(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page,
                                      uint16_t usage,
                                      int32_t value);
static void parse_usage_firmware_v4_v5(uni_hid_device_t* d,
                                       hid_globals_t* globals,
                                       uint16_t usage_page,
                                       uint16_t usage,
                                       int32_t value);

// Needed for the GameSir T3s controller when put in iOS mode, which is basically impersonating an
// Xbox Wireless controller with FW 4.8.
bool uni_hid_parser_xboxone_does_name_match(struct uni_hid_device_s* d, const char* name) {
    if (!name)
        return false;

    if (strncmp("Xbox Wireless Controller", name, 25) != 0)
        return false;

    // Fake PID/VID
    uni_hid_device_set_vendor_id(d, XBOX_WIRELESS_VID);
    uni_hid_device_set_product_id(d, XBOX_WIRELESS_PID);
    // Fake HID
    uni_hid_device_set_hid_descriptor(d, xbox_hid_descriptor_4_8_fw, sizeof(xbox_hid_descriptor_4_8_fw));
    return true;
}

void uni_hid_parser_xboxone_setup(uni_hid_device_t* d) {
    xboxone_instance_t* ins = get_xboxone_instance(d);
    // FIXME: Parse HID descriptor and see if it supports 0xf buttons. Checking
    // for the len is a horrible hack.
    if (gap_get_connection_type(d->conn.handle) == GAP_CONNECTION_LE) {
        logi("Xbox: Assuming it is firmware v5.x\n");
        ins->version = XBOXONE_FIRMWARE_V5;
    } else if (d->hid_descriptor_len > 330) {
        logi("Xbox: Assuming it is firmware v4.8\n");
        ins->version = XBOXONE_FIRMWARE_V4_8;
    } else {
        // If it is really firmware 4.8, it will be set later.
        logi("Xbox: Assuming it is firmware v3.1\n");
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
        // Valid for v4 and v5
        parse_usage_firmware_v4_v5(d, globals, usage_page, usage, value);
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
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS:
            switch (usage) {
                case HID_USAGE_BATTERY_STRENGTH:
                    ctl->battery = value;
                    break;
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
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
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;
                    break;
                case 0x08:  // Menu button
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_START;
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
                    // Only available in firmware v4.8 / 5.x
                    xboxone_instance_t* ins = get_xboxone_instance(d);
                    ins->version = XBOXONE_FIRMWARE_V4_8;
                    logi("Xbox: Firmware 4.8 / 5.x detected\n");
                    break;
                }
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
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
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        // unknown usage page
        default:
            logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
            break;
    }
}

// v4.8 / 5.x are almost identical to the Android mappings.
static void parse_usage_firmware_v4_v5(uni_hid_device_t* d,
                                       hid_globals_t* globals,
                                       uint16_t usage_page,
                                       uint16_t usage,
                                       int32_t value) {
    uint8_t hat;
    uni_controller_t* ctl = &d->controller;

    xboxone_instance_t* ins = get_xboxone_instance(d);

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
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_SIMULATION_CONTROLS:
            switch (usage) {
                case 0xc4:  // Accelerator
                    ctl->gamepad.throttle = uni_hid_parser_process_pedal(globals, value);
                    if (ctl->gamepad.throttle >= TRIGGER_BUTTON_THRESHOLD)
                        ctl->gamepad.buttons |= BUTTON_TRIGGER_R;
                    break;
                case 0xc5:  // Brake
                    ctl->gamepad.brake = uni_hid_parser_process_pedal(globals, value);
                    if (ctl->gamepad.brake >= TRIGGER_BUTTON_THRESHOLD)
                        ctl->gamepad.buttons |= BUTTON_TRIGGER_L;
                    break;
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        case HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS:
            switch (usage) {
                case HID_USAGE_BATTERY_STRENGTH:
                    ctl->battery = value;
                    break;
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
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
                    break;
                case 0x0b:  // Unused in v4.8, used in v5.x
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;
                    break;
                case 0x0c:  // Burger button
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_START;
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
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }

        case HID_USAGE_PAGE_CONSUMER:
            switch (usage) {
                case HID_USAGE_RECORD:
                    // Model 1914: Share button
                    // Model 1708: reports it but always 0
                    // FW 5.x
                    if (ins->version != XBOXONE_FIRMWARE_V5) {
                        ins->version = XBOXONE_FIRMWARE_V5;
                        logi("Xbox: Assuming it is firmware v5.x\n");
                    }
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_CAPTURE;
                    break;
                case HID_USAGE_AC_BACK:  // Back in v4.8 (not v5.x)
                    if (value)
                        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;
                    break;
                case HID_USAGE_ASSIGN_SELECTION:
                case HID_USAGE_ORDER_MOVIE:
                case HID_USAGE_MEDIA_SELECT_SECURITY:
                    // Xbox Adaptive Controller
                    // Don't know the purpose of these "usages".
                    break;
                default:
                    logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;

        // unknown usage page
        default:
            logi("Xbox: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
            break;
    }
}

void uni_hid_parser_xboxone_play_dual_rumble(struct uni_hid_device_s* d,
                                             uint16_t start_delay_ms,
                                             uint16_t duration_ms,
                                             uint8_t weak_magnitude,
                                             uint8_t strong_magnitude) {
    xboxone_play_quad_rumble(d, start_delay_ms, duration_ms, 0 /* left trigger */, 0 /* right trigger */,
                             weak_magnitude, strong_magnitude);
}

void xboxone_play_quad_rumble(struct uni_hid_device_s* d,
                              uint16_t start_delay_ms,
                              uint16_t duration_ms,
                              uint8_t left_trigger,
                              uint8_t right_trigger,
                              uint8_t weak_magnitude,
                              uint8_t strong_magnitude) {
    if (d == NULL) {
        loge("Xbox: Invalid device\n");
        return;
    }

    xboxone_instance_t* ins = get_xboxone_instance(d);
    switch (ins->rumble_state) {
        case XBOXONE_STATE_RUMBLE_DELAYED:
            btstack_run_loop_remove_timer(&ins->rumble_timer_delayed_start);
            break;
        case XBOXONE_STATE_RUMBLE_IN_PROGRESS:
            btstack_run_loop_remove_timer(&ins->rumble_timer_duration);
            break;
        default:
            // Do nothing
            break;
    }

    if (start_delay_ms == 0) {
        xboxone_play_quad_rumble_now(d, duration_ms, left_trigger, right_trigger, weak_magnitude, strong_magnitude);
    } else {
        // Set timer to have a delayed start
        ins->rumble_timer_delayed_start.process = &on_xboxone_set_rumble_on;
        ins->rumble_timer_delayed_start.context = d;
        ins->rumble_state = XBOXONE_STATE_RUMBLE_DELAYED;
        ins->rumble_duration_ms = duration_ms;
        ins->rumble_trigger_left = left_trigger;
        ins->rumble_trigger_right = right_trigger;
        ins->rumble_strong_magnitude = strong_magnitude;
        ins->rumble_weak_magnitude = weak_magnitude;

        btstack_run_loop_set_timer(&ins->rumble_timer_delayed_start, start_delay_ms);
        btstack_run_loop_add_timer(&ins->rumble_timer_delayed_start);
    }
}

void uni_hid_parser_xboxone_device_dump(uni_hid_device_t* d) {
    static const char* versions[] = {
        "v3.1",
        "v4.8",
        "v5.x",
    };
    xboxone_instance_t* ins = get_xboxone_instance(d);
    if (ins->version >= 0 && ins->version < ARRAY_SIZE(versions))
        logi("\tXbox: FW version %s\n", versions[ins->version]);
}

//
// Helpers
//
xboxone_instance_t* get_xboxone_instance(uni_hid_device_t* d) {
    return (xboxone_instance_t*)&d->parser_data[0];
}

static void xboxone_retry_cmd(uni_hid_device_t* d, xboxone_retry_cmd_t cmd) {
    xboxone_instance_t* ins = get_xboxone_instance(d);
    ins->rumble_timer_delayed_start.context = d;

    switch (cmd) {
        case XBOXONE_RETRY_CMD_RUMBLE_ON:
            ins->rumble_timer_delayed_start.process = &on_xboxone_set_rumble_on;
            ins->rumble_state = XBOXONE_STATE_RUMBLE_DELAYED;
            btstack_run_loop_set_timer(&ins->rumble_timer_delayed_start, BLE_RETRY_MS);
            btstack_run_loop_add_timer(&ins->rumble_timer_delayed_start);
            break;
        case XBOXONE_RETRY_CMD_RUMBLE_OFF:
            ins->rumble_timer_duration.process = &on_xboxone_set_rumble_off;
            ins->rumble_state = XBOXONE_STATE_RUMBLE_IN_PROGRESS;
            btstack_run_loop_set_timer(&ins->rumble_timer_duration, BLE_RETRY_MS);
            btstack_run_loop_add_timer(&ins->rumble_timer_duration);
            break;
        default:
            break;
    }
}

static void xboxone_stop_rumble_now(uni_hid_device_t* d) {
    uint8_t status;
    xboxone_instance_t* ins = get_xboxone_instance(d);

    // No need to protect it with a mutex since it runs in the same main thread
    assert(ins->rumble_state != XBOXONE_STATE_RUMBLE_DISABLED);
    ins->rumble_state = XBOXONE_STATE_RUMBLE_DISABLED;

    struct xboxone_ff_report ff = {
        .transaction_type = (HID_MESSAGE_TYPE_DATA << 4) | HID_REPORT_TYPE_OUTPUT,
        .report_id = XBOX_RUMBLE_REPORT_ID,
        .enable_actuators = XBOXONE_FF_TRIGGER_LEFT | XBOXONE_FF_TRIGGER_RIGHT | XBOXONE_FF_WEAK | XBOXONE_FF_STRONG,
        .magnitude_left_trigger = 0,
        .magnitude_right_trigger = 0,
        .magnitude_strong = 0,
        .magnitude_weak = 0,
        .duration_10ms = 0,
        .start_delay_10ms = 0,
        .loop_count = 0,
    };

    if (ins->version == XBOXONE_FIRMWARE_V5) {
        status = hids_client_send_write_report(d->hids_cid, XBOX_RUMBLE_REPORT_ID, HID_REPORT_TYPE_OUTPUT,
                                               &ff.enable_actuators,  // skip the first type bytes,
                                               sizeof(ff) - 2         // subtract the 2 bytes from total
        );
        if (status == ERROR_CODE_COMMAND_DISALLOWED) {
            logd("Xbox: Failed to turn off rumble, error=%#x, retrying...\n", status);
            xboxone_retry_cmd(d, XBOXONE_RETRY_CMD_RUMBLE_OFF);
            return;
        } else if (status != ERROR_CODE_SUCCESS) {
            // Do nothing, log the error
            logi("Xbox: Failed to turn off rumble, error=%#x\n", status);
        }
        // else, SUCCESS
    } else {
        uni_hid_device_send_intr_report(d, (uint8_t*)&ff, sizeof(ff));
    }
}

static void xboxone_play_quad_rumble_now(uni_hid_device_t* d,
                                         uint16_t duration_ms,
                                         uint8_t left_trigger,
                                         uint8_t right_trigger,
                                         uint8_t weak_magnitude,
                                         uint8_t strong_magnitude) {
    uint8_t status;
    uint8_t mask = 0;

    xboxone_instance_t* ins = get_xboxone_instance(d);

    if (duration_ms == 0) {
        if (ins->rumble_state != XBOXONE_STATE_RUMBLE_DISABLED)
            xboxone_stop_rumble_now(d);
        return;
    }

    mask |= (left_trigger != 0) ? XBOXONE_FF_TRIGGER_LEFT : 0;
    mask |= (right_trigger != 0) ? XBOXONE_FF_TRIGGER_RIGHT : 0;
    mask |= (weak_magnitude != 0) ? XBOXONE_FF_WEAK : 0;
    mask |= (strong_magnitude != 0) ? XBOXONE_FF_STRONG : 0;

    logd("xbox rumble: duration=%d, left=%d, right=%d, weak=%d, strong=%d, mask=%#x\n", duration_ms, left_trigger,
         right_trigger, weak_magnitude, strong_magnitude, mask);

    // Magnitude is 0..100 so scale the 8-bit input here

    struct xboxone_ff_report ff = {
        .transaction_type = (HID_MESSAGE_TYPE_DATA << 4) | HID_REPORT_TYPE_OUTPUT,
        .report_id = XBOX_RUMBLE_REPORT_ID,
        .enable_actuators = mask,
        .magnitude_left_trigger = ((uint16_t)(left_trigger * 100)) / UINT8_MAX,
        .magnitude_right_trigger = ((uint16_t)(right_trigger * 100)) / UINT8_MAX,
        .magnitude_strong = ((uint16_t)(strong_magnitude * 100)) / UINT8_MAX,
        .magnitude_weak = ((uint16_t)(weak_magnitude * 100)) / UINT8_MAX,
        .duration_10ms = 0xff,  // forever, timer will turn it off
        .start_delay_10ms = 0,
        .loop_count = 25,  // timer will turn it off, but in case it fails, limit it to no more than
                           // the max 65535 ms accepted for duration: 255 * 10ms * 26 = 66300ms
    };

    if (ins->version == XBOXONE_FIRMWARE_V5) {
        status = hids_client_send_write_report(d->hids_cid, XBOX_RUMBLE_REPORT_ID, HID_REPORT_TYPE_OUTPUT,
                                               &ff.enable_actuators,  // skip the first two bytes,
                                               sizeof(ff) - 2         // subtract the two bytes from total
        );
        if (status == ERROR_CODE_COMMAND_DISALLOWED) {
            logd("Xbox: Failed to send rumble report, error=%#x, retrying...\n", status);
            xboxone_retry_cmd(d, XBOXONE_RETRY_CMD_RUMBLE_ON);
            return;
        } else if (status != ERROR_CODE_SUCCESS) {
            // Don't retry, log the error and return
            logi("Xbox: Failed to send rumble report, error=%#x\n", status);
            return;
        }
    } else {
        uni_hid_device_send_intr_report(d, (uint8_t*)&ff, sizeof(ff));
    }

    // Set timer to turn off rumble
    ins->rumble_timer_duration.process = &on_xboxone_set_rumble_off;
    ins->rumble_timer_duration.context = d;
    ins->rumble_state = XBOXONE_STATE_RUMBLE_IN_PROGRESS;

    btstack_run_loop_set_timer(&ins->rumble_timer_duration, duration_ms);
    btstack_run_loop_add_timer(&ins->rumble_timer_duration);
}

static void on_xboxone_set_rumble_on(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    xboxone_instance_t* ins = get_xboxone_instance(d);

    xboxone_play_quad_rumble_now(d, ins->rumble_duration_ms, ins->rumble_trigger_left, ins->rumble_trigger_right,
                                 ins->rumble_weak_magnitude, ins->rumble_strong_magnitude);
}

static void on_xboxone_set_rumble_off(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    xboxone_stop_rumble_now(d);
}
