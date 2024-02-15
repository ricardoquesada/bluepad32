// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include <string.h>

#include <uni.h>

//
// Globals
//
static int g_enhanced_mode = 0;
static int g_delete_keys = 0;

// Posix "instance"
typedef struct posix_instance_s {
    uni_gamepad_seat_t gamepad_seat;  // which "seat" is being used
} posix_instance_t;

// Declarations
static void trigger_event_on_gamepad(uni_hid_device_t* d);
static posix_instance_t* get_posix_instance(uni_hid_device_t* d);
struct uni_platform* uni_platform_custom_create(void);

//
// Platform Overrides
//
static void posix_init(int argc, const char** argv) {
    logi("posix: init()\n");
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--enhanced") == 0 || strcmp(argv[i], "-e") == 0) {
            g_enhanced_mode = 1;
            logi("Enhanced mode enabled\n");
        }
        if (strcmp(argv[i], "--delete") == 0 || strcmp(argv[i], "-d") == 0) {
            g_delete_keys = 1;
            logi("Stored keys will be deleted\n");
        }
    }

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
    uni_gamepad_set_mappings_type(UNI_GAMEPAD_MAPPINGS_TYPE_XBOX);
    uni_bt_service_set_enabled(true);
}

static void posix_on_init_complete(void) {
    logi("posix: on_init_complete()\n");

    // Safe to call "unsafe" functions since they are called from BT thread
    if (g_delete_keys)
        uni_bt_del_keys_unsafe();
    else
        uni_bt_list_keys_unsafe();

    uni_property_list_all();

    // Start scanning
    uni_bt_enable_new_connections_unsafe(true);
}

static void posix_on_device_connected(uni_hid_device_t* d) {
    logi("posix: device connected: %p\n", d);
}

static void posix_on_device_disconnected(uni_hid_device_t* d) {
    logi("posix: device disconnected: %p\n", d);
}

static uni_error_t posix_on_device_ready(uni_hid_device_t* d) {
    logi("posix: device ready: %p\n", d);
    posix_instance_t* ins = get_posix_instance(d);
    ins->gamepad_seat = GAMEPAD_SEAT_A;

    trigger_event_on_gamepad(d);
    return UNI_ERROR_SUCCESS;
}

static void posix_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
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
        case UNI_CONTROLLER_CLASS_MOUSE:
            uni_hid_parser_mouse_device_dump(d);
            break;
        default:
            break;
    }
}

static const uni_property_t* posix_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

static void posix_on_oob_event(uni_platform_oob_event_t event, void* data) {
    switch (event) {
        case UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON: {
            uni_hid_device_t* d = data;

            if (d == NULL) {
                loge("ERROR: posix_on_oob_event: Invalid NULL device\n");
                return;
            }

            posix_instance_t* ins = get_posix_instance(d);
            ins->gamepad_seat = ins->gamepad_seat == GAMEPAD_SEAT_A ? GAMEPAD_SEAT_B : GAMEPAD_SEAT_A;

            trigger_event_on_gamepad(d);
            break;
        }

        case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:
            logi("posix_on_oob_event: Bluetooth enabled: %d\n", (bool)(data));
            break;

        default:
            logi("posix_on_oob_event: unsupported event: 0x%04x\n", event);
            break;
    }
}

//
// Helpers
//
static posix_instance_t* get_posix_instance(uni_hid_device_t* d) {
    return (posix_instance_t*)&d->platform_data[0];
}

static void trigger_event_on_gamepad(uni_hid_device_t* d) {
    posix_instance_t* ins = get_posix_instance(d);

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
struct uni_platform* get_my_platform(void) {
    static struct uni_platform plat = {
        .name = "Posix",
        .init = posix_init,
        .on_init_complete = posix_on_init_complete,
        .on_device_connected = posix_on_device_connected,
        .on_device_disconnected = posix_on_device_disconnected,
        .on_device_ready = posix_on_device_ready,
        .on_oob_event = posix_on_oob_event,
        .on_controller_data = posix_on_controller_data,
        .get_property = posix_get_property,
    };

    return &plat;
}
