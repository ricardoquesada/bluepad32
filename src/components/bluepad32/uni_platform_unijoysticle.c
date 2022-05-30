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

#include "uni_platform_unijoysticle.h"

#include <math.h>
#include <stdbool.h>

#include <argtable3/argtable3.h>
#include <driver/gpio.h>
#include <esp_console.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>

#include "sdkconfig.h"
#include "uni_bluetooth.h"
#include "uni_bt_defines.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_debug.h"
#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_joystick.h"
#include "uni_mouse_quadrature.h"

// --- Defines / Enums

// To be used with Unijoysticle devices that only connect to one port.
// For exmaple, the Amiga device made by https://arananet.net/
// These devices have only one port, so they only cannot use JOYSTICK_PORT_A,
// and have 3 buttons mapped.
// Enabled if 1
#define PLAT_UNIJOYSTICLE_SINGLE_PORT 0

// In some board models not all GPIOs are set. Macro to simplify code for that.
#define SAFE_SET_BIT(__value) (__value == -1) ? 0 : (1ULL << __value)

// CPU where the Quadrature task runs
#define QUADRATURE_MOUSE_TASK_CPU 1

// 20 milliseconds ~= 1 frame in PAL
// 16.6 milliseconds ~= 1 frame in NTSC
// From: https://eab.abime.net/showthread.php?t=99970
//  Quickgun Turbo Pro:   7 cps
//  Zipstick:            13 cps
//  Quickshot 128F:      29 cps
//  Competition Pro:     62 cps
// One "Click-per-second" means that there is one "click" +  "release" in one second
// So the frequency must be doubled.
#define AUTOFIRE_FREQ_QUICKGUN_MS (1000 / 7 / 2)          // ~71ms, ~4 frames
#define AUTOFIRE_FREQ_ZIPSTICK_MS (1000 / 13 / 2)         // ~38ms, ~2 frames
#define AUTOFIRE_FREQ_QUICKSHOT_MS (1000 / 29 / 2)        // ~17ms, ~1 frame
#define AUTOFIRE_FREQ_COMPETITION_PRO_MS (1000 / 62 / 2)  // ~8ms, ~1/2 frame

#define TASK_AUTOFIRE_PRIO (9)
#define TASK_PUSH_BUTTON_PRIO (8)

// Data coming from gamepad axis is different from mouse deltas.
// They need to be scaled down, otherwise the pointer moves too fast.
#define GAMEPAD_AXIS_TO_MOUSE_DELTA_RATIO (50)

// AtariST is not as fast as Amiga while processing quadrature events.
// Cap max events to the max that AtariST can process.
// This is true for the official AtariST mouse as well.
// This value was "calculated" using an AtariST 520.
// Might not be true for newer models, like the Falcon.
#define ATARIST_MOUSE_DELTA_MAX (28)

enum {
    MOUSE_EMULATION_AMIGA,
    MOUSE_EMULATION_ATARIST,
};

enum {
    PUSH_BUTTON_0,  // Toggle enhanced/mouse mode
    PUSH_BUTTON_1,  // Swap ports
    PUSH_BUTTON_MAX,
};

enum {
    LED_J1,  // Player #1 connected, Green
    LED_J2,  // Player #2 connected, Red
    LED_BT,  // Bluetooth enabled, Blue
    LED_MAX,
};

enum {
    // Push buttons
    EVENT_BUTTON_0 = PUSH_BUTTON_0,
    EVENT_BUTTON_1 = PUSH_BUTTON_1,

    // Autofire group
    EVENT_AUTOFIRE = 0,
};

typedef enum {
    // Unknown model
    BOARD_MODEL_UNK,

    // Unijosyticle 2: Through hole version
    BOARD_MODEL_UNIJOYSTICLE2,

    // Unijoysticle 2 plus: SMT version
    BOARD_MODEL_UNIJOYSTICLE2_PLUS,

    // Unijoysticle 2 A500 version
    BOARD_MODEL_UNIJOYSTICLE2_A500,

    // Unijosyticle 2 ST520 version
    BOARD_MODEL_UNIJOYSTICLE2_ST520,

    // Unijosyticle Single port, like Arananet's Unijoy2Amiga
    BOARD_MODEL_UNIJOYSTICLE2_SINGLE_PORT,
} board_model_t;

// Different emulation modes
typedef enum {
    EMULATION_MODE_SINGLE_JOY,  // Basic mode
    EMULATION_MODE_SINGLE_MOUSE,
    EMULATION_MODE_COMBO_JOY_JOY,  // Enhanced mode
    EMULATION_MODE_COMBO_JOY_MOUSE,
} emulation_mode_t;

// Console commands
enum {
    CMD_SWAP_PORTS,
    CMD_SET_GAMEPAD_MODE_NORMAL,    // Basic mode
    CMD_SET_GAMEPAD_MODE_ENHANCED,  // Enhanced mode
    CMD_SET_GAMEPAD_MODE_MOUSE,     // Mouse mode
    CMD_GET_GAMEPAD_MODE,

    CMD_ENHANCED_MODE_COUNT,
};

// --- Structs / Typedefs
typedef void (*button_cb_t)(int button_idx);

// This is the "state" of the push button, and changes in runtime.
// This is the "mutable" part of the button that is stored in RAM.
// The "fixed" part is stored in ROM.
struct push_button_state {
    bool enabled;
    int64_t last_time_pressed_us;  // in microseconds
};

// These are const values. Cannot be modified in runtime.
struct push_button {
    gpio_num_t gpio;
    button_cb_t callback;
};

enum {
    JOY_UP,       // Pin 1
    JOY_DOWN,     // Pin 2
    JOY_LEFT,     // Pin 3
    JOY_RIGHT,    // Pin 4
    JOY_FIRE,     // Pin 6
    JOY_BUTTON2,  // Pin 9, AKA Pot X (C64), Pot Y (Amiga)
    JOY_BUTTON3,  // Pin 5, AKA Pot Y (C64), Pot X (Amiga)

    JOY_MAX,
};

struct gpio_config {
    gpio_num_t port_a[JOY_MAX];
    gpio_num_t port_b[JOY_MAX];
    gpio_num_t leds[LED_MAX];
    struct push_button push_buttons[PUSH_BUTTON_MAX];

    // Autofire frequency. How fast is the autofire.
    int autofire_freq_ms;
};

// The platform "instance"
typedef struct unijoysticle_instance_s {
    emulation_mode_t emu_mode;             // type of controller to emulate
    uni_gamepad_seat_t gamepad_seat;       // which "seat" (port) is being used
    uni_gamepad_seat_t prev_gamepad_seat;  // which "seat" (port) was used before
                                           // switching emu mode
} unijoysticle_instance_t;
_Static_assert(sizeof(unijoysticle_instance_t) < HID_DEVICE_MAX_PLATFORM_DATA, "Unijoysticle intance too big");

