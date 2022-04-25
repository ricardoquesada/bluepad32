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

#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <math.h>
#include <stdbool.h>

#include "sdkconfig.h"
#include "uni_bluetooth.h"
#include "uni_bt_defines.h"
#include "uni_config.h"
#include "uni_debug.h"
#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_joystick.h"

// --- Defines / Enums

// To be used with Unijoysticle devices that only connect to one port.
// For exmaple, the Amiga device made by https://arananet.net/
// These devices have only one port, so they only cannot use JOYSTICK_PORT_A,
// and have 3 buttons mapped.
// Enabled if 1
#define PLAT_UNIJOYSTICLE_SINGLE_PORT 0

// Number of usable ports in the DB9 joystick
// Up, down, left, right, fire, pot x, pot y
#define DB9_TOTAL_USABLE_PORTS 7

// In some board models not all GPIOs are set. Macro to simplify code for that.
#define SAFE_SET_BIT(__value) (__value == -1) ? 0 : (1ULL << __value)

// Max of push buttons that can be in a board.
#define PUSH_BUTTONS_MAX 2

enum {
    // Event group: push buttons and mouse
    // FIXME: EVENT_BUTTON_0 must be 0, EVENT_BUTTON_1 must be 1, etc...
    // This is because how our ISR expects the arg value.
    EVENT_BUTTON_0 = 0,
    EVENT_BUTTON_1 = 1,
    EVENT_MOUSE = 2,

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

    // Unijoysticle 2 plus: A500 version
    BOARD_MODEL_UNIJOYSTICLE2_A500,

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

// --- Structs / Typedefs
typedef void (*button_cb_t)(int button_idx);
struct push_button {
    bool enabled;
    int64_t last_time_pressed_us;  // in microseconds
    int gpio;                      // assigned at runtime when unijoysticle is initialized
    button_cb_t callback;          // function to call when triggered
};

struct uni_gpio_config_joy {
    gpio_num_t up;
    gpio_num_t down;
    gpio_num_t left;
    gpio_num_t right;
    gpio_num_t fire;
    gpio_num_t pot_x;
    gpio_num_t pot_y;
};

struct uni_gpio_config {
    union {
        struct uni_gpio_config_joy port_a_named;
        gpio_num_t port_a[DB9_TOTAL_USABLE_PORTS];
    };
    union {
        struct uni_gpio_config_joy port_b_named;
        gpio_num_t port_b[DB9_TOTAL_USABLE_PORTS];
    };
    gpio_num_t led_j1;         // Green
    gpio_num_t led_j2;         // Red
    gpio_num_t led_bt;         // Blue (Bluetooth on + misc)
    gpio_num_t push_button_0;  // Enhanced mode / mouse mode
    gpio_num_t push_button_1;  // Swap

