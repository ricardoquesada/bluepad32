/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2022 Ricardo Quesada

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

#ifndef UNI_MOUSE_H
#define UNI_MOUSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "uni_common.h"

enum {
    MOUSE_BUTTON_LEFT = BIT(0),
    MOUSE_BUTTON_RIGHT = BIT(1),
    MOUSE_BUTTON_MIDDLE = BIT(2),

    MOUSE_BUTTON_AUX_0 = BIT(3),
    MOUSE_BUTTON_AUX_1 = BIT(4),
    MOUSE_BUTTON_AUX_2 = BIT(5),
    MOUSE_BUTTON_AUX_3 = BIT(6),
    MOUSE_BUTTON_AUX_4 = BIT(7),
    MOUSE_BUTTON_AUX_5 = BIT(8),
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