// --- Function declaration

static board_model_t get_board_model();

static unijoysticle_instance_t* get_unijoysticle_instance(const uni_hid_device_t* d);
static void set_gamepad_seat(uni_hid_device_t* d, uni_gamepad_seat_t seat);
static void process_joystick(uni_hid_device_t* d, uni_gamepad_seat_t seat, const uni_joystick_t* joy);
static void process_mouse(uni_hid_device_t* d,
                          uni_gamepad_seat_t seat,
                          int32_t delta_x,
                          int32_t delta_y,
                          uint16_t buttons);
static void joy_update_port(const uni_joystick_t* joy, const gpio_num_t* gpios);
static int get_mouse_emulation();
static bool is_device_a_mouse(uni_hid_device_t* d);

// Interrupt handlers
static void handle_event_button(int button_idx);

// GPIO Interrupt handlers
static void IRAM_ATTR gpio_isr_handler_button(void* arg);

static void pushbutton_event_loop(void* arg);
static void auto_fire_loop(void* arg);

static esp_err_t safe_gpio_set_level(gpio_num_t gpio, int value);
static void enable_bluetooth(bool enabled);
static void maybe_enable_mouse_timers(void);

// Button callbacks, called from a task that is not BT main thread
static void toggle_combo_enhanced_gamepad_cb(int button_idx);
static void toggle_combo_mouse_gamepad_cb(int button_idx);
static void swap_ports_cb(int button_idx);

// Commands
static void swap_ports(void);
static void set_gamepad_mode(int mode);
static void get_gamepad_mode(void);

// --- Consts (ROM)

// Unijoysticle v2: Through-hole version
const struct gpio_config gpio_config_univ2 = {
    .port_a = {GPIO_NUM_26, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_14, GPIO_NUM_33, GPIO_NUM_16},
    .port_b = {GPIO_NUM_27, GPIO_NUM_25, GPIO_NUM_32, GPIO_NUM_17, GPIO_NUM_12, -1, -1},
    .leds = {GPIO_NUM_5, GPIO_NUM_13, -1},
    .push_buttons = {{
                         .gpio = GPIO_NUM_10,
                         .callback = toggle_combo_enhanced_gamepad_cb,
                     },
                     {
                         .gpio = -1,
                         .callback = NULL,
                     }},
    .autofire_freq_ms = AUTOFIRE_FREQ_QUICKGUN_MS,
};

// Unijoysticle v2+: SMD version
const struct gpio_config gpio_config_univ2plus = {
    .port_a = {GPIO_NUM_26, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_14, GPIO_NUM_33, GPIO_NUM_16},
    .port_b = {GPIO_NUM_27, GPIO_NUM_25, GPIO_NUM_32, GPIO_NUM_17, GPIO_NUM_13, GPIO_NUM_21, GPIO_NUM_22},
    .leds = {GPIO_NUM_5, GPIO_NUM_12, -1},
    .push_buttons = {{
                         .gpio = GPIO_NUM_15,
                         .callback = toggle_combo_enhanced_gamepad_cb,
                     },
                     {
                         .gpio = -1,
                         .callback = NULL,
                     }},
    .autofire_freq_ms = AUTOFIRE_FREQ_QUICKGUN_MS,
};

// Unijoysticle v2 A500
const struct gpio_config gpio_config_univ2a500 = {
    .port_a = {GPIO_NUM_26, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_14, GPIO_NUM_33, GPIO_NUM_16},
    .port_b = {GPIO_NUM_27, GPIO_NUM_25, GPIO_NUM_32, GPIO_NUM_17, GPIO_NUM_13, GPIO_NUM_21, GPIO_NUM_22},
    .leds = {GPIO_NUM_5, GPIO_NUM_12, GPIO_NUM_15},
    .push_buttons = {{
                         .gpio = GPIO_NUM_34,
                         .callback = toggle_combo_mouse_gamepad_cb,
                     },
                     {
                         .gpio = GPIO_NUM_35,
                         .callback = swap_ports_cb,
                     }},
    .autofire_freq_ms = AUTOFIRE_FREQ_QUICKGUN_MS,
};

// Arananet's Unijoy2Amiga
const struct gpio_config gpio_config_univ2singleport = {
    // Only has one port. Just mirror Port A with Port B.
    .port_a = {GPIO_NUM_26, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_14, GPIO_NUM_33, GPIO_NUM_16},
    .port_b = {GPIO_NUM_26, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_14, GPIO_NUM_33, GPIO_NUM_16},

    // Not sure whether the LEDs and Push button are correct.
    .leds = {GPIO_NUM_12, -1, -1},
    .push_buttons = {{
                         .gpio = GPIO_NUM_15,
                         .callback = toggle_combo_enhanced_gamepad_cb,
                     },
                     {
                         .gpio = -1,
                         .callback = NULL,
                     }},
    .autofire_freq_ms = AUTOFIRE_FREQ_QUICKGUN_MS,
};

static const bd_addr_t zero_addr = {0, 0, 0, 0, 0, 0};

// --- Globals (RAM)

const struct gpio_config* g_gpio_config = NULL;

static EventGroupHandle_t g_event_group;
static EventGroupHandle_t g_auto_fire_group;

struct push_button_state g_push_buttons_state[PUSH_BUTTON_MAX] = {0};

// Autofire
static bool g_autofire_a_enabled = 0;
static bool g_autofire_b_enabled = 0;

// For the console
static struct {
    struct arg_str* value;
    struct arg_end* end;
} set_gamepad_mode_args;

static btstack_context_callback_registration_t cmd_callback_registration;