    // Callback for each button
    button_cb_t push_button_0_cb;
    button_cb_t push_button_1_cb;
};

// C64 "instance"
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
static void process_joystick(const uni_joystick_t* joy, uni_gamepad_seat_t seat);
static void process_mouse(uni_hid_device_t* d, int32_t delta_x, int32_t delta_y, uint16_t buttons);

// Interrupt handlers
static void handle_event_mouse();
static void handle_event_button(int button_idx);

// Mouse related
static void joy_update_port(const uni_joystick_t* joy, const gpio_num_t* gpios);
static void mouse_send_move(int pin_a, int pin_b, uint32_t delay);
static void mouse_move_x(int dir, uint32_t delay);
static void mouse_move_y(int dir, uint32_t delay);

static void delay_us(uint32_t delay);

// GPIO Interrupt handlers
static void IRAM_ATTR gpio_isr_handler_button(void* arg);

static void event_loop(void* arg);
static void auto_fire_loop(void* arg);

static esp_err_t safe_gpio_set_level(gpio_num_t gpio, int value);

// Button callbacks
static void toggle_enhanced_mode(int button_idx);
static void toggle_mouse_mode(int button_idx);
static void swap_ports(int button_idx);

// --- Consts

// 20 milliseconds ~= 1 frame in PAL
// 17 milliseconds ~= 1 frame in NTSC
static const int AUTOFIRE_FREQ_MS = 20 * 4;  // change every ~4 frames

static const int MOUSE_DELAY_BETWEEN_EVENT_US = 1200;  // microseconds

// Unijoysticle v2: Through-hole version
const struct uni_gpio_config uni_gpio_config_v2 = {
    .port_a = {GPIO_NUM_26, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_14, -1, -1},
    .port_b = {GPIO_NUM_27, GPIO_NUM_25, GPIO_NUM_32, GPIO_NUM_17, GPIO_NUM_12, -1, -1},
    .led_j1 = GPIO_NUM_5,
    .led_j2 = GPIO_NUM_13,
    .led_bt = -1,
    .push_button_0 = GPIO_NUM_10,
    .push_button_1 = -1,
    .push_button_0_cb = toggle_enhanced_mode,
    .push_button_1_cb = NULL,
};
_Static_assert(sizeof(uni_gpio_config_v2.port_a) == sizeof(uni_gpio_config_v2.port_a_named),
               "Check uni_gpio_config union size");

// Unijoysticle v2+: SMD version
const struct uni_gpio_config uni_gpio_config_v2plus = {
    .port_a = {GPIO_NUM_26, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_14, GPIO_NUM_33, GPIO_NUM_16},
    .port_b = {GPIO_NUM_27, GPIO_NUM_25, GPIO_NUM_32, GPIO_NUM_17, GPIO_NUM_13, GPIO_NUM_21, GPIO_NUM_22},
    .led_j1 = GPIO_NUM_5,
    .led_j2 = GPIO_NUM_12,
    .led_bt = -1,
    .push_button_0 = GPIO_NUM_15,
    .push_button_1 = -1,
    .push_button_0_cb = toggle_enhanced_mode,
    .push_button_1_cb = NULL,
};

// Unijoysticle v2 A500
const struct uni_gpio_config uni_gpio_config_a500 = {
    .port_a = {GPIO_NUM_26, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_14, GPIO_NUM_33, GPIO_NUM_16},
    .port_b = {GPIO_NUM_27, GPIO_NUM_25, GPIO_NUM_32, GPIO_NUM_17, GPIO_NUM_13, GPIO_NUM_21, GPIO_NUM_22},
    .led_j1 = GPIO_NUM_5,
    .led_j2 = GPIO_NUM_12,
    .led_bt = GPIO_NUM_15,
    .push_button_0 = GPIO_NUM_34,
    .push_button_1 = GPIO_NUM_35,
    .push_button_0_cb = toggle_mouse_mode,
    .push_button_1_cb = swap_ports,
};

// Arananet's Unijoy2Amiga
const struct uni_gpio_config uni_gpio_config_singleport = {
    // Only has one port. Just mirror Port A with Port B.
    .port_a = {GPIO_NUM_26, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_14, GPIO_NUM_33, GPIO_NUM_16},
    .port_b = {GPIO_NUM_26, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_14, GPIO_NUM_33, GPIO_NUM_16},

    // Not sure whether the LEDs and Push button are correct.
    .led_j1 = GPIO_NUM_12,
    .led_j2 = -1,
    .led_bt = -1,
    .push_button_0 = GPIO_NUM_15,
    .push_button_1 = -1,
    .push_button_0_cb = toggle_enhanced_mode,
    .push_button_1_cb = NULL,
};

static const bd_addr_t zero_addr = {0, 0, 0, 0, 0, 0};

// --- Globals

const struct uni_gpio_config* g_uni_config = NULL;

// FIXME, should be part of g_gpio_config
static struct push_button g_push_buttons[PUSH_BUTTONS_MAX];
static EventGroupHandle_t g_event_group;
static EventGroupHandle_t g_auto_fire_group;

// Mouse "shared data from main task to mouse task.
static int32_t g_delta_x = 0;
static int32_t g_delta_y = 0;

// Autofire
static bool g_autofire_a_enabled = 0;
static bool g_autofire_b_enabled = 0;

//
// Platform Overrides
//
static void unijoysticle_init(int argc, const char** argv) {
    UNUSED(argc);
    UNUSED(argv);

    memset(&g_push_buttons, 0, sizeof(g_push_buttons));

    board_model_t model = get_board_model();

    switch (model) {
        case BOARD_MODEL_UNIJOYSTICLE2:
            logi("Hardware detected: Unijoysticle 2\n");
            g_uni_config = &uni_gpio_config_v2;
            break;
        case BOARD_MODEL_UNIJOYSTICLE2_PLUS:
            logi("Hardware detected: Unijoysticle 2+\n");
            g_uni_config = &uni_gpio_config_v2plus;
            break;
        case BOARD_MODEL_UNIJOYSTICLE2_A500:
            logi("Hardware detected: Unijoysticle 2 A500\n");
            g_uni_config = &uni_gpio_config_a500;
            break;
        case BOARD_MODEL_UNIJOYSTICLE2_SINGLE_PORT:
            logi("Hardware detected: Unijoysticle 2 single port\n");
            g_uni_config = &uni_gpio_config_singleport;
            break;
        default:
            logi("Hardware detected: ERROR!\n");
            g_uni_config = &uni_gpio_config_v2;
            break;
    }

    // Update Push Buttons config
    g_push_buttons[0].gpio = g_uni_config->push_button_0;
    g_push_buttons[0].callback = g_uni_config->push_button_0_cb;
    g_push_buttons[1].gpio = g_uni_config->push_button_1;
    g_push_buttons[1].callback = g_uni_config->push_button_1_cb;

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pin_bit_mask = 0;
    // Port A & B
    for (int i = 0; i < DB9_TOTAL_USABLE_PORTS; i++) {
        io_conf.pin_bit_mask |= SAFE_SET_BIT(g_uni_config->port_a[i]);
        io_conf.pin_bit_mask |= SAFE_SET_BIT(g_uni_config->port_b[i]);
    }

    // LEDs
    io_conf.pin_bit_mask |= SAFE_SET_BIT(g_uni_config->led_j1);
    io_conf.pin_bit_mask |= SAFE_SET_BIT(g_uni_config->led_j2);

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Set low all GPIOs... just in case.
    for (int i = 0; i < DB9_TOTAL_USABLE_PORTS; i++) {
        ESP_ERROR_CHECK(safe_gpio_set_level(g_uni_config->port_a[i], 0));
        ESP_ERROR_CHECK(safe_gpio_set_level(g_uni_config->port_b[i], 0));
    }

    // Turn On LEDs
    safe_gpio_set_level(g_uni_config->led_j1, 1);
    safe_gpio_set_level(g_uni_config->led_j2, 1);

    // Push Buttons
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    for (int i = 0; i < PUSH_BUTTONS_MAX; i++) {
        if (g_push_buttons[i].gpio == -1)
            continue;

        io_conf.intr_type = GPIO_INTR_ANYEDGE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        // GPIOs 34~39 don't have internal Pull-up resistors.
        io_conf.pull_up_en = (g_push_buttons[i].gpio < GPIO_NUM_34) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
        io_conf.pin_bit_mask = BIT(g_push_buttons[i].gpio);
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        // FIXME: "i" must match EVENT_MOUSE_0, value, etc.
        ESP_ERROR_CHECK(gpio_isr_handler_add(g_push_buttons[i].gpio, gpio_isr_handler_button, (void*)i));
    }

    // Split "events" from "auto_fire", since auto-fire is an on-going event.
    g_event_group = xEventGroupCreate();
    xTaskCreate(event_loop, "event_loop", 2048, NULL, 10, NULL);

    g_auto_fire_group = xEventGroupCreate();
    xTaskCreate(auto_fire_loop, "auto_fire_loop", 2048, NULL, 10, NULL);
    // xTaskCreatePinnedToCore(event_loop, "event_loop", 2048,
    // NULL, portPRIVILEGE_BIT, NULL, 1);
}

static void unijoysticle_on_init_complete(void) {
    // Turn off LEDs
    safe_gpio_set_level(g_uni_config->led_j1, 0);
    safe_gpio_set_level(g_uni_config->led_j2, 0);
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
        uni_bluetooth_enable_new_connections_safe(false);
        logi("unijoysticle: New gamepad connections disabled\n");
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
            safe_gpio_set_level(g_uni_config->led_j1, 0);
        if (ins->gamepad_seat == GAMEPAD_SEAT_B || ins->emu_mode == EMULATION_MODE_COMBO_JOY_JOY)
            safe_gpio_set_level(g_uni_config->led_j2, 0);

        ins->gamepad_seat = GAMEPAD_SEAT_NONE;
        ins->emu_mode = EMULATION_MODE_SINGLE_JOY;
    }

