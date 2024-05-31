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

enum {
    TRIGGER_EFFECT_VIBRATION,
    TRIGGER_EFFECT_WEAPON,
    TRIGGER_EFFECT_FEEDBACK,
    TRIGGER_EFFECT_OFF,

    TRIGGER_EFFECT_COUNT,
};

// Posix "instance"
typedef struct posix_instance_s {
    uni_gamepad_seat_t gamepad_seat;  // which "seat" is being used
} posix_instance_t;

// Declarations
static void trigger_event_on_gamepad(uni_hid_device_t* d);
static posix_instance_t* get_posix_instance(uni_hid_device_t* d);
struct uni_platform* uni_platform_custom_create(void);

static ds5_adaptive_trigger_effect_t next_trigger_adaptive_effect(int* trigger_effect_index) {
    ds5_adaptive_trigger_effect_t ret;

    switch (*trigger_effect_index) {
        case TRIGGER_EFFECT_VIBRATION:
            ret = ds5_new_adaptive_trigger_effect_vibration(3, 8, 15);
            break;
        case TRIGGER_EFFECT_WEAPON:
            ret = ds5_new_adaptive_trigger_effect_weapon(5, 7, 6);
            break;
        case TRIGGER_EFFECT_FEEDBACK:
            ret = ds5_new_adaptive_trigger_effect_feedback(2, 8);
            break;
        case TRIGGER_EFFECT_OFF:
        default:
            ret = ds5_new_adaptive_trigger_effect_off();
            break;
    }

    (*trigger_effect_index)++;
    if ((*trigger_effect_index) > TRIGGER_EFFECT_COUNT)
        (*trigger_effect_index) = 0;

    return ret;
}

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
    //    uni_bt_service_set_enabled(true);
}

static void posix_on_init_complete(void) {
    logi("posix: on_init_complete()\n");

    // Safe to call "unsafe" functions since they are called from BT thread
    if (g_delete_keys)
        uni_bt_del_keys_unsafe();
    else
        uni_bt_list_keys_unsafe();

    uni_property_dump_all();

    // Start scanning
    uni_bt_enable_new_connections_unsafe(true);
}