//
// Platform Overrides
//
static void unijoysticle_init(int argc, const char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    board_model_t model = get_board_model();

    switch (model) {
        case BOARD_MODEL_UNIJOYSTICLE2:
            logi("Hardware detected: Unijoysticle 2\n");
            g_gpio_config = &gpio_config_univ2;
            break;
        case BOARD_MODEL_UNIJOYSTICLE2_PLUS:
            logi("Hardware detected: Unijoysticle 2+\n");
            g_gpio_config = &gpio_config_univ2plus;
            break;
        case BOARD_MODEL_UNIJOYSTICLE2_A500:
            logi("Hardware detected: Unijoysticle 2 A500\n");
            g_gpio_config = &gpio_config_univ2a500;
            break;
        case BOARD_MODEL_UNIJOYSTICLE2_ST520:
            logi("Hardware detected: Unijoysticle 2 ST520\n");
            // Shares the same GPIO pins as the A500 one
            g_gpio_config = &gpio_config_univ2a500;
            break;
        case BOARD_MODEL_UNIJOYSTICLE2_SINGLE_PORT:
            logi("Hardware detected: Unijoysticle 2 single port\n");
            g_gpio_config = &gpio_config_univ2singleport;
            break;
        default:
            logi("Hardware detected: ERROR! %d\n", model);
            g_gpio_config = &gpio_config_univ2;
            break;
    }

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pin_bit_mask = 0;
    // Setup pins for Port A & B
    for (int i = 0; i < JOY_MAX; i++) {
        io_conf.pin_bit_mask |= SAFE_SET_BIT(g_gpio_config->port_a[i]);
        io_conf.pin_bit_mask |= SAFE_SET_BIT(g_gpio_config->port_b[i]);
    }

    // Setup pins for LEDs
    for (int i = 0; i < LED_MAX; i++)
        io_conf.pin_bit_mask |= SAFE_SET_BIT(g_gpio_config->leds[i]);

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Set low all joystick GPIOs... just in case.
    for (int i = 0; i < JOY_MAX; i++) {
        ESP_ERROR_CHECK(safe_gpio_set_level(g_gpio_config->port_a[i], 0));
        ESP_ERROR_CHECK(safe_gpio_set_level(g_gpio_config->port_b[i], 0));
    }

    // Turn On Player LEDs
    safe_gpio_set_level(g_gpio_config->leds[LED_J1], 1);
    safe_gpio_set_level(g_gpio_config->leds[LED_J2], 1);
    // Turn off Bluetooth LED
    safe_gpio_set_level(g_gpio_config->leds[LED_BT], 0);

    // Push Buttons
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    for (int i = 0; i < PUSH_BUTTON_MAX; i++) {
        if (g_gpio_config->push_buttons[i].gpio == -1)
            continue;

        io_conf.intr_type = GPIO_INTR_ANYEDGE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        // GPIOs 34~39 don't have internal Pull-up resistors.
        io_conf.pull_up_en =
            (g_gpio_config->push_buttons[i].gpio < GPIO_NUM_34) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
        io_conf.pin_bit_mask = BIT(g_gpio_config->push_buttons[i].gpio);
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        // "i" must match EVENT_BUTTON_0, value, etc.
        ESP_ERROR_CHECK(gpio_isr_handler_add(g_gpio_config->push_buttons[i].gpio, gpio_isr_handler_button, (void*)i));
    }

    // Split "events" from "auto_fire", since auto-fire is an on-going event.
    g_event_group = xEventGroupCreate();
    xTaskCreate(pushbutton_event_loop, "bp.uni.button", 2048, NULL, TASK_PUSH_BUTTON_PRIO, NULL);

    g_auto_fire_group = xEventGroupCreate();
    xTaskCreate(auto_fire_loop, "bp.uni.autofire", 2048, NULL, TASK_AUTOFIRE_PRIO, NULL);
    // xTaskCreatePinnedToCore(event_loop, "event_loop", 2048, NULL, portPRIVILEGE_BIT, NULL, 1);
}

static void unijoysticle_on_init_complete(void) {
    // Turn off LEDs
    safe_gpio_set_level(g_gpio_config->leds[LED_J1], 0);
    safe_gpio_set_level(g_gpio_config->leds[LED_J2], 0);

    // Values taken from:
    // * http://wiki.icomp.de/wiki/DE-9_Mouse
    // * https://www.waitingforfriday.com/?p=827#Commodore_Amiga
    // But they contradict on the Amiga pinout. Using "waitingforfriday" pinout.
    int x1, x2, y1, y2;
    switch (get_mouse_emulation()) {
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
            break;
    }

    // FIXME: These values are hardcoded for Amiga
    struct uni_mouse_quadrature_encoder_gpios port_a_x = {
        .a = g_gpio_config->port_a[x1],  // H-pulse (up)
        .b = g_gpio_config->port_a[x2],  // HQ-pulse (left)
    };

    struct uni_mouse_quadrature_encoder_gpios port_a_y = {
        .a = g_gpio_config->port_a[y1],  // V-pulse (down)
        .b = g_gpio_config->port_a[y2]   // VQ-pulse (right)
    };

    // Mouse AtariST is known to only work only one port A, but for the sake
    // of completness, both ports are configured on AtariST. Overkill ?
    struct uni_mouse_quadrature_encoder_gpios port_b_x = {
        .a = g_gpio_config->port_b[x1],  // H-pulse (up)
        .b = g_gpio_config->port_b[x2],  // HQ-pulse (left)
    };
    struct uni_mouse_quadrature_encoder_gpios port_b_y = {
        .a = g_gpio_config->port_b[y1],  // V-pulse (down)
        .b = g_gpio_config->port_b[y2]   // VQ-pulse (right)
    };
    uni_mouse_quadrature_init(QUADRATURE_MOUSE_TASK_CPU);
    uni_mouse_quadrature_setup_port(UNI_MOUSE_QUADRATURE_PORT_0, port_a_x, port_a_y);
    uni_mouse_quadrature_setup_port(UNI_MOUSE_QUADRATURE_PORT_1, port_b_x, port_b_y);

    enable_bluetooth(true);
}

static void unijoysticle_on_device_connected(uni_hid_device_t* d) {
    int connected = 0;
    if (d == NULL) {
        loge("ERROR: unijoysticle_on_device_connected: Invalid NULL device\n");
    }

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(i);
        if (uni_bt_conn_is_connected(&tmp_d->conn))
            connected++;
    }

    if (connected == 2) {
        enable_bluetooth(false);
    }
}

static void unijoysticle_on_device_disconnected(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("ERROR: unijoysticle_on_device_disconnected: Invalid NULL device\n");
        return;
    }
    unijoysticle_instance_t* ins = get_unijoysticle_instance(d);

    if (ins->gamepad_seat != GAMEPAD_SEAT_NONE) {
        // Turn off the LEDs
        if (ins->gamepad_seat == GAMEPAD_SEAT_A || ins->emu_mode == EMULATION_MODE_COMBO_JOY_JOY)
            safe_gpio_set_level(g_gpio_config->leds[LED_J1], 0);
        if (ins->gamepad_seat == GAMEPAD_SEAT_B || ins->emu_mode == EMULATION_MODE_COMBO_JOY_JOY)
            safe_gpio_set_level(g_gpio_config->leds[LED_J2], 0);

        ins->gamepad_seat = GAMEPAD_SEAT_NONE;
        ins->emu_mode = EMULATION_MODE_SINGLE_JOY;
    }

    // Regarless of how many connections are active, enable Bluetooth connections.
    enable_bluetooth(true);

    maybe_enable_mouse_timers();
}

