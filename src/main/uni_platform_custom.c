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

// This example platform can be used as a starting point to create a new custom
// vendor/user/project-specific platform

#include "uni_platform_custom.h"

#include <stdio.h>
#include <string.h>

#include "uni_bt.h"
#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_log.h"

//
// Globals
//
static int g_delete_keys = 0;

// PC Debug "instance"
typedef struct custom_instance_s {
    uni_gamepad_seat_t gamepad_seat;  // which "seat" is being used
} custom_instance_t;

// Declarations
static void trigger_event_on_gamepad(uni_hid_device_t* d);
static custom_instance_t* get_custom_instance(uni_hid_device_t* d);

//
// Platform Overrides
//
static void custom_init(int argc, const char** argv) {

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    logi("custom: init()\n");

#if 0
    uni_gamepad_mappings_t mappings = GAMEPAD_DEFAULT_MAPPINGS;

    // Inverted axis with inverted Y in RY.
    mappings.axis_x = UNI_GAMEPAD_MAPPINGS_AXIS_RX;
    mappings.axis_y = UNI_GAMEPAD_MAPPINGS_AXIS_RY;
    mappings.axis_ry_inverted = true;
    mappings.axis_rx = UNI_GAMEPAD_MAPPINGS_AXIS_X;
    mappings.axis_ry = UNI_GAMEPAD_MAPPINGS_AXIS_Y;

    // Invert A & B
    mappings.button_a = UNI_GAMEPAD_MAPPINGS_BUTTON_B;
    mappings.button_b = UNI_GAMEPAD_MAPPINGS_BUTTON_A;

    uni_gamepad_set_mappings(&mappings);
#endif
}

static void custom_on_init_complete(void) {
    logi("custom: on_init_complete()\n");
}

static void custom_on_device_connected(uni_hid_device_t* d) {
    logi("custom: device connected: %p\n", d);
}

static void custom_on_device_disconnected(uni_hid_device_t* d) {
    logi("custom: device disconnected: %p\n", d);
}

static uni_error_t custom_on_device_ready(uni_hid_device_t* d) {
    logi("custom: device ready: %p\n", d);
    custom_instance_t* ins = get_custom_instance(d);
    ins->gamepad_seat = GAMEPAD_SEAT_A;

    trigger_event_on_gamepad(d);
    return UNI_ERROR_SUCCESS;
}

static void custom_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    static uint8_t leds = 0;
    static uint8_t enabled = true;
    static uni_controller_t prev = {0};
    uni_gamepad_t* gp;

    if (memcmp(&prev, ctl, sizeof(*ctl)) == 0) {
        return;
    }
    prev = *ctl;
    // Print device Id before dumping gamepad.
    logi("(%p) ", d);
    uni_controller_dump(ctl);

    switch (ctl->klass) {
        case UNI_CONTROLLER_CLASS_GAMEPAD:
            gp = &ctl->gamepad;

            // Debugging
            // Axis ry: control rumble
            if ((gp->buttons & BUTTON_A) && d->report_parser.set_rumble != NULL) {
                d->report_parser.set_rumble(d, 128, 128);
            }
            // Buttons: Control LEDs On/Off
            if ((gp->buttons & BUTTON_B) && d->report_parser.set_player_leds != NULL) {
                d->report_parser.set_player_leds(d, leds++ & 0x0f);
            }
            // Axis: control RGB color
            if ((gp->buttons & BUTTON_X) && d->report_parser.set_lightbar_color != NULL) {
                uint8_t r = (gp->axis_x * 256) / 512;
                uint8_t g = (gp->axis_y * 256) / 512;
                uint8_t b = (gp->axis_rx * 256) / 512;
                d->report_parser.set_lightbar_color(d, r, g, b);
            }

            // Toggle Bluetooth connections
            if ((gp->buttons & BUTTON_SHOULDER_L) && enabled) {
                logi("*** Disabling Bluetooth connections\n");
                uni_bt_enable_new_connections_safe(false);
                enabled = false;
            }
            if ((gp->buttons & BUTTON_SHOULDER_R) && !enabled) {
                logi("*** Enabling Bluetooth connections\n");
                uni_bt_enable_new_connections_safe(true);
                enabled = true;
            }
            break;
        default:
            break;
    }
}

static int32_t custom_get_property(uni_platform_property_t key) {
    logi("custom: get_property(): %d\n", key);
    if (key != UNI_PLATFORM_PROPERTY_DELETE_STORED_KEYS)
        return -1;
    return g_delete_keys;
}

static void custom_on_oob_event(uni_platform_oob_event_t event, void* data) {
    logi("custom: on_device_oob_event(): %d\n", event);

    if (event != UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON) {
        logi("custom_on_device_gamepad_event: unsupported event: 0x%04x\n", event);
        return;
    }

    uni_hid_device_t* d = data;

    if (d == NULL) {
        loge("ERROR: custom_on_device_gamepad_event: Invalid NULL device\n");
        return;
    }

    custom_instance_t* ins = get_custom_instance(d);
    ins->gamepad_seat = ins->gamepad_seat == GAMEPAD_SEAT_A ? GAMEPAD_SEAT_B : GAMEPAD_SEAT_A;

    trigger_event_on_gamepad(d);
}

//
// Helpers
//
static custom_instance_t* get_custom_instance(uni_hid_device_t* d) {
    return (custom_instance_t*)&d->platform_data[0];
}

static void trigger_event_on_gamepad(uni_hid_device_t* d) {
    custom_instance_t* ins = get_custom_instance(d);

    if (d->report_parser.set_rumble != NULL) {
        d->report_parser.set_rumble(d, 0x80 /* value */, 15 /* duration */);
    }

    if (d->report_parser.set_player_leds != NULL) {
        d->report_parser.set_player_leds(d, ins->gamepad_seat);
    }

    if (d->report_parser.set_lightbar_color != NULL) {
        uint8_t red = (ins->gamepad_seat & 0x01) ? 0xff : 0;
        uint8_t green = (ins->gamepad_seat & 0x02) ? 0xff : 0;
        uint8_t blue = (ins->gamepad_seat & 0x04) ? 0xff : 0;
        d->report_parser.set_lightbar_color(d, red, green, blue);
    }
}

//
// Entry Point
//
struct uni_platform* uni_platform_custom_create(void) {
    static struct uni_platform plat = {
        .name = "custom",
        .init = custom_init,
        .on_init_complete = custom_on_init_complete,
        .on_device_connected = custom_on_device_connected,
        .on_device_disconnected = custom_on_device_disconnected,
        .on_device_ready = custom_on_device_ready,
        .on_oob_event = custom_on_oob_event,
        .on_controller_data = custom_on_controller_data,
        .get_property = custom_get_property,
    };

    return &plat;
}