    // Regarless of how many connections are active, enable Bluetooth connections.
    uni_bluetooth_enable_new_connections_safe(true);
    logi("unijoysticle: New gamepad connections enabled\n");
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
        // d->emu_mode = EMULATION_MODE_COMBO_JOY_JOY;

        // ... unless it is a mouse which should try with PORT A.
        // Amiga/Atari ST use mice in PORT A. Undefined on the C64, but
        // most apps use it in PORT A as well.
        uint32_t mouse_cod = UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE;
        if ((d->cod & mouse_cod) == mouse_cod) {
            wanted_seat = GAMEPAD_SEAT_A;
            ins->emu_mode = EMULATION_MODE_SINGLE_MOUSE;
        }

        // If wanted port is already assigned, try with the next one
        if (used_joystick_ports & wanted_seat) {
            logi("unijoysticle: Port already assigned, trying another one\n");
            wanted_seat = (~wanted_seat) & GAMEPAD_SEAT_AB_MASK;
        }
    }

    set_gamepad_seat(d, wanted_seat);

    return 0;
}

static void unijoysticle_on_gamepad_data(uni_hid_device_t* d, uni_gamepad_t* gp) {
    if (d == NULL) {
        loge("ERROR: unijoysticle_on_device_gamepad_data: Invalid NULL device\n");
        return;
    }

    unijoysticle_instance_t* ins = get_unijoysticle_instance(d);

    // FIXME: Add support for EMULATION_MODE_COMBO_JOY_MOUSE
    uni_joystick_t joy, joy_ext;
    memset(&joy, 0, sizeof(joy));
    memset(&joy_ext, 0, sizeof(joy_ext));

    switch (ins->emu_mode) {
        case EMULATION_MODE_SINGLE_JOY:
            uni_joy_to_single_joy_from_gamepad(gp, &joy);
            process_joystick(&joy, ins->gamepad_seat);
            break;
        case EMULATION_MODE_SINGLE_MOUSE:
            uni_joy_to_single_mouse_from_gamepad(gp, &joy);
            process_mouse(d, gp->axis_x, gp->axis_y, gp->buttons);
            break;
        case EMULATION_MODE_COMBO_JOY_JOY:
            uni_joy_to_combo_joy_joy_from_gamepad(gp, &joy, &joy_ext);
            process_joystick(&joy, GAMEPAD_SEAT_A);
            process_joystick(&joy_ext, GAMEPAD_SEAT_B);
            break;
        case EMULATION_MODE_COMBO_JOY_MOUSE:
            uni_joy_to_combo_joy_mouse_from_gamepad(gp, &joy, &joy_ext);
            process_joystick(&joy, GAMEPAD_SEAT_B);
            process_joystick(&joy_ext, GAMEPAD_SEAT_A);
            break;
        default:
            loge("unijoysticle: Unsupported emulation mode: %d\n", ins->emu_mode);
            break;
    }
}