static int unijoysticle_on_device_ready(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("ERROR: unijoysticle_on_device_ready: Invalid NULL device\n");
        return -1;
    }
    unijoysticle_instance_t* ins = get_unijoysticle_instance(d);

    // Some safety checks. These conditions should not happen
    if ((ins->gamepad_seat != GAMEPAD_SEAT_NONE) || (!uni_hid_device_has_controller_type(d))) {
        loge("ERROR: unijoysticle_on_device_ready: pre-condition not met\n");
        return -1;
    }

    uint32_t used_joystick_ports = 0;
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(i);
        used_joystick_ports |= get_unijoysticle_instance(tmp_d)->gamepad_seat;
    }

    // Either two gamepads are connected, or one is in Enhanced mode.
    // Don't allow new connections.
    if (used_joystick_ports == (GAMEPAD_SEAT_A | GAMEPAD_SEAT_B))
        return -1;

    int wanted_seat = GAMEPAD_SEAT_A;
    if (get_board_model() == BOARD_MODEL_UNIJOYSTICLE2_SINGLE_PORT) {
        // Single port boards only supports one port, so keep using SEAT A
        wanted_seat = GAMEPAD_SEAT_A;
        ins->emu_mode = EMULATION_MODE_SINGLE_JOY;

    } else {
        // Try with Port B, assume it is a joystick
        wanted_seat = GAMEPAD_SEAT_B;
        ins->emu_mode = EMULATION_MODE_SINGLE_JOY;

        // ... unless it is a mouse which should try with PORT A.
        // Amiga/Atari ST use mice in PORT A. Undefined on the C64, but
        // most apps use it in PORT A as well.
        if (is_device_a_mouse(d)) {
            wanted_seat = GAMEPAD_SEAT_A;
            ins->emu_mode = EMULATION_MODE_SINGLE_MOUSE;
        }

        // If wanted port is already assigned, try with the next one.
        if (used_joystick_ports & wanted_seat) {
            logi("unijoysticle: Port %d already assigned, trying another one\n", wanted_seat);
            wanted_seat = (~wanted_seat) & GAMEPAD_SEAT_AB_MASK;
        }
    }

    set_gamepad_seat(d, wanted_seat);

    maybe_enable_mouse_timers();

    return 0;
}

static void unijoysticle_on_gamepad_data(uni_hid_device_t* d, uni_gamepad_t* gp) {
    int32_t axis_x;
    int32_t axis_y;

    if (d == NULL) {
        loge("ERROR: unijoysticle_on_device_gamepad_data: Invalid NULL device\n");
        return;
    }

    unijoysticle_instance_t* ins = get_unijoysticle_instance(d);

    uni_joystick_t joy, joy_ext;
    memset(&joy, 0, sizeof(joy));
    memset(&joy_ext, 0, sizeof(joy_ext));

    switch (ins->emu_mode) {
        case EMULATION_MODE_SINGLE_JOY:
            uni_joy_to_single_joy_from_gamepad(gp, &joy);
            process_joystick(d, ins->gamepad_seat, &joy);
            break;
        case EMULATION_MODE_SINGLE_MOUSE:
            axis_x = gp->axis_x;
            axis_y = gp->axis_y;
            if (get_mouse_emulation() == MOUSE_EMULATION_ATARIST) {
                if (axis_x < -ATARIST_MOUSE_DELTA_MAX)
                    axis_x = -ATARIST_MOUSE_DELTA_MAX;
                if (axis_x > ATARIST_MOUSE_DELTA_MAX)
                    axis_x = ATARIST_MOUSE_DELTA_MAX;
                if (axis_y < -ATARIST_MOUSE_DELTA_MAX)
                    axis_y = -ATARIST_MOUSE_DELTA_MAX;
                if (axis_y > ATARIST_MOUSE_DELTA_MAX)
                    axis_y = ATARIST_MOUSE_DELTA_MAX;
            }
            process_mouse(d, ins->gamepad_seat, axis_x, axis_y, gp->buttons);
            break;
        case EMULATION_MODE_COMBO_JOY_JOY:
            uni_joy_to_combo_joy_joy_from_gamepad(gp, &joy, &joy_ext);
            process_joystick(d, GAMEPAD_SEAT_A, &joy);
            process_joystick(d, GAMEPAD_SEAT_B, &joy_ext);
            break;
        case EMULATION_MODE_COMBO_JOY_MOUSE:
            // Allow to control the mouse with both axis. Use case:
            // - Right axis: easier to control with the right thumb (for right handed people)
            // - Left axis: easier to drag a window (move + button pressed)
            // ... but only if the axis have the same sign
            // Do: (x/ratio) + (rx/ratio), instead of "(x+rx)/ratio". We want to lose precision.
            process_mouse(
                d, ins->gamepad_seat,
                (gp->axis_x / GAMEPAD_AXIS_TO_MOUSE_DELTA_RATIO) + (gp->axis_rx / GAMEPAD_AXIS_TO_MOUSE_DELTA_RATIO),
                (gp->axis_y / GAMEPAD_AXIS_TO_MOUSE_DELTA_RATIO) + (gp->axis_ry / GAMEPAD_AXIS_TO_MOUSE_DELTA_RATIO),
                gp->buttons);
            break;
        default:
            loge("unijoysticle: Unsupported emulation mode: %d\n", ins->emu_mode);
            break;
    }
}

static int32_t unijoysticle_get_property(uni_platform_property_t key) {
    if (key != UNI_PLATFORM_PROPERTY_DELETE_STORED_KEYS)
        return -1;

    if (g_gpio_config->push_buttons[PUSH_BUTTON_0].gpio == -1)
        return -1;

    // Hi-released, Low-pressed
    return !gpio_get_level(g_gpio_config->push_buttons[PUSH_BUTTON_0].gpio);
}