static uni_error_t posix_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    // You can filter discovered devices here. Return any value different from UNI_ERROR_SUCCESS.
    // @param addr: the Bluetooth address
    // @param name: could be NULL, could be zero-length, or might contain the name.
    // @param cod: Class of Device. See "uni_bt_defines.h" for possible values.
    // @param rssi: Received Signal Strength Indicator (RSSI) measured in dBms. The higher (255) the better.

    // As an example, if you want to filter out keyboards, do:
    if (((cod & UNI_BT_COD_MINOR_MASK) & UNI_BT_COD_MINOR_KEYBOARD) == UNI_BT_COD_MINOR_KEYBOARD) {
        //        logi("Ignoring keyboard\n");
        //        return UNI_ERROR_IGNORE_DEVICE;
    }

    return UNI_ERROR_SUCCESS;
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
    static ds5_adaptive_trigger_effect_t trigger_effect;
    static int trigger_effect_index_left = 0, trigger_effect_index_right = 0;
    static bool trigger_left_in_progress = false, trigger_right_in_progress = false;
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

            // Testing special Xbox One features
            if (d->controller_type == CONTROLLER_TYPE_XBoxOneController) {
                if ((gp->buttons & BUTTON_TRIGGER_L) && !trigger_left_in_progress) {
                    xboxone_play_quad_rumble(d, 0 /* delayed start ms */, 500 /* duration ms */,
                                             0xff /* trigger left magnitude*/, 0 /* trigger right magnitude */,
                                             0 /* weak motor magnitude */, 0 /* right motor magnitude */);
                    trigger_left_in_progress = true;
                }
                if ((gp->buttons & BUTTON_TRIGGER_L) == 0) {
                    trigger_left_in_progress = false;
                }

                if ((gp->buttons & BUTTON_TRIGGER_R) && !trigger_right_in_progress) {
                    xboxone_play_quad_rumble(d, 0 /* delayed start ms */, 500 /* duration ms */,
                                             0 /* trigger left magnitude */, 0xff /* trigger right magnitude */,
                                             0 /* weak motor magnitude */, 0 /* right motor magnitude */);
                    trigger_right_in_progress = true;
                }
                if ((gp->buttons & BUTTON_TRIGGER_R) == 0) {
                    trigger_right_in_progress = false;
                }
            }

            // Testing special DualSense features
            if (d->controller_type == CONTROLLER_TYPE_PS5Controller) {
                // toggle between FFBs for left trigger
                if ((gp->buttons & BUTTON_TRIGGER_L) && !trigger_left_in_progress) {
                    trigger_effect = next_trigger_adaptive_effect(&trigger_effect_index_left);
                    ds5_set_adaptive_trigger_effect(d, UNI_ADAPTIVE_TRIGGER_TYPE_LEFT, &trigger_effect);
                    trigger_left_in_progress = true;
                }
                if ((gp->buttons & BUTTON_TRIGGER_L) == 0) {
                    trigger_left_in_progress = false;
                }

                // toggle between FFBs for right trigger
                if ((gp->buttons & BUTTON_TRIGGER_R) && !trigger_right_in_progress) {
                    trigger_effect = next_trigger_adaptive_effect(&trigger_effect_index_right);
                    ds5_set_adaptive_trigger_effect(d, UNI_ADAPTIVE_TRIGGER_TYPE_RIGHT, &trigger_effect);
                    trigger_right_in_progress = true;
                }
                if ((gp->buttons & BUTTON_TRIGGER_R) == 0) {
                    trigger_right_in_progress = false;
                }
            }

            // Axis ry: control rumble
            if ((gp->buttons & BUTTON_A) && d->report_parser.play_dual_rumble != NULL) {
                d->report_parser.play_dual_rumble(d, 0 /* delayed start ms */, 500 /* duration ms */,
                                                  255 /* weak motor magnitude */, 0 /* strong motor magnitude */);
            }

            if ((gp->buttons & BUTTON_B) && d->report_parser.play_dual_rumble != NULL) {
                d->report_parser.play_dual_rumble(d, 0 /* delayed start ms */, 500 /* duration ms */,
                                                  0 /* weak motor magnitude */, 255 /* strong motor magnitude */);
            }

            // Buttons: Control LEDs On/Off
            if ((gp->buttons & BUTTON_X) && d->report_parser.set_player_leds != NULL) {
                d->report_parser.set_player_leds(d, leds++ & 0x0f);
            }
            // Axis: control RGB color
            if ((gp->buttons & BUTTON_Y) && d->report_parser.set_lightbar_color != NULL) {
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
            // TODO: Do something
            break;
        case UNI_CONTROLLER_CLASS_KEYBOARD:
            // TODO: Do something
            if (d->controller.keyboard.modifiers & UNI_KEYBOARD_MODIFIER_LEFT_ALT)
                uni_hid_parser_keyboard_set_leds(d, 0xff);
            break;
        case UNI_CONTROLLER_CLASS_BALANCE_BOARD:
            // TODO: Do something
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

    if (d->report_parser.play_dual_rumble != NULL) {
        d->report_parser.play_dual_rumble(
            d, 0 /* delayed start ms */, 150 /* duration ms */, 0 /* weak_magnitude */, 255 /* strong_magnitude */
        );
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
        .on_device_discovered = posix_on_device_discovered,
        .on_device_connected = posix_on_device_connected,
        .on_device_disconnected = posix_on_device_disconnected,
        .on_device_ready = posix_on_device_ready,
        .on_oob_event = posix_on_oob_event,
        .on_controller_data = posix_on_controller_data,
        .get_property = posix_get_property,
    };

    return &plat;
}