static int32_t unijoysticle_get_property(uni_platform_property_t key) {
    if (key != UNI_PLATFORM_PROPERTY_DELETE_STORED_KEYS)
        return -1;

    // Hi-released, Low-pressed
    return !gpio_get_level(g_uni_config->push_button_1);
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
        logi(
            "INFO: unijoysticle_on_device_oob_event: No effect in single port "
            "boards\n");
        return;
    }

    unijoysticle_instance_t* ins = get_unijoysticle_instance(d);

    if (ins->gamepad_seat == GAMEPAD_SEAT_NONE) {
        logi(
            "unijoysticle: cannot swap port since device has joystick_port = "
            "GAMEPAD_SEAT_NONE\n");
        return;
    }

    // This could happen if device is any Combo emu mode.
    if (ins->gamepad_seat == (GAMEPAD_SEAT_A | GAMEPAD_SEAT_B)) {
        logi(
            "unijoysticle: cannot swap port since has more than one port "
            "associated with. Leave emu mode and try again.\n");
        return;
    }

    // Swap joysticks iff one device is attached.
    int num_devices = 0;
    for (int j = 0; j < CONFIG_BLUEPAD32_MAX_DEVICES; j++) {
        uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(j);
        if ((bd_addr_cmp(tmp_d->conn.btaddr, zero_addr) != 0) && (get_unijoysticle_instance(tmp_d)->gamepad_seat > 0)) {
            num_devices++;
            if (num_devices > 1) {
                logi(
                    "unijoysticle: cannot swap joystick ports when more than one "
                    "device is attached\n");
                uni_hid_device_dump_all();
                return;
            }
        }
    }

    // swap joystick A with B
    uni_gamepad_seat_t seat = (ins->gamepad_seat == GAMEPAD_SEAT_A) ? GAMEPAD_SEAT_B : GAMEPAD_SEAT_A;
    set_gamepad_seat(d, seat);

    // Clear joystick after switch to avoid having a line "On".
    uni_joystick_t joy;
    memset(&joy, 0, sizeof(joy));
    process_joystick(&joy, GAMEPAD_SEAT_A);
    process_joystick(&joy, GAMEPAD_SEAT_B);
}