static void unijoysticle_on_device_oob_event(uni_hid_device_t* d, uni_platform_oob_event_t event) {
    if (d == NULL) {
        loge("ERROR: unijoysticle_on_device_gamepad_event: Invalid NULL device\n");
        return;
    }

    if (event != UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON) {
        loge("ERROR: unijoysticle_on_device_oob_event: unsupported event: 0x%04x\n", event);
        return;
    }

    if (get_board_model() == BOARD_MODEL_UNIJOYSTICLE2_SINGLE_PORT) {
        logi("INFO: unijoysticle_on_device_oob_event: No effect in single port boards\n");
        return;
    }

    unijoysticle_instance_t* ins = get_unijoysticle_instance(d);

    if (ins->gamepad_seat == GAMEPAD_SEAT_NONE) {
        logi("unijoysticle: cannot swap port since device has joystick_port = GAMEPAD_SEAT_NONE\n");
        return;
    }

    // This could happen if device is any Combo emu mode.
    if (ins->gamepad_seat == (GAMEPAD_SEAT_A | GAMEPAD_SEAT_B)) {
        logi(
            "unijoysticle: cannot swap port since has more than one port associated with. Leave emu mode and try "
            "again.\n");
        return;
    }

    // Swap joystick ports except if there is a connected gamepad that doesn't have the "System" button pressed.
    // Basically allow swapping mouse+gampead, or two gamepads while both are pressing the "system" button at the same
    // time.
    for (int j = 0; j < CONFIG_BLUEPAD32_MAX_DEVICES; j++) {
        uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(j);
        unijoysticle_instance_t* tmp_ins = get_unijoysticle_instance(tmp_d);
        if (uni_bt_conn_is_connected(&tmp_d->conn) && tmp_ins->gamepad_seat != GAMEPAD_SEAT_NONE &&
            tmp_ins->emu_mode == EMULATION_MODE_SINGLE_JOY &&
            ((tmp_d->gamepad.misc_buttons & MISC_BUTTON_SYSTEM) == 0)) {
            logi("unijoysticle: two swap ports press 'system' button on both gamepads at the same time\n");
            uni_hid_device_dump_all();
            return;
        }
    }

    swap_ports();
}

static void unijoysticle_device_dump(uni_hid_device_t* d) {
    unijoysticle_instance_t* ins = get_unijoysticle_instance(d);

    if (is_device_a_mouse(d)) {
        logi("\ttype=mouse, ");
    } else {
        logi("\ttype=gamepad, mode=");
        if (ins->emu_mode == EMULATION_MODE_COMBO_JOY_JOY)
            logi("enhanced, ");
        else if (ins->emu_mode == EMULATION_MODE_COMBO_JOY_MOUSE)
            logi("mouse, ");
        else if (ins->emu_mode == EMULATION_MODE_SINGLE_JOY)
            logi("normal, ");
        else
            logi("unk, ");
    }
    logi("seat: 0x%02x\n", ins->gamepad_seat);
}

//
// Helpers
//

static bool is_device_a_mouse(uni_hid_device_t* d) {
    uint32_t mouse_cod = UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE;
    return (d->cod & mouse_cod) == mouse_cod;
}

static int get_mouse_emulation() {
    int ret = MOUSE_EMULATION_AMIGA;
#if CONFIG_BLUEPAD32_UNIJOYSTICLE_MOUSE_AUTO
    ret = (get_board_model() == BOARD_MODEL_UNIJOYSTICLE2_ST520) ? MOUSE_EMULATION_ATARIST : MOUSE_EMULATION_AMIGA;
#elif defined(CONFIG_BLUEPAD32_UNIJOYSTICLE_MOUSE_AMIGA)
    ret = MOUSE_EMULATION_AMIGA;
#elif defined(CONFIG_BLUEPAD32_UNIJOYSTICLE_MOUSE_ATARIST)
    ret = MOUSE_EMULATION_ATARIST;
#endif  // CONFIG_BLUEPAD32_UNIJOYSTICLE_MOUSE
    return ret;
}
static board_model_t get_board_model() {
#if PLAT_UNIJOYSTICLE_SINGLE_PORT
    // Legacy: Only needed for Arananet's Unijoy2Amiga.
    // Single-port boards should ground GPIO 5. It will be detected in runtime.
    return BOARD_MODEL_UNIJOYSTICLE2_SINGLE_PORT;
#else
    // Cache value. Detection must only be done once.
    static board_model_t _model = BOARD_MODEL_UNK;
    if (_model != BOARD_MODEL_UNK)
        return _model;

    // Detect hardware version based on GPIOs 4, 5, 15
    //              GPIO 4   GPIO 5    GPIO 15
    // Uni 2:       Hi       Hi        Hi
    // Uni 2+:      Low      Hi        Hi
    // Uni 2 A500:  Hi       Hi        Lo
    // Uni 2 ST520: Low      Hi        Lo
    // Single port: Hi       Low       Hi

    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY);

    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_5, GPIO_PULLUP_ONLY);

    gpio_set_direction(GPIO_NUM_15, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY);

    int gpio_4 = gpio_get_level(GPIO_NUM_4);
    int gpio_5 = gpio_get_level(GPIO_NUM_5);
    int gpio_15 = gpio_get_level(GPIO_NUM_15);

    logi("Unijoysticle: Board ID values: %d,%d,%d\n", gpio_4, gpio_5, gpio_15);
    if (gpio_5 == 0)
        _model = BOARD_MODEL_UNIJOYSTICLE2_SINGLE_PORT;
    else if (gpio_4 == 1 && gpio_15 == 1)
        _model = BOARD_MODEL_UNIJOYSTICLE2;
    else if (gpio_4 == 0 && gpio_15 == 1)
        _model = BOARD_MODEL_UNIJOYSTICLE2_PLUS;
    else if (gpio_4 == 1 && gpio_15 == 0)
        _model = BOARD_MODEL_UNIJOYSTICLE2_A500;
    else if (gpio_4 == 0 && gpio_15 == 0)
        _model = BOARD_MODEL_UNIJOYSTICLE2_ST520;
    else {
        logi("Unijoysticle: Invalid Board ID value: %d,%d,%d\n", gpio_4, gpio_5, gpio_15);
        _model = BOARD_MODEL_UNIJOYSTICLE2;
    }

    // After detection, remove the pullup. The GPIOs might be used for something else after booting.
    gpio_set_pull_mode(GPIO_NUM_4, GPIO_FLOATING);
    gpio_set_pull_mode(GPIO_NUM_5, GPIO_FLOATING);
    gpio_set_pull_mode(GPIO_NUM_15, GPIO_FLOATING);
    return _model;
#endif  // !PLAT_UNIJOYSTICLE_SINGLE_PORT
}

static unijoysticle_instance_t* get_unijoysticle_instance(const uni_hid_device_t* d) {
    return (unijoysticle_instance_t*)&d->platform_data[0];
}

