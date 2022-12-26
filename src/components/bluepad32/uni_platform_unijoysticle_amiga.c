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

// Unijoysticle platform

#include "uni_platform_unijoysticle_amiga.h"

#include <stdbool.h>

#include <argtable3/argtable3.h>
#include <esp_console.h>
#include <freertos/FreeRTOS.h>

#include "sdkconfig.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_log.h"
#include "uni_mouse_quadrature.h"
#include "uni_platform_unijoysticle.h"
#include "uni_property.h"

// --- Defines / Enums

// CPU where the Quadrature task runs
#define QUADRATURE_MOUSE_TASK_CPU 1

// AtariST is not as fast as Amiga while processing quadrature events.
// Cap max events to the max that AtariST can process.
// This is true for the official AtariST mouse as well.
// This value was "calculated" using an AtariST 520.
// Might not be true for newer models, like the Falcon.
#define ATARIST_MOUSE_DELTA_MAX (28)

enum {
    MOUSE_EMULATION_FROM_BOARD_MODEL,  // Used internally for NVS (Deprecated)
    MOUSE_EMULATION_AMIGA,
    MOUSE_EMULATION_ATARIST,

    MOUSE_EMULATION_COUNT,
};

// --- Function declaration

static int get_mouse_emulation_from_nvs(void);

// Commands or Event related
static int cmd_set_mouse_emulation(int argc, char** argv);
static int cmd_get_mouse_emulation(int argc, char** argv);

// --- Consts (ROM)

// Keep them in the order of the defines
static const char* mouse_modes[] = {
    "unknown",  // MOUSE_EMULATION_FROM_BOARD_MODEL
    "amiga",    // MOUSE_EMULATION_AMIGA
    "atarist",  // MOUSE_EMULATION_ATARIST
};

// --- Globals (RAM)

static struct {
    struct arg_str* value;
    struct arg_end* end;
} set_mouse_emulation_args;

//
// Platform Overrides
//

//
// Helpers
//

static void set_mouse_emulation_to_nvs(int mode) {
    uni_property_value_t value;
    value.u32 = mode;

    uni_property_set(UNI_PROPERTY_KEY_UNI_MOUSE_EMULATION, UNI_PROPERTY_TYPE_U32, value);
    logi("Done. Restart required. Type 'restart' + Enter\n");
}

static int get_mouse_emulation_from_nvs() {
    uni_property_value_t value;
    uni_property_value_t def;

    def.u32 = MOUSE_EMULATION_AMIGA;

    value = uni_property_get(UNI_PROPERTY_KEY_UNI_MOUSE_EMULATION, UNI_PROPERTY_TYPE_U32, def);

    // Validate return value.
    if (value.u8 >= MOUSE_EMULATION_COUNT || value.u8 == MOUSE_EMULATION_FROM_BOARD_MODEL)
        return MOUSE_EMULATION_AMIGA;
    return value.u8;
}

static int cmd_set_mouse_emulation(int argc, char** argv) {
    int nerrors = arg_parse(argc, argv, (void**)&set_mouse_emulation_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_mouse_emulation_args.end, argv[0]);
        return 1;
    }

    if (strcmp(set_mouse_emulation_args.value->sval[0], "amiga") == 0) {
        set_mouse_emulation_to_nvs(MOUSE_EMULATION_AMIGA);
    } else if (strcmp(set_mouse_emulation_args.value->sval[0], "atarist") == 0) {
        set_mouse_emulation_to_nvs(MOUSE_EMULATION_ATARIST);
    } else {
        loge("Invalid mouse emulation: %s\n", set_mouse_emulation_args.value->sval[0]);
        loge("Valid values: 'amiga' or 'atarist'\n");
        return 1;
    }

    return 0;
}

static int cmd_get_mouse_emulation(int argc, char** argv) {
    int mode = get_mouse_emulation_from_nvs();

    if (mode >= MOUSE_EMULATION_COUNT) {
        logi("Invalid mouse emulation: %d\n", mode);
        return 1;
    }

    logi("%s\n", mouse_modes[mode]);
    return 0;
}