//
// Helpers
//

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
    // Reserved:    Low      Hi        Lo
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

static void process_mouse(uni_hid_device_t* d, int32_t delta_x, int32_t delta_y, uint16_t buttons) {
    UNUSED(d);
    static uint16_t prev_buttons = 0;
    logd("unijoysticle: mouse: x=%d, y=%d, buttons=0x%4x\n", delta_x, delta_y, buttons);

    // Mouse is implemented using a quadrature encoding
    // FIXME: Passing values to mouse task using global variables.
    // This is, of course, error-prone to races and what not, but
    // seeems to be good enough for our purpose.
    if (delta_x || delta_y) {
        g_delta_x = delta_x;
        g_delta_y = delta_y;
        xEventGroupSetBits(g_event_group, BIT(EVENT_MOUSE));
    }
    if (buttons != prev_buttons) {
        prev_buttons = buttons;
        safe_gpio_set_level(g_uni_config->port_a_named.fire, (buttons & BUTTON_A));
        safe_gpio_set_level(g_uni_config->port_a_named.pot_x, (buttons & BUTTON_B));
        safe_gpio_set_level(g_uni_config->port_a_named.pot_y, (buttons & BUTTON_X));
    }
}

static void process_joystick(const uni_joystick_t* joy, uni_gamepad_seat_t seat) {
    if (seat == GAMEPAD_SEAT_A) {
        joy_update_port(joy, g_uni_config->port_a);
        g_autofire_a_enabled = joy->auto_fire;
    } else if (seat == GAMEPAD_SEAT_B) {
        joy_update_port(joy, g_uni_config->port_b);
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
    safe_gpio_set_level(g_uni_config->led_j1, status_a);
    safe_gpio_set_level(g_uni_config->led_j2, status_b);

    bool lightbar_or_led_set = false;
    // Try with LightBar and/or Player LEDs. Some devices like DualSense support
    // both. Use them both when available.
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

    //  If Lightbar or Player LEDs cannot be set, use rumble as fallback option
    if (!lightbar_or_led_set && d->report_parser.set_rumble != NULL) {
        d->report_parser.set_rumble(d, 0x80 /* value */, 0x04 /* duration */);
    }
}

static void joy_update_port(const uni_joystick_t* joy, const gpio_num_t* gpios) {
    logd("up=%d, down=%d, left=%d, right=%d, fire=%d, potx=%d, poty=%d\n", joy->up, joy->down, joy->left, joy->right,
         joy->fire, joy->pot_x, joy->pot_y);

    safe_gpio_set_level(gpios[0], !!joy->up);
    safe_gpio_set_level(gpios[1], !!joy->down);
    safe_gpio_set_level(gpios[2], !!joy->left);
    safe_gpio_set_level(gpios[3], !!joy->right);

    // Only update fire if auto-fire is off. Otherwise it will conflict.
    if (!joy->auto_fire) {
        safe_gpio_set_level(gpios[4], !!joy->fire);
    }

    // Check for valid GPIO values since some models might have it disabled.
    safe_gpio_set_level(gpios[5], !!joy->pot_x);
    safe_gpio_set_level(gpios[6], !!joy->pot_y);
}

static void event_loop(void* arg) {
    // timeout of 10s
    const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;
    while (1) {
        EventBits_t uxBits = xEventGroupWaitBits(
            g_event_group, BIT(EVENT_BUTTON_0) | BIT(EVENT_BUTTON_1) | BIT(EVENT_MOUSE), pdTRUE, pdFALSE, xTicksToWait);

        // timeout ?
        if (uxBits == 0)
            continue;

        if (uxBits & BIT(EVENT_MOUSE))
            handle_event_mouse();

        if (uxBits & BIT(EVENT_BUTTON_0))
            handle_event_button(EVENT_BUTTON_0);

        if (uxBits & BIT(EVENT_BUTTON_1))
            handle_event_button(EVENT_BUTTON_1);
    }
}

static void auto_fire_loop(void* arg) {
    // timeout of 10s
    const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;
    const TickType_t delayTicks = AUTOFIRE_FREQ_MS / portTICK_PERIOD_MS;
    while (1) {
        EventBits_t uxBits = xEventGroupWaitBits(g_auto_fire_group, BIT(EVENT_AUTOFIRE), pdTRUE, pdFALSE, xTicksToWait);

        // timeout ?
        if (uxBits == 0)
            continue;

        while (g_autofire_a_enabled || g_autofire_b_enabled) {
            if (g_autofire_a_enabled)
                safe_gpio_set_level(g_uni_config->port_a_named.fire, 1);
            if (g_autofire_b_enabled)
                safe_gpio_set_level(g_uni_config->port_b_named.fire, 1);

            vTaskDelay(delayTicks);

            if (g_autofire_a_enabled)
                safe_gpio_set_level(g_uni_config->port_a_named.fire, 0);
            if (g_autofire_b_enabled)
                safe_gpio_set_level(g_uni_config->port_b_named.fire, 0);

            vTaskDelay(delayTicks);
        }
    }
}

// Mouse handler
void handle_event_mouse() {
    // Copy global variables to local, in case they changed.
    int delta_x = g_delta_x;
    int delta_y = g_delta_y;

    // Should not happen, but better safe than sorry
    if (delta_x == 0 && delta_y == 0)
        return;

    int dir_x = 0;
    int dir_y = 0;
    float a = atan2f(delta_y, delta_x) + M_PI;
    float d = a * (180.0 / M_PI);
    logd("x=%d, y=%d, r=%f, d=%f\n", delta_x, delta_y, a, d);

    if (d < 60 || d >= 300) {
        // Moving left? (a<60, a>=300)
        dir_x = -1;
    } else if (d > 120 && d <= 240) {
        // Moving right? (120 < a <= 240)
        dir_x = 1;
    }

    if (d > 30 && d <= 150) {
        // Moving down? (30 < a <= 150)
        dir_y = -1;
    } else if (d > 210 && d <= 330) {
        // Moving up?
        dir_y = 1;
    }

    int delay = MOUSE_DELAY_BETWEEN_EVENT_US - (abs(delta_x) + abs(delta_y)) * 3;

    if (dir_y != 0) {
        mouse_move_y(dir_y, delay);
    }
    if (dir_x != 0) {
        mouse_move_x(dir_x, delay);
    }
}

static void mouse_send_move(int pin_a, int pin_b, uint32_t delay) {
    safe_gpio_set_level(pin_a, 1);
    delay_us(delay);
    safe_gpio_set_level(pin_b, 1);
    delay_us(delay);

    safe_gpio_set_level(pin_a, 0);
    delay_us(delay);
    safe_gpio_set_level(pin_b, 0);
    delay_us(delay);

    vTaskDelay(0);
}

static void mouse_move_x(int dir, uint32_t delay) {
    // up, down, left, right, fire
    if (dir < 0)
        mouse_send_move(g_uni_config->port_a[0], g_uni_config->port_a[1], delay);
    else
        mouse_send_move(g_uni_config->port_a[1], g_uni_config->port_a[0], delay);
}

static void mouse_move_y(int dir, uint32_t delay) {
    // up, down, left, right, fire
    if (dir < 0)
        mouse_send_move(g_uni_config->port_a[2], g_uni_config->port_a[3], delay);
    else
        mouse_send_move(g_uni_config->port_a[3], g_uni_config->port_a[2], delay);
}

// Delay in microseconds. Anything bigger than 1000 microseconds
// (1 millisecond) should be scheduled using vTaskDelay(), which
// will allow context-switch and allow other tasks to run.
static void delay_us(uint32_t delay) {
    if (delay > 1000)
        vTaskDelay(delay / 1000);
    else
        ets_delay_us(delay);
}

static void IRAM_ATTR gpio_isr_handler_button(void* arg) {
    int button_idx = (int)arg;
    struct push_button* pb = &g_push_buttons[button_idx];
    // Button released ?
    if (gpio_get_level(pb->gpio)) {
        pb->last_time_pressed_us = esp_timer_get_time();
        return;
    }

    // Button pressed
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(g_event_group, BIT(button_idx), &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE)
        portYIELD_FROM_ISR();
}

static void handle_event_button(int button_idx) {
    // FIXME: Debouncer might fail when releasing the button.
    // Implement something like this one:
    // https://hackaday.com/2015/12/10/embed-with-elliot-debounce-your-noisy-buttons-part-ii/
    const int64_t button_threshold_time_us = 300 * 1000;  // 300ms

    struct push_button* pb = &g_push_buttons[button_idx];

    // Regardless of the state, ignore the event if not enough time passed.
    int64_t now = esp_timer_get_time();
    if ((now - pb->last_time_pressed_us) < button_threshold_time_us)
        return;

    pb->last_time_pressed_us = now;

    // "up" button is released. Ignore event.
    if (gpio_get_level(pb->gpio)) {
        return;
    }

    // "down", button pressed.
    logi("handle_event_button(%d): %d -> %d\n", button_idx, pb->enabled, !pb->enabled);
    pb->enabled = !pb->enabled;

    pb->callback(button_idx);
}

static void toggle_enhanced_mode(int button_idx) {
    UNUSED(button_idx);

    // Change emulation mode
    int num_devices = 0;
    uni_hid_device_t* d = NULL;
    for (int j = 0; j < CONFIG_BLUEPAD32_MAX_DEVICES; j++) {
        uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(j);
        if (bd_addr_cmp(tmp_d->conn.btaddr, zero_addr) != 0 && tmp_d->conn.control_cid != 0 &&
            tmp_d->conn.interrupt_cid != 0) {
            num_devices++;
            d = tmp_d;
        }
    }

    if (d == NULL) {
        loge("unijoysticle: Cannot find valid HID device\n");
    }

    if (num_devices != 1) {
        loge("unijoysticle: cannot change mode. Expected num_devices=1, actual=%d\n", num_devices);
        return;
    }

    unijoysticle_instance_t* ins = get_unijoysticle_instance(d);

    if (ins->emu_mode == EMULATION_MODE_SINGLE_JOY) {
        ins->emu_mode = EMULATION_MODE_COMBO_JOY_JOY;
        ins->prev_gamepad_seat = ins->gamepad_seat;
        set_gamepad_seat(d, GAMEPAD_SEAT_A | GAMEPAD_SEAT_B);
        logi("unijoysticle: Emulation mode = Combo Joy Joy\n");

        uni_bluetooth_enable_new_connections_safe(false);
        logi("unijoysticle: New gamepad connections disabled\n");

    } else if (ins->emu_mode == EMULATION_MODE_COMBO_JOY_JOY) {
        ins->emu_mode = EMULATION_MODE_SINGLE_JOY;
        set_gamepad_seat(d, ins->prev_gamepad_seat);
        // Turn on only the valid one
        logi("unijoysticle: Emulation mode = Single Joy\n");

        uni_bluetooth_enable_new_connections_safe(true);
        logi("unijoysticle: New gamepad connections enabled\n");

    } else {
        loge("unijoysticle: Cannot switch emu mode. Current mode: %d\n", ins->emu_mode);
    }
}

static void toggle_mouse_mode(int button_idx) {
    // Not implemented. A500 feature
    logi("toggle_mouse_mode called: %d\n", button_idx);
}

static void swap_ports(int button_idx) {
    // Not implemented. A500 feature
    logi("swap_ports called: %d\n", button_idx);
}

// In some boards, not all GPIOs are set. If so, don't try change their values.
static esp_err_t safe_gpio_set_level(gpio_num_t gpio, int value) {
    if (gpio == -1)
        return ESP_OK;
    return gpio_set_level(gpio, value);
}

//
// Entry Point
//
struct uni_platform* uni_platform_unijoysticle_create(void) {
    static struct uni_platform plat;

    plat.name = "unijoysticle2";
    plat.init = unijoysticle_init;
    plat.on_init_complete = unijoysticle_on_init_complete;
    plat.on_device_connected = unijoysticle_on_device_connected;
    plat.on_device_disconnected = unijoysticle_on_device_disconnected;
    plat.on_device_ready = unijoysticle_on_device_ready;
    plat.on_device_oob_event = unijoysticle_on_device_oob_event;
    plat.on_gamepad_data = unijoysticle_on_gamepad_data;
    plat.get_property = unijoysticle_get_property;

    return &plat;
}