static void process_mouse(uni_hid_device_t* d,
                          uni_gamepad_seat_t seat,
                          int32_t delta_x,
                          int32_t delta_y,
                          uint16_t buttons) {
    ARG_UNUSED(d);
    static uint16_t prev_buttons = 0;
    logd("unijoysticle: seat: %d, mouse: x=%d, y=%d, buttons=0x%04x\n", seat, delta_x, delta_y, buttons);

    int port_idx = (seat == GAMEPAD_SEAT_A) ? UNI_MOUSE_QUADRATURE_PORT_0 : UNI_MOUSE_QUADRATURE_PORT_1;

    uni_mouse_quadrature_update(port_idx, delta_x, delta_y);

    if (buttons != prev_buttons) {
        prev_buttons = buttons;
        int fire, button2, button3;
        if (seat == GAMEPAD_SEAT_A) {
            fire = g_gpio_config->port_a[JOY_FIRE];
            button2 = g_gpio_config->port_a[JOY_BUTTON2];
            button3 = g_gpio_config->port_a[JOY_BUTTON3];
        } else {
            fire = g_gpio_config->port_b[JOY_FIRE];
            button2 = g_gpio_config->port_b[JOY_BUTTON2];
            button3 = g_gpio_config->port_b[JOY_BUTTON3];
        }
        safe_gpio_set_level(fire, !!(buttons & BUTTON_A));
        safe_gpio_set_level(button2, !!(buttons & BUTTON_B));
        safe_gpio_set_level(button3, !!(buttons & BUTTON_X));
    }
}

static void process_joystick(uni_hid_device_t* d, uni_gamepad_seat_t seat, const uni_joystick_t* joy) {
    ARG_UNUSED(d);
    if (seat == GAMEPAD_SEAT_A) {
        joy_update_port(joy, g_gpio_config->port_a);
        g_autofire_a_enabled = joy->auto_fire;
    } else if (seat == GAMEPAD_SEAT_B) {
        joy_update_port(joy, g_gpio_config->port_b);
        g_autofire_b_enabled = joy->auto_fire;
    } else {
        loge("unijoysticle: process_joystick: invalid gamepad seat: %d\n", seat);
    }

    if (g_autofire_a_enabled || g_autofire_b_enabled) {
        xEventGroupSetBits(g_auto_fire_group, BIT(EVENT_AUTOFIRE));
    }
}

static void set_gamepad_seat(uni_hid_device_t* d, uni_gamepad_seat_t seat) {
    unijoysticle_instance_t* ins = get_unijoysticle_instance(d);
    ins->gamepad_seat = seat;

    logi("unijoysticle: device %s has new gamepad seat: %d\n", bd_addr_to_str(d->conn.btaddr), seat);

    // Fetch all enabled ports
    uni_gamepad_seat_t all_seats = GAMEPAD_SEAT_NONE;
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(i);
        if (tmp_d == NULL)
            continue;
        if (bd_addr_cmp(tmp_d->conn.btaddr, zero_addr) != 0) {
            all_seats |= get_unijoysticle_instance(tmp_d)->gamepad_seat;
        }
    }

    bool status_a = ((all_seats & GAMEPAD_SEAT_A) != 0);
    bool status_b = ((all_seats & GAMEPAD_SEAT_B) != 0);
    safe_gpio_set_level(g_gpio_config->leds[LED_J1], status_a);
    safe_gpio_set_level(g_gpio_config->leds[LED_J2], status_b);

    bool lightbar_or_led_set = false;
    if (d->report_parser.set_lightbar_color != NULL) {
        uint8_t red = 0;
        uint8_t green = 0;
        if (seat & 0x01)
            green = 0xff;
        if (seat & 0x02)
            red = 0xff;
        d->report_parser.set_lightbar_color(d, red, green, 0x00 /* blue*/);
        lightbar_or_led_set = true;
    }
    if (d->report_parser.set_player_leds != NULL) {
        d->report_parser.set_player_leds(d, seat);
        lightbar_or_led_set = true;
    }

    if (!lightbar_or_led_set && d->report_parser.set_rumble != NULL) {
        d->report_parser.set_rumble(d, 0x80 /* value */, 0x04 /* duration */);
    }
}

static void joy_update_port(const uni_joystick_t* joy, const gpio_num_t* gpios) {
    logd("up=%d, down=%d, left=%d, right=%d, fire=%d, bt2=%d, bt3=%d\n", joy->up, joy->down, joy->left, joy->right,
         joy->fire, joy->button2, joy->button3);

    safe_gpio_set_level(gpios[0], !!joy->up);
    safe_gpio_set_level(gpios[1], !!joy->down);
    safe_gpio_set_level(gpios[2], !!joy->left);
    safe_gpio_set_level(gpios[3], !!joy->right);

    // Only update fire if auto-fire is off. Otherwise it will conflict.
    if (!joy->auto_fire) {
        safe_gpio_set_level(gpios[4], !!joy->fire);
    }

    safe_gpio_set_level(gpios[5], !!joy->button2);
    safe_gpio_set_level(gpios[6], !!joy->button3);
}

static void pushbutton_event_loop(void* arg) {
    // timeout of 10s
    const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;
    while (1) {
        EventBits_t uxBits = xEventGroupWaitBits(g_event_group, BIT(EVENT_BUTTON_0) | BIT(EVENT_BUTTON_1), pdTRUE,
                                                 pdFALSE, xTicksToWait);

        // timeout ?
        if (uxBits == 0)
            continue;

        if (uxBits & BIT(EVENT_BUTTON_0))
            handle_event_button(EVENT_BUTTON_0);

        if (uxBits & BIT(EVENT_BUTTON_1))
            handle_event_button(EVENT_BUTTON_1);
    }
}

static void auto_fire_loop(void* arg) {
    // timeout of 10s
    const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;
    const TickType_t delayTicks = g_gpio_config->autofire_freq_ms / portTICK_PERIOD_MS;
    while (1) {
        EventBits_t uxBits = xEventGroupWaitBits(g_auto_fire_group, BIT(EVENT_AUTOFIRE), pdTRUE, pdFALSE, xTicksToWait);

        // timeout ?
        if (uxBits == 0)
            continue;

        while (g_autofire_a_enabled || g_autofire_b_enabled) {
            if (g_autofire_a_enabled)
                safe_gpio_set_level(g_gpio_config->port_a[JOY_FIRE], 1);
            if (g_autofire_b_enabled)
                safe_gpio_set_level(g_gpio_config->port_b[JOY_FIRE], 1);

            vTaskDelay(delayTicks);

            if (g_autofire_a_enabled)
                safe_gpio_set_level(g_gpio_config->port_a[JOY_FIRE], 0);
            if (g_autofire_b_enabled)
                safe_gpio_set_level(g_gpio_config->port_b[JOY_FIRE], 0);

            vTaskDelay(delayTicks);
        }
    }
}

