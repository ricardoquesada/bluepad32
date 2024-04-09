// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_MOUSE_H
#define UNI_MOUSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "uni_common.h"

enum {
    UNI_MOUSE_BUTTON_LEFT = BIT(0),
    UNI_MOUSE_BUTTON_RIGHT = BIT(1),
    UNI_MOUSE_BUTTON_MIDDLE = BIT(2),

    UNI_MOUSE_BUTTON_AUX_0 = BIT(3),  // Backward
    UNI_MOUSE_BUTTON_AUX_1 = BIT(4),  // Forward
    UNI_MOUSE_BUTTON_AUX_2 = BIT(5),
    UNI_MOUSE_BUTTON_AUX_3 = BIT(6),
    UNI_MOUSE_BUTTON_AUX_4 = BIT(7),
    UNI_MOUSE_BUTTON_AUX_5 = BIT(8),
};

typedef struct {
    int32_t delta_x;
    int32_t delta_y;
    uint16_t buttons;
    int8_t scroll_wheel;
    uint8_t misc_buttons;
} uni_mouse_t;

void uni_mouse_dump(const uni_mouse_t* ms);

#ifdef __cplusplus
}
#endif

#endif  // UNI_MOUSE_H
