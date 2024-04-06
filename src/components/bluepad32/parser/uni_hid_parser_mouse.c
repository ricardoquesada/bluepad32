// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "parser/uni_hid_parser_mouse.h"

#include <math.h>
#include <time.h>

#include "controller/uni_controller.h"
#include "hid_usage.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

#define TANK_MOUSE_VID 0x248a
#define TANK_MOUSE_PID 0x8266

typedef struct __attribute((packed)) tank_mouse_input_report {
    uint8_t report_id;  // Should be 0x03
    uint8_t buttons;
    int16_t delta_x;
    int16_t delta_y;
    uint8_t scroll_wheel;
} tank_mouse_input_report_t;

typedef struct {
    float scale;
} mouse_instance_t;
_Static_assert(sizeof(mouse_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "Mouse intance too big");

struct mouse_resolution {
    uint16_t vid;
    uint16_t pid;
    // If name is NULL, it means it should not be used.
    const char* name;
    float scale;
};

static mouse_instance_t* get_mouse_instance(uni_hid_device_t* d) {
    return (mouse_instance_t*)&d->parser_data[0];
}

// TL;DR: Mouse reports are hit and miss.
// Don't try to be clever: just use the "delta" multiplied by some constant (defined per mouse).
// Long:
// "Unitless" reports have no sense. How many "deltas" should I move?
// "Unit"-oriented reports (like Apple Magic Mouse), are extremely complex to calculate.
// Should I really do a complicated math in order to translate how many inches the mouse move and
// then try to map that to the Amiga? Error-prone.
//
// Easy-and-hackish wat to solve:
// Since we only support a handful of mice, harcode the resolutions.
// This works only becuase we support a few mice. In the future we might need to do
// "smart" way to solve it... or just allow the user change the resolution via BLE application.
static const struct mouse_resolution resolutions[] = {
    // Apple Magic Mouse 1st gen
    {0x05ac, 0x030d, NULL, 0.2},
    // Apple Magic Mouse 2nd gen
    {0x05ac, 0x0269, NULL, 0.2},
    {0x004c, 0x0269, NULL, 0.2},  // VID 0x004c... did I get a fake one?
    // Apple Magic Trackpad 1st gen
    {0x05ac, 0x030e, NULL, 0.47},
    // TECKNET 2600DPI. Has a "DPI" button. The faster one is unusable. Make the faster usable.
    {0x0a5c, 0x4503, "BM30X mouse", 0.3334},
    // Adesso iMouse M300
    {0x0a5c, 0x4503, "Adesso Bluetooth 3.0 Mouse", 0.5},
    // Bornd C170B
    {0x0a5c, 0x0001, "BORND Bluetooth Mouse", 0.6667},
    // Logitech	M336 / M337 / M535
    {0x046d, 0xb016, "Bluetooth Mouse M336/M337/M535", 0.3334},
    // Logitech M-RCL124 MX Revolution
    {0x046d, 0xb007, "Logitech MX Revolution Mouse", 0.3334},
    // Kensigton SureTrack Dual Wireless Mouse (K75351WW)
    {0xae24, 0x8618, "BT3.0 Mouse", 0.2},
    // TECKNET "3 Modes, 2400 DPI"
    {0xae24, 0x8618, "BM20X-3.0", 0.2},
    // HP Z5000
    {0x03f0, 0x084c, "HP Bluetooth Mouse Z5000", 0.5},
    // LogiLink ID0078A
    {0x046d, 0xb016, "ID0078A", 0.3334},
    // Tank mouse
    {TANK_MOUSE_VID, TANK_MOUSE_PID, NULL, 0.75},

    // No need to add entries where scale is 1
};

static int32_t process_mouse_delta(uni_hid_device_t* d, int32_t value) {
    if (value == 0)
        return 0;

    mouse_instance_t* ins = get_mouse_instance(d);
    float ret = value;

    ret *= ins->scale;

    // Quadrature-driver expect values between -127 and 127.
    if (ret < -127)
        ret = -127;
    if (ret > 127)
        ret = 127;

    int ret_i = roundf(ret);
    // Don't return 0 when there is any kind of movement... feels "janky".
    if (ret_i == 0)
        ret_i = (value < 0) ? -1 : 1;
    return ret_i;
}

void uni_hid_parser_mouse_setup(uni_hid_device_t* d) {
    char buf[64];
    // At setup time, fetch the "scale" for the mouse.
    float scale = 1;
    mouse_instance_t* ins = get_mouse_instance(d);

    for (unsigned int i = 0; i < ARRAY_SIZE(resolutions); i++) {
        if (resolutions[i].vid == d->vendor_id && resolutions[i].pid == d->product_id &&
            (resolutions[i].name == NULL || strcmp(resolutions[i].name, d->name) == 0)) {
            scale = resolutions[i].scale;
            break;
        }
    }

    ins->scale = scale;

    logi("mouse: vid=0x%04x, pid=0x%04x, name='%s' uses scale:", d->vendor_id, d->product_id, d->name);
    // ets_printf() doesn't support "%f"
    sprintf(buf, "%f\n", ins->scale);
    logi(buf);

    uni_hid_device_set_ready_complete(d);
}

void uni_hid_parser_mouse_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    ARG_UNUSED(d);
    ARG_UNUSED(report);
    ARG_UNUSED(len);
}

