/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2020 Ricardo Quesada

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

#ifndef UNI_PLATFORM_ARDUINO_H
#define UNI_PLATFORM_ARDUINO_H

#include <stdint.h>

#include "uni_gamepad.h"
#include "uni_platform.h"

typedef struct {
  // Indicates which gamepad it is. Goes from 0 to 3.
  uint8_t idx;

  // Type of gamepad: PS4, PS3, Xbox, etc..?
  uint8_t type;

  // The gamepad data: buttons, axis, etc.
  uni_gamepad_t data;
} arduino_gamepad_t;

struct uni_platform* uni_platform_arduino_create(void);

arduino_gamepad_t arduino_get_gamepad_data(int idx);
void arduino_set_player_leds(int idx, uint8_t leds);
void arduino_set_lightbar_color(int idx, uint8_t r, uint8_t g, uint8_t b);
void arduino_set_rumble(int idx, uint8_t force, uint8_t duration);

#endif  // UNI_PLATFORM_ARDUINO_H
