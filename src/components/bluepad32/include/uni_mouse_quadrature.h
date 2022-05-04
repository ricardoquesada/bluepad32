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

#ifndef UNI_MOUSE_QUADRATURE_H
#define UNI_MOUSE_QUADRATURE_H

#include <stdint.h>

/*
 * cpu_id indicates in which CPU the quadrature task runs.
 * x1,x2: GPIOs for horizontal movement
 * y1,y2: GPIOs for vertical movement
 */
void uni_mouse_quadrature_init(int cpu_id, int gpio_x1, int gpio_x2, int gpio_y1, int gpio_y2);
void uni_mouse_quadrature_update(int32_t dx, int32_t dy);
void uni_mouse_quadrature_start();
void uni_mouse_quadrature_pause();
void uni_mouse_quadrature_deinit();

#endif // UNI_MOUSE_QUADRATURE_H
