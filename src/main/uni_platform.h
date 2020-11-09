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

#ifndef UNI_PLATFORM_H
#define UNI_PLATFORM_H

#include <stdint.h>

#include "uni_joystick.h"

void uni_platform_init(int argc, const char** argv);

// events
void uni_platform_on_init_complete(void);
void uni_platform_on_port_assign_changed(uni_joystick_port_t port);
void uni_platform_on_joy_a_data(uni_joystick_t* joy);
void uni_platform_on_joy_b_data(uni_joystick_t* joy);
void uni_platform_on_mouse_data(int32_t delta_x, int32_t delta_y,
                                uint16_t buttons);
uint8_t uni_platform_is_button_pressed(void);

#endif  // UNI_PLATFORM_H