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

#ifndef UNI_PLATFORM_UNIJOYSTICLE_AMIGA_H
#define UNI_PLATFORM_UNIJOYSTICLE_AMIGA_H

#include <stdint.h>

#include "uni_hid_device.h"

void uni_platform_unijoysticle_amiga_register_cmds(void);
void uni_platform_unijoysticle_amiga_on_init_complete(void);
void uni_platform_unijoysticle_amiga_maybe_enable_mouse_timers(void);
void uni_platform_unijoysticle_amiga_process_mouse(uni_hid_device_t* d,
                                                   uni_gamepad_seat_t seat,
                                                   int32_t delta_x,
                                                   int32_t delta_y,
                                                   uint16_t buttons);
void uni_platform_unijoysticle_amiga_version(void);
#endif  // UNI_PLATFORM_UNIJOYSTICLE_AMIGA_H
