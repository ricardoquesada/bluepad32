// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_PLATFORM_UNIJOYSTICLE_H
#define UNI_PLATFORM_UNIJOYSTICLE_H

#include <stdbool.h>

#include <driver/gpio.h>

#include "controller/uni_controller.h"
#include "platform/uni_platform.h"
#include "uni_hid_device.h"

// How many Balance Board entries to store
#define UNI_PLATFORM_UNIJOYSTICLE_BB_VALUES_ARRAY_COUNT 8

// Console commands
typedef enum {
    UNI_PLATFORM_UNIJOYSTICLE_CMD_SWAP_PORTS,

    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_GAMEPAD_MODE_NORMAL,     // Basic mode
    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_GAMEPAD_MODE_TWINSTICK,  // Twin Stick mode (AKA Enhanced)
    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_GAMEPAD_MODE_MOUSE,      // Mouse mode
    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_GAMEPAD_MODE_NEXT,       // Set the "next" gamepad mode in the list
    UNI_PLATFORM_UNIJOYSTICLE_CMD_GET_GAMEPAD_MODE,

    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_C64_POT_MODE_3BUTTONS,  // Used for 3 buttons
    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_C64_POT_MODE_5BUTTONS,  // Used for 5 buttons (select + start)
    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_C64_POT_MODE_RUMBLE,    // C64 can enable rumble via Pots
    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_C64_POT_MODE_PADDLE,    // Use for paddle

    UNI_PLATFORM_UNIJOYSTICLE_CMD_COUNT,
} uni_platform_unijoysticle_cmd_t;

// Different emulation modes
// It is a bitmap since variants declare which modes are supported.
typedef enum {
    UNI_PLATFORM_UNIJOYSTICLE_GAMEPAD_MODE_NONE,                // None. Should not happen
    UNI_PLATFORM_UNIJOYSTICLE_GAMEPAD_MODE_NORMAL = BIT(0),     // Regular mode, controls one joystick
    UNI_PLATFORM_UNIJOYSTICLE_GAMEPAD_MODE_TWINSTICK = BIT(1),  // Twin Stick mode (AKA Enhanced)
    UNI_PLATFORM_UNIJOYSTICLE_GAMEPAD_MODE_MOUSE = BIT(2),      // Gamepad behaves like mouse
} uni_platform_unijoysticle_gamepad_mode_t;

enum {
    UNI_PLATFORM_UNIJOYSTICLE_VARIANT_FLAG_QUADRATURE_MOUSE = BIT(0),
    // DualShock4 / DualSense mouse
    UNI_PLATFORM_UNIJOYSTICLE_VARIANT_FLAG_VIRTUAL_MOUSE = BIT(1),
    // platform uses two button joysticks, like MSX
    UNI_PLATFORM_UNIJOYSTICLE_VARIANT_FLAG_TWO_BUTTONS = BIT(2),
};

enum {
    UNI_PLATFORM_UNIJOYSTICLE_MOUSE_EMULATION_FROM_BOARD_MODEL,  // Used internally for NVS (Deprecated)
    UNI_PLATFORM_UNIJOYSTICLE_MOUSE_EMULATION_AMIGA,
    UNI_PLATFORM_UNIJOYSTICLE_MOUSE_EMULATION_ATARIST,
    UNI_PLATFORM_UNIJOYSTICLE_MOUSE_EMULATION_AUTO,  // Each variant defines it

    UNI_PLATFORM_UNIJOYSTICLE_MOUSE_EMULATION_COUNT,
};

enum {
    UNI_PLATFORM_UNIJOYSTICLE_JOY_UP,       // Pin 1
    UNI_PLATFORM_UNIJOYSTICLE_JOY_DOWN,     // Pin 2
    UNI_PLATFORM_UNIJOYSTICLE_JOY_LEFT,     // Pin 3
    UNI_PLATFORM_UNIJOYSTICLE_JOY_RIGHT,    // Pin 4
    UNI_PLATFORM_UNIJOYSTICLE_JOY_FIRE,     // Pin 6
    UNI_PLATFORM_UNIJOYSTICLE_JOY_BUTTON2,  // Pin 9, AKA Pot X (C64), Pot Y (Amiga)
    UNI_PLATFORM_UNIJOYSTICLE_JOY_BUTTON3,  // Pin 5, AKA Pot Y (C64), Pot X (Amiga)

    UNI_PLATFORM_UNIJOYSTICLE_JOY_MAX,
};