static void IRAM_ATTR gpio_isr_handler_button(void* arg) {
    int button_idx = (int)arg;

    // Stored din ROM
    const struct push_button* pb = &g_gpio_config->push_buttons[button_idx];
    // Stored in RAM
    struct push_button_state* st = &g_push_buttons_state[button_idx];

    // Button released ?
    if (gpio_get_level(pb->gpio)) {
        st->last_time_pressed_us = esp_timer_get_time();
        return;
    }

    // Button pressed
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(g_event_group, BIT(button_idx), &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE)
        portYIELD_FROM_ISR();
}

static void handle_event_button(int button_idx) {
    // Basic software debouncer. If it fails, it could be improved with:
    // https://hackaday.com/2015/12/10/embed-with-elliot-debounce-your-noisy-buttons-part-ii/
    const int64_t button_threshold_time_us = 300 * 1000;  // 300ms

    // Stored in ROM
    const struct push_button* pb = &g_gpio_config->push_buttons[button_idx];
    // Stored in RAM
    struct push_button_state* st = &g_push_buttons_state[button_idx];

    // Regardless of the state, ignore the event if not enough time passed.
    int64_t now = esp_timer_get_time();
    if ((now - st->last_time_pressed_us) < button_threshold_time_us)
        return;

    st->last_time_pressed_us = now;

    // "up" button is released. Ignore event.
    if (gpio_get_level(pb->gpio)) {
        return;
    }

    // "down", button pressed.
    logi("handle_event_button(%d): %d -> %d\n", button_idx, st->enabled, !st->enabled);

    st->enabled = !st->enabled;
    pb->callback(button_idx);
}

static void cmd_callback(void* context) {
    int cmd = (int)context;
    switch (cmd) {
        case CMD_SWAP_PORTS:
            swap_ports();
            break;
        case CMD_SET_GAMEPAD_MODE_ENHANCED:
            set_gamepad_mode(EMULATION_MODE_COMBO_JOY_JOY);
            break;
        case CMD_SET_GAMEPAD_MODE_MOUSE:
            set_gamepad_mode(EMULATION_MODE_COMBO_JOY_MOUSE);
            break;
        case CMD_SET_GAMEPAD_MODE_NORMAL:
            set_gamepad_mode(EMULATION_MODE_SINGLE_JOY);
            break;
        case CMD_GET_GAMEPAD_MODE:
            get_gamepad_mode();
            break;
        default:
            loge("Unijoysticle: invalid command: %d\n", cmd);
            break;
    }
}

static void get_gamepad_mode() {
    // Change emulation mode
    int num_devices = 0;
    uni_hid_device_t* d = NULL;
    for (int j = 0; j < CONFIG_BLUEPAD32_MAX_DEVICES; j++) {
        uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(j);
        if (uni_bt_conn_is_connected(&tmp_d->conn)) {
            num_devices++;
            if (!is_device_a_mouse(tmp_d)) {
                d = tmp_d;
                break;
            }
        }
    }

    if (d == NULL) {
        loge("unijoysticle: Cannot find gamepad device\n");
        return;
    }

    unijoysticle_instance_t* ins = get_unijoysticle_instance(d);

    switch (ins->emu_mode) {
        case EMULATION_MODE_COMBO_JOY_JOY:
            logi("Gamepad mode = enhanced\n");
            break;

        case EMULATION_MODE_COMBO_JOY_MOUSE:
            logi("Gamepad mode = mouse\n");
            break;

        case EMULATION_MODE_SINGLE_JOY:
            logi("Gamepad mode = normal\n");
            break;

        default:
            logi("Gamepad mode = unknown\n");
            break;
    }
}

static void set_gamepad_mode(int mode) {
    // Change emulation mode
    int num_devices = 0;
    uni_hid_device_t* d = NULL;
    for (int j = 0; j < CONFIG_BLUEPAD32_MAX_DEVICES; j++) {
        uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(j);
        if (uni_bt_conn_is_connected(&tmp_d->conn)) {
            num_devices++;
            if (!is_device_a_mouse(tmp_d)) {
                d = tmp_d;
                break;
            }
        }
    }

    if (d == NULL) {
        loge("unijoysticle: Cannot find gamepad device\n");
        return;
    }

    unijoysticle_instance_t* ins = get_unijoysticle_instance(d);

    if (ins->emu_mode == mode)
        return;

    if (ins->emu_mode != EMULATION_MODE_COMBO_JOY_JOY && ins->emu_mode != EMULATION_MODE_COMBO_JOY_MOUSE &&
        ins->emu_mode != EMULATION_MODE_SINGLE_JOY)
        return;

    switch (mode) {
        case EMULATION_MODE_COMBO_JOY_JOY:
            if (num_devices != 1) {
                loge("unijoysticle: cannot change mode. Expected num_devices=1, actual=%d\n", num_devices);
                return;
            }

            ins->emu_mode = EMULATION_MODE_COMBO_JOY_JOY;
            ins->prev_gamepad_seat = ins->gamepad_seat;
            set_gamepad_seat(d, GAMEPAD_SEAT_A | GAMEPAD_SEAT_B);
            logi("unijoysticle: Gamepad mode = enhanced\n");

            enable_bluetooth(false);
            break;

        case EMULATION_MODE_COMBO_JOY_MOUSE:
            if (ins->emu_mode == EMULATION_MODE_COMBO_JOY_JOY) {
                set_gamepad_seat(d, ins->prev_gamepad_seat);
                enable_bluetooth(true);
            }
            ins->emu_mode = EMULATION_MODE_COMBO_JOY_MOUSE;
            logi("unijoysticle: Gamepad mode = mouse\n");
            break;

        case EMULATION_MODE_SINGLE_JOY:
            if (ins->emu_mode == EMULATION_MODE_COMBO_JOY_JOY) {
                set_gamepad_seat(d, ins->prev_gamepad_seat);
                enable_bluetooth(true);
            }
            ins->emu_mode = EMULATION_MODE_SINGLE_JOY;
            logi("unijoysticle: Gamepad mode = normal\n");
            break;

        default:
            loge("unijoysticle: unsupported gamepad mode: %d\n", mode);
            break;
    }
    maybe_enable_mouse_timers();
}

static void toggle_combo_enhanced_gamepad_cb(int button_idx) {
    ARG_UNUSED(button_idx);
    static bool enabled = false;

    enabled = !enabled;

    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)(enabled ? CMD_SET_GAMEPAD_MODE_ENHANCED : CMD_SET_GAMEPAD_MODE_NORMAL);
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

