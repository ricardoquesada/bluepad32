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

#ifndef UNI_PLATFORM_UNIJOYSTICLE_H
#define UNI_PLATFORM_UNIJOYSTICLE_H

#include <driver/gpio.h>

#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_platform.h"

// Console commands
typedef enum {
    UNI_PLATFORM_UNIJOYSTICLE_CMD_SWAP_PORTS,

    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_GAMEPAD_MODE_NORMAL,    // Basic mode
    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_GAMEPAD_MODE_ENHANCED,  // Enhanced mode
    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_GAMEPAD_MODE_MOUSE,     // Mouse mode
    UNI_PLATFORM_UNIJOYSTICLE_CMD_GET_GAMEPAD_MODE,

    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_C64_POT_MODE_3BUTTONS,  // Used for mouse, paddle & 3 buttons
    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_C64_POT_MODE_5BUTTONS,  // Used for mouse, paddle & 5 buttons (select + start)
    UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_C64_POT_MODE_RUMBLE,    // C64 can enable rumble via Pots

    UNI_PLATFORM_UNIJOYSTICLE_CMD_COUNT,
} uni_platform_unijoysticle_cmd_t;

// Different emulation modes
typedef enum {
    UNI_PLATFORM_UNIJOYSTICLE_EMULATION_MODE_NONE,             // None. Should not happen
    UNI_PLATFORM_UNIJOYSTICLE_EMULATION_MODE_SINGLE_JOY,       // Basic mode
    UNI_PLATFORM_UNIJOYSTICLE_EMULATION_MODE_COMBO_JOY_JOY,    // Enhanced mode: Dual stick mode
    UNI_PLATFORM_UNIJOYSTICLE_EMULATION_MODE_COMBO_JOY_MOUSE,  // Gamepad behaves like mouse
    UNI_PLATFORM_UNIJOYSTICLE_EMULATION_MODE_COUNT,
} uni_platform_unijoysticle_emulation_mode_t;

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

// The platform "instance"
typedef struct uni_platform_unijoysticle_instance_s {
    uni_platform_unijoysticle_emulation_mode_t gamepad_mode;  // type of gamepad mode
    uni_gamepad_seat_t seat;                                  // which "seat" (port) is being used
    uni_gamepad_seat_t prev_seat;                             // which "seat" (port) was used before
                                                              // switching emu mode
} uni_platform_unijoysticle_instance_t;
_Static_assert(sizeof(uni_platform_unijoysticle_instance_t) < HID_DEVICE_MAX_PLATFORM_DATA,
               "Unijoysticle intance too big");

struct uni_platform* uni_platform_unijoysticle_create(void);
// Can be called from any thread. The command will get executed in the btthread.
void uni_platform_unijoysticle_run_cmd(uni_platform_unijoysticle_cmd_t cmd);
gpio_num_t uni_platform_unijoysticle_get_gpio_sync_irq(int idx);
gpio_num_t uni_platform_unijoysticle_get_gpio_port_a(int idx);
gpio_num_t uni_platform_unijoysticle_get_gpio_port_b(int idx);
uni_platform_unijoysticle_instance_t* uni_platform_unijoysticle_get_instance(const uni_hid_device_t* d);

#endif  // UNI_PLATFORM_UNIJOYSTICLE_H
