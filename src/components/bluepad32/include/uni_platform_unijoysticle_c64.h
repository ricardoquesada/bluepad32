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

#ifndef UNI_PLATFORM_UNIJOYSTICLE_C64_H
#define UNI_PLATFORM_UNIJOYSTICLE_C64_H

#include <driver/gpio.h>
#include <stdint.h>

#include "uni_gamepad.h"
#include "uni_hid_device.h"

// Number of SYNC IRQs. One for each port
#define UNI_PLATFORM_UNIJOYSTICLE_C64_SYNC_IRQ_MAX 2

typedef enum {
    UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_INVALID,   // Invalid
    UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_3BUTTONS,  // Pots are used for mouse, paddle, extra buttons
    UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_5BUTTONS,  // Pots are used for mouse, paddle, extra buttons
    UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_RUMBLE,    // Pots are used to toggle rumble

    UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_COUNT,
} uni_platform_unijoysticle_c64_pot_mode_t;

void uni_platform_unijoysticle_c64_register_cmds(void);
void uni_platform_unijoysticle_c64_on_init_complete(const gpio_num_t* port_a, const gpio_num_t* port_b);
void uni_platform_unijoysticle_c64_set_pot_mode(uni_platform_unijoysticle_c64_pot_mode_t mode);
void uni_platform_unijoysticle_c64_version(void);
void uni_platform_unijoysticle_c64_set_pot_level(gpio_num_t gpio_num, uint8_t level);
void uni_platform_unijoysticle_c64_process_gamepad(uni_hid_device_t* d,
                                                   uni_gamepad_t* gp,
                                                   uni_gamepad_seat_t seat,
                                                   const gpio_num_t* port_a,
                                                   const gpio_num_t* port_b);

#endif  // UNI_PLATFORM_UNIJOYSTICLE_C64_H