static int cmd_set_gamepad_mode(int argc, char** argv) {
    int nerrors = arg_parse(argc, argv, (void**)&set_gamepad_mode_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_gamepad_mode_args.end, argv[0]);
        return 1;
    }

    int mode = 0;

    if (strcmp(set_gamepad_mode_args.value->sval[0], "normal") == 0) {
        mode = CMD_SET_GAMEPAD_MODE_NORMAL;
    } else if (strcmp(set_gamepad_mode_args.value->sval[0], "enhanced") == 0) {
        mode = CMD_SET_GAMEPAD_MODE_ENHANCED;
    } else if (strcmp(set_gamepad_mode_args.value->sval[0], "mouse") == 0) {
        mode = CMD_SET_GAMEPAD_MODE_MOUSE;
    } else {
        loge("Invalid mouse mode: %s\n", set_gamepad_mode_args.value->sval[0]);
        return 1;
    }

    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)mode;
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
    return 0;
}

static void toggle_combo_mouse_gamepad_cb(int button_idx) {
    ARG_UNUSED(button_idx);
    static bool enabled = false;

    enabled = !enabled;

    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)(enabled ? CMD_SET_GAMEPAD_MODE_MOUSE : CMD_SET_GAMEPAD_MODE_NORMAL);
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

static int cmd_get_gamepad_mode(int argc, char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)CMD_GET_GAMEPAD_MODE;
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
    return 0;
}

static void swap_ports(void) {
    uni_hid_device_t* d;
    unijoysticle_instance_t* ins;
    uni_gamepad_seat_t prev_seat, new_seat;

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        d = uni_hid_device_get_instance_for_idx(i);
        if (uni_bt_conn_is_connected(&d->conn)) {
            ins = get_unijoysticle_instance(d);

            prev_seat = ins->gamepad_seat;
            new_seat = ~prev_seat & GAMEPAD_SEAT_AB_MASK;
            logi("unijoysticle: swap seat for %s: %d -> %d\n", bd_addr_to_str(d->conn.btaddr), prev_seat, new_seat);
            set_gamepad_seat(d, new_seat);
        }
    }

    // Clear joystick after switch to avoid having a line "On".
    uni_joystick_t joy;
    memset(&joy, 0, sizeof(joy));
    process_joystick(d, GAMEPAD_SEAT_A, &joy);
    process_joystick(d, GAMEPAD_SEAT_B, &joy);

    maybe_enable_mouse_timers();
}

static void swap_ports_cb(int button_idx) {
    ARG_UNUSED(button_idx);

    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)CMD_SWAP_PORTS;
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

static int cmd_swap_ports(int argc, char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)CMD_SWAP_PORTS;
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
    return 0;
}

// In some boards, not all GPIOs are set. If so, don't try change their values.
static esp_err_t safe_gpio_set_level(gpio_num_t gpio, int value) {
    if (gpio == -1)
        return ESP_OK;
    return gpio_set_level(gpio, value);
}

static void enable_bluetooth(bool enabled) {
    safe_gpio_set_level(g_gpio_config->leds[LED_BT], enabled);
    uni_bluetooth_enable_new_connections_safe(enabled);

    logi("unijoysticle: New gamepad connections %s\n", enabled ? "enabled" : "disabled");
}

static void maybe_enable_mouse_timers(void) {
    // Mouse support requires that the mouse timers are enabled.
    // Only enable them when needed
    bool enable_timer_0 = false;
    bool enable_timer_1 = false;

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        uni_hid_device_t* d = uni_hid_device_get_instance_for_idx(i);
        if (uni_bt_conn_is_connected(&d->conn)) {
            unijoysticle_instance_t* ins = get_unijoysticle_instance(d);

            // COMBO_JOY_MOUSE counts as real mouse.
            if (ins->emu_mode == EMULATION_MODE_COMBO_JOY_MOUSE || ins->emu_mode == EMULATION_MODE_SINGLE_MOUSE) {
                if (ins->gamepad_seat == GAMEPAD_SEAT_A)
                    enable_timer_0 = true;
                else if (ins->gamepad_seat == GAMEPAD_SEAT_B)
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

//
// Public
//
void uni_platform_unijoysticle_register_cmds(void) {
    set_gamepad_mode_args.value = arg_str1(NULL, NULL, "<mode>", "mode can be normal, enhanced or mouse");
    set_gamepad_mode_args.end = arg_end(2);

    const esp_console_cmd_t swap_ports = {
        .command = "swap_ports",
        .help = "Swaps joystick ports",
        .hint = NULL,
        .func = &cmd_swap_ports,
    };

    const esp_console_cmd_t set_gamepad_mode = {
        .command = "set_gamepad_mode",
        .help = "Sets the gamepad mode. At least one gamepad must be connected.",
        .hint = NULL,
        .func = &cmd_set_gamepad_mode,
        .argtable = &set_gamepad_mode_args,
    };

    const esp_console_cmd_t get_gamepad_mode = {
        .command = "get_gamepad_mode",
        .help = "Returns the gamepad mode. At least one gamepad must be connected.",
        .hint = NULL,
        .func = &cmd_get_gamepad_mode,
    };

#if CONFIG_BLUEPAD32_UNIJOYSTICLE_MOUSE_AUTO
    // const esp_console_cmd_t cmd_emulation_set = {
    //     .command = "mouse_emulation_set",
    //     .help = "Sets mouse emulation mode",
    //     .hint = NULL,
    //     .func = &cmd_mouse_emulation_set,
    //     .argtable = &mouse_emulation_set_cmd_args,
    // };

    // const esp_console_cmd_t cmd_emulation_get = {
    //     .command = "mouse_emulation_get",
    //     .help = "Returns mouse emulation mode",
    //     .hint = NULL,
    //     .func = &cmd_mouse_emulation_get,
    // };
#endif  // CONFIG_BLUEPAD32_UNIJOYSTICLE_MOUSE_AUTO

    ESP_ERROR_CHECK(esp_console_cmd_register(&swap_ports));
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_gamepad_mode));
    ESP_ERROR_CHECK(esp_console_cmd_register(&get_gamepad_mode));

#if CONFIG_BLUEPAD32_UNIJOYSTICLE_MOUSE_AUTO
    // ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_emulation_set));
    // ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_emulation_get));
#endif  // CONFIG_BLUEPAD32_UNIJOYSTICLE_MOUSE_AUTO
}

struct uni_platform* uni_platform_unijoysticle_create(void) {
    static struct uni_platform plat = {
        .name = "unijoysticle2",
        .init = unijoysticle_init,
        .on_init_complete = unijoysticle_on_init_complete,
        .on_device_connected = unijoysticle_on_device_connected,
        .on_device_disconnected = unijoysticle_on_device_disconnected,
        .on_device_ready = unijoysticle_on_device_ready,
        .on_device_oob_event = unijoysticle_on_device_oob_event,
        .on_gamepad_data = unijoysticle_on_gamepad_data,
        .get_property = unijoysticle_get_property,
        .device_dump = unijoysticle_device_dump,
    };

    return &plat;
}