enum {
    UNI_PLATFORM_UNIJOYSTICLE_PUSH_BUTTON_0,  // Changes mode:
    UNI_PLATFORM_UNIJOYSTICLE_PUSH_BUTTON_1,  // Swap ports
    UNI_PLATFORM_UNIJOYSTICLE_PUSH_BUTTON_MAX,
};

enum {
    UNI_PLATFORM_UNIJOYSTICLE_LED_J1,  // Player #1 connected, Green
    UNI_PLATFORM_UNIJOYSTICLE_LED_J2,  // Player #2 connected, Red
    UNI_PLATFORM_UNIJOYSTICLE_LED_BT,  // Bluetooth enabled, Blue
    UNI_PLATFORM_UNIJOYSTICLE_LED_MAX,
};

// Number of SYNC IRQs. One for each port. Only valid for C64.
#define UNI_PLATFORM_UNIJOYSTICLE_SYNC_IRQ_MAX 2

// The platform "instance"
typedef struct uni_platform_unijoysticle_instance_s {
    uni_platform_unijoysticle_gamepad_mode_t gamepad_mode;  // type of emulation mode
    uni_gamepad_seat_t seat;                                // which "seat" (port) is being used
    uni_gamepad_seat_t prev_seat;                           // which "seat" (port) was used before switching emu mode

    bool swap_ports_in_twinstick;  // whether the ports in Twin Stick mode are swapped.

    // Used by Balance Board to determine joystick movements/fire
    uni_balance_board_state_t bb_state;

    // Debouncer for buttons and keys
    uint32_t debouncer;
} uni_platform_unijoysticle_instance_t;
_Static_assert(sizeof(uni_platform_unijoysticle_instance_t) < HID_DEVICE_MAX_PLATFORM_DATA,
               "Unijoysticle intance too big");

typedef void (*uni_platform_unijoysticle_button_cb_t)(int button_idx);

// These are const values. Cannot be modified in runtime.
struct uni_platform_unijoysticle_push_button {
    gpio_num_t gpio;
    uni_platform_unijoysticle_button_cb_t callback;
};

struct uni_platform_unijoysticle_gpio_config {
    gpio_num_t port_a[UNI_PLATFORM_UNIJOYSTICLE_JOY_MAX];
    gpio_num_t port_b[UNI_PLATFORM_UNIJOYSTICLE_JOY_MAX];
    gpio_num_t leds[UNI_PLATFORM_UNIJOYSTICLE_LED_MAX];
    struct uni_platform_unijoysticle_push_button push_buttons[UNI_PLATFORM_UNIJOYSTICLE_PUSH_BUTTON_MAX];
    gpio_num_t sync_irq[UNI_PLATFORM_UNIJOYSTICLE_SYNC_IRQ_MAX];
};

struct uni_platform_unijoysticle_variant {
    // The name of the variant: A500, C64, 800XL, etc.
    const char* name;

    // GPIO configuration
    const struct uni_platform_unijoysticle_gpio_config* gpio_config;

    // Which features are supported.
    // E.g.: Quadrature Mouse
    uint32_t flags;

    // Which "modes" are supported.
    // E.g.: Twin stick, normal, mouse.
    uint32_t supported_modes;

    // If any, which mouse emulation should be used by default
    int default_mouse_emulation;

    // Each platform defines the preferred seat (AKA port)
    // for the device type. If the seat is already used, it will use
    // the available one.
    uni_gamepad_seat_t preferred_seat_for_joystick;
    uni_gamepad_seat_t preferred_seat_for_mouse;

    // Variant "callbacks".

    // Print additional info about the version
    void (*print_version)(void);

    // on_init_complete is called when initialization finishes
    void (*on_init_complete)(void);

    // Register console commands. Optional
    void (*register_console_cmds)(void);

    // Set the pot values
    void (*set_gpio_level_for_pot)(gpio_num_t gpio, bool value);

    // Process gamepad misc buttons
    // Returns "True" if the Misc buttons where processed. Otherwise, "False"
    bool (*process_gamepad_misc_buttons)(uni_hid_device_t* d, uni_gamepad_seat_t seat, uint8_t misc_buttons);
};

struct uni_platform* uni_platform_unijoysticle_create(void);

// Can be called from any thread. The command will get executed in the btthread.
void uni_platform_unijoysticle_run_cmd(uni_platform_unijoysticle_cmd_t cmd);
void uni_platform_unijoysticle_on_push_button_mode_pressed(int button_idx);
void uni_platform_unijoysticle_on_push_button_swap_pressed(int button_idx);
uni_platform_unijoysticle_instance_t* uni_platform_unijoysticle_get_instance(const uni_hid_device_t* d);

#endif  // UNI_PLATFORM_UNIJOYSTICLE_H