//
// Public
//
void uni_platform_unijoysticle_amiga_register_cmds(void) {
    set_mouse_emulation_args.value = arg_str1(NULL, NULL, "<emulation>", "valid options: 'amiga' or 'atarist'");
    set_mouse_emulation_args.end = arg_end(2);

    const esp_console_cmd_t set_mouse_emulation = {
        .command = "set_mouse_emulation",
        .help =
            "Sets mouse emulation mode.\n"
            "  Default: amiga",
        .hint = NULL,
        .func = &cmd_set_mouse_emulation,
        .argtable = &set_mouse_emulation_args,
    };

    const esp_console_cmd_t get_mouse_emulation = {
        .command = "get_mouse_emulation",
        .help = "Returns mouse emulation mode",
        .hint = NULL,
        .func = &cmd_get_mouse_emulation,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&set_mouse_emulation));
    ESP_ERROR_CHECK(esp_console_cmd_register(&get_mouse_emulation));
}

void uni_platform_unijoysticle_amiga_on_init_complete(void) {
    // Values taken from:
    // * http://wiki.icomp.de/wiki/DE-9_Mouse
    // * https://www.waitingforfriday.com/?p=827#Commodore_Amiga
    // But they contradict on the Amiga pinout. Using "waitingforfriday" pinout.
    int x1, x2, y1, y2;
    switch (get_mouse_emulation_from_nvs()) {
        case MOUSE_EMULATION_AMIGA:
            x1 = 1;
            x2 = 3;
            y1 = 2;
            y2 = 0;
            logi("Unijoysticle: Using Amiga mouse emulation\n");
            break;
        case MOUSE_EMULATION_ATARIST:
            x1 = 1;
            x2 = 0;
            y1 = 2;
            y2 = 3;
            logi("Unijoysticle: Using AtariST mouse emulation\n");
            break;
        default:
            loge("Unijoysticle: Invalid mouse emulation mode\n");
            return;
    }

    // FIXME: These values are hardcoded for Amiga
    struct uni_mouse_quadrature_encoder_gpios port_a_x = {
        .a = uni_platform_unijoysticle_get_gpio_port_a(x1),  // H-pulse (up)
        .b = uni_platform_unijoysticle_get_gpio_port_a(x2),  // HQ-pulse (left)
    };

    struct uni_mouse_quadrature_encoder_gpios port_a_y = {
        .a = uni_platform_unijoysticle_get_gpio_port_a(y1),  // V-pulse (down)
        .b = uni_platform_unijoysticle_get_gpio_port_a(y2),  // VQ-pulse (right)
    };

    // Mouse AtariST is known to only work only one port A, but for the sake
    // of completness, both ports are configured on AtariST. Overkill ?
    struct uni_mouse_quadrature_encoder_gpios port_b_x = {
        .a = uni_platform_unijoysticle_get_gpio_port_b(x1),  // H-pulse (up)
        .b = uni_platform_unijoysticle_get_gpio_port_b(x2),  // HQ-pulse (left)
    };
    struct uni_mouse_quadrature_encoder_gpios port_b_y = {
        .a = uni_platform_unijoysticle_get_gpio_port_b(y1),  // V-pulse (down)
        .b = uni_platform_unijoysticle_get_gpio_port_b(y2),  // VQ-pulse (right)
    };

    uni_mouse_quadrature_init(QUADRATURE_MOUSE_TASK_CPU);
    uni_mouse_quadrature_setup_port(UNI_MOUSE_QUADRATURE_PORT_0, port_a_x, port_a_y);
    uni_mouse_quadrature_setup_port(UNI_MOUSE_QUADRATURE_PORT_1, port_b_x, port_b_y);
}

