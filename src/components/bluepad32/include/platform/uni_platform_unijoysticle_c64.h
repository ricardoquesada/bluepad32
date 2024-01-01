// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_PLATFORM_UNIJOYSTICLE_C64_H
#define UNI_PLATFORM_UNIJOYSTICLE_C64_H

#include <driver/gpio.h>
#include <stdint.h>

#include "controller/uni_controller.h"
#include "uni_hid_device.h"
#include "uni_platform_unijoysticle.h"

typedef enum {
    UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_INVALID,   // Invalid
    UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_3BUTTONS,  // Pots are used for extra buttons
    UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_5BUTTONS,  // Pots are used for extra buttons
    UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_RUMBLE,    // Pots are used to toggle rumble
    UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_PADDLE,    // Pots are used to control paddle (experimental)

    UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_COUNT,
} uni_platform_unijoysticle_c64_pot_mode_t;

void uni_platform_unijoysticle_c64_set_pot_mode(uni_platform_unijoysticle_c64_pot_mode_t mode);
void uni_platform_unijoysticle_c64_set_pot_level(gpio_num_t gpio_num, uint8_t level);

const struct uni_platform_unijoysticle_variant* uni_platform_unijoysticle_c64_create_variant(void);
#endif  // UNI_PLATFORM_UNIJOYSTICLE_C64_H