void uni_hid_parser_mouse_init_report(uni_hid_device_t* d) {
    // Reset old state. Each report contains a full-state.
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));
    ctl->klass = UNI_CONTROLLER_CLASS_MOUSE;
}

void uni_hid_parser_mouse_parse_usage(uni_hid_device_t* d,
                                      hid_globals_t* globals,
                                      uint16_t usage_page,
                                      uint16_t usage,
                                      int32_t value) {
    ARG_UNUSED(globals);

    // TODO: should be a union of gamepad/mouse/keyboard
    uni_controller_t* ctl = &d->controller;
    switch (usage_page) {
        case HID_USAGE_PAGE_GENERIC_DESKTOP: {
            switch (usage) {
                case HID_USAGE_AXIS_X:
                    // Mouse delta X
                    // Negative: left, positive: right.
                    ctl->mouse.delta_x = process_mouse_delta(d, value);
                    // printf("min: %d, max: %d\n", globals->logical_minimum, globals->logical_maximum);
                    // printf("delta x old value: %d -> new value: %d\n", value, ctl->mouse.delta_x);
                    break;
                case HID_USAGE_AXIS_Y:
                    // Mouse delta Y
                    // Negative: up, positive: down.
                    ctl->mouse.delta_y = process_mouse_delta(d, value);
                    // printf("delta y old value: %d -> new value: %d\n", value, ctl->mouse.delta_y);
                    break;
                case HID_USAGE_WHEEL:
                    ctl->mouse.scroll_wheel = value;
                    break;
                default:
                    logi("Mouse: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }

        case HID_USAGE_PAGE_BUTTON: {
            switch (usage) {
                case 0x01:  // Left click
                    if (value)
                        ctl->mouse.buttons |= UNI_MOUSE_BUTTON_LEFT;
                    break;
                case 0x02:  // Right click
                    if (value)
                        ctl->mouse.buttons |= UNI_MOUSE_BUTTON_RIGHT;
                    break;
                case 0x03:  // Middle click
                    if (value)
                        ctl->mouse.buttons |= UNI_MOUSE_BUTTON_MIDDLE;
                    break;
                case 0x04:  // Back button
                    if (value)
                        ctl->mouse.buttons |= UNI_MOUSE_BUTTON_AUX_0;
                    break;
                case 0x05:  // Forward button
                    if (value)
                        ctl->mouse.buttons |= UNI_MOUSE_BUTTON_AUX_1;
                    break;
                case 0x06:  // Logitech M535 Tilt Wheel Left
                    if (value)
                        ctl->mouse.buttons |= UNI_MOUSE_BUTTON_AUX_2;
                    break;
                case 0x07:  // Logitech M535 Tilt Wheel Right
                    if (value)
                        ctl->mouse.buttons |= UNI_MOUSE_BUTTON_AUX_3;
                    break;
                case 0x08:  // Logitech M535 (???)
                    if (value)
                        ctl->mouse.buttons |= UNI_MOUSE_BUTTON_AUX_4;
                    break;
                case 0x09:  // Logitech M-RCL124
                case 0x0a:
                case 0x0b:
                case 0x0c:
                case 0x0d:
                case 0x0e:
                case 0x0f:
                case 0x10:
                    break;
                default:
                    logi("Mouse: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }

        case HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS: {
            switch (usage) {
                case HID_USAGE_BATTERY_STRENGTH:
                    ctl->battery = value;
                    break;
                default:
                    logi("Mouse: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }
        case HID_USAGE_PAGE_KEYBOARD_KEYPAD: {
            // Triggered by Logitech M535's "gesture button"
            // Ignore it.
            break;
        }

        case HID_USAGE_PAGE_CONSUMER: {
            switch (usage) {
                case 0x00:  // Unassigned, Logitech M-RCL124
                    break;
                case HID_USAGE_AC_SEARCH:  // Logitech M-RCL124
                    break;
                case HID_USAGE_AC_PAN:  // Logitech M535
                    break;
                default:
                    logi("Mouse: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
                    break;
            }
            break;
        }

        case 0xff00:  // Logitech M-RCL124
            // User defined
            break;

        case 0xff01:  // Tank Mouse
                      // User defined
                      // fall-through

        // unknown usage page
        default:
            logi("Mouse: Unsupported page: 0x%04x, usage: 0x%04x, value=0x%x\n", usage_page, usage, value);
            break;
    }
}

void uni_hid_parser_mouse_device_dump(struct uni_hid_device_s* d) {
    char buf[48];

    mouse_instance_t* ins = get_mouse_instance(d);
    // ets_printf() doesn't support "%f"
    sprintf(buf, "\tmouse: scale=%f\n", ins->scale);
    logi(buf);
}
