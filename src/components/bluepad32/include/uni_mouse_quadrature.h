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

enum {
    UNI_MOUSE_QUADRATURE_PORT_0,
    UNI_MOUSE_QUADRATURE_PORT_1,
    UNI_MOUSE_QUADRATURE_PORT_MAX,
};

enum {
    UNI_MOUSE_QUADRATURE_ENCODER_H,
    UNI_MOUSE_QUADRATURE_ENCODER_V,
    UNI_MOUSE_QUADRATURE_ENCODER_MAX,
};

struct uni_mouse_quadrature_encoder {
    int gpio_a;
    int gpio_b;
};

struct uni_mouse_quadrature_port {
    // Each port has two encoders: horizontal and vertical.
    struct uni_mouse_quadrature_encoder encoders[UNI_MOUSE_QUADRATURE_ENCODER_MAX];
};

/*
 * cpu_id indicates in which CPU the quadrature task runs.
 * x1,x2: GPIOs for horizontal movement
 * y1,y2: GPIOs for vertical movement
 */
void uni_mouse_quadrature_init(int cpu_id,
                               struct uni_mouse_quadrature_port port_a,
                               struct uni_mouse_quadrature_port port_b);
void uni_mouse_quadrature_update(int port_idx, int32_t dx, int32_t dy);
void uni_mouse_quadrature_start();
void uni_mouse_quadrature_pause();
void uni_mouse_quadrature_deinit();

#endif  // UNI_MOUSE_QUADRATURE_H