void uni_platform_unijoysticle_amiga_maybe_enable_mouse_timers(void) {
    // Mouse support requires that the mouse timers are enabled.
    // Only enable them when needed
    bool enable_timer_0 = false;
    bool enable_timer_1 = false;

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        uni_hid_device_t* d = uni_hid_device_get_instance_for_idx(i);
        if (uni_bt_conn_is_connected(&d->conn)) {
            uni_platform_unijoysticle_instance_t* ins = uni_platform_unijoysticle_get_instance(d);

            // COMBO_JOY_MOUSE counts as real mouse.
            if ((uni_hid_device_is_gamepad(d) &&
                 ins->gamepad_mode == UNI_PLATFORM_UNIJOYSTICLE_EMULATION_MODE_COMBO_JOY_MOUSE) ||
                uni_hid_device_is_mouse(d)) {
                if (ins->seat == GAMEPAD_SEAT_A)
                    enable_timer_0 = true;
                else if (ins->seat == GAMEPAD_SEAT_B)
                    enable_timer_1 = true;
            }
        }
    }

    logi("mice timers enabled/disabled: port A=%d, port B=%d\n", enable_timer_0, enable_timer_1);

    if (enable_timer_0)
        uni_mouse_quadrature_start(UNI_MOUSE_QUADRATURE_PORT_0);
    else
        uni_mouse_quadrature_pause(UNI_MOUSE_QUADRATURE_PORT_0);

    if (enable_timer_1)
        uni_mouse_quadrature_start(UNI_MOUSE_QUADRATURE_PORT_1);
    else
        uni_mouse_quadrature_pause(UNI_MOUSE_QUADRATURE_PORT_1);
}

void uni_platform_unijoysticle_amiga_process_mouse(uni_hid_device_t* d,
                                                   uni_gamepad_seat_t seat,
                                                   int32_t delta_x,
                                                   int32_t delta_y,
                                                   uint16_t buttons) {
    ARG_UNUSED(d);
    static uint16_t prev_buttons = 0;

    /* TODO: Cache this value. Might delay processing the mouse events */
    if (get_mouse_emulation_from_nvs() == MOUSE_EMULATION_ATARIST) {
        if (delta_x < -ATARIST_MOUSE_DELTA_MAX)
            delta_x = -ATARIST_MOUSE_DELTA_MAX;
        if (delta_x > ATARIST_MOUSE_DELTA_MAX)
            delta_x = ATARIST_MOUSE_DELTA_MAX;
        if (delta_y < -ATARIST_MOUSE_DELTA_MAX)
            delta_y = -ATARIST_MOUSE_DELTA_MAX;
        if (delta_y > ATARIST_MOUSE_DELTA_MAX)
            delta_y = ATARIST_MOUSE_DELTA_MAX;
    }

    logd("unijoysticle: seat: %d, mouse: x=%d, y=%d, buttons=0x%04x\n", seat, delta_x, delta_y, buttons);

    int port_idx = (seat == GAMEPAD_SEAT_A) ? UNI_MOUSE_QUADRATURE_PORT_0 : UNI_MOUSE_QUADRATURE_PORT_1;

    uni_mouse_quadrature_update(port_idx, delta_x, delta_y);

    if (buttons != prev_buttons) {
        prev_buttons = buttons;
        int fire, button2, button3;
        if (seat == GAMEPAD_SEAT_A) {
            fire = uni_platform_unijoysticle_get_gpio_port_a(UNI_PLATFORM_UNIJOYSTICLE_JOY_FIRE);
            button2 = uni_platform_unijoysticle_get_gpio_port_a(UNI_PLATFORM_UNIJOYSTICLE_JOY_BUTTON2);
            button3 = uni_platform_unijoysticle_get_gpio_port_a(UNI_PLATFORM_UNIJOYSTICLE_JOY_BUTTON3);
        } else {
            fire = uni_platform_unijoysticle_get_gpio_port_b(UNI_PLATFORM_UNIJOYSTICLE_JOY_FIRE);
            button2 = uni_platform_unijoysticle_get_gpio_port_b(UNI_PLATFORM_UNIJOYSTICLE_JOY_BUTTON2);
            button3 = uni_platform_unijoysticle_get_gpio_port_b(UNI_PLATFORM_UNIJOYSTICLE_JOY_BUTTON3);
        }
        if (fire != -1)
            gpio_set_level(fire, !!(buttons & BUTTON_A));
        if (button2 != -1)
            gpio_set_level(button2, !!(buttons & BUTTON_B));
        if (button3 != -1)
            gpio_set_level(button3, !!(buttons & BUTTON_X));
    }
}

void uni_platform_unijoysticle_amiga_version(void) {
    logi("\tMouse Emulation: %s\n", mouse_modes[get_mouse_emulation_from_nvs()]);
}
