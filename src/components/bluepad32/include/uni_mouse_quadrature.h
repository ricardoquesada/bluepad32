// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_MOUSE_QUADRATURE_H
#define UNI_MOUSE_QUADRATURE_H

#include <stdint.h>

// How many ports (mice) are supported
// In this case, up to two mice are supported.
// Required, at least, for Lemmings. In Amiga, it has a 2-player mode that uses two mice.
enum {
    UNI_MOUSE_QUADRATURE_PORT_0,
    UNI_MOUSE_QUADRATURE_PORT_1,
    UNI_MOUSE_QUADRATURE_PORT_MAX,
};

// Each "port" has two quadrature: Horizonal and Vertical movements.
enum {
    UNI_MOUSE_QUADRATURE_ENCODER_H,
    UNI_MOUSE_QUADRATURE_ENCODER_V,
    UNI_MOUSE_QUADRATURE_ENCODER_MAX,
};

// And each encoder requries two GPIOs.
struct uni_mouse_quadrature_encoder_gpios {
    int a;  // GPIO A
    int b;  // GPIO B
};

/*
 * cpu_id indicates in which CPU the quadrature task runs.
 * x1,x2: GPIOs for horizontal movement
 * y1,y2: GPIOs for vertical movement
 */
void uni_mouse_quadrature_init(int cpu_id);
void uni_mouse_quadrature_setup_port(int port_idx,
                                     struct uni_mouse_quadrature_encoder_gpios h,
                                     struct uni_mouse_quadrature_encoder_gpios v);
void uni_mouse_quadrature_update(int port_idx, int32_t dx, int32_t dy);
void uni_mouse_quadrature_start(int port_idx);
void uni_mouse_quadrature_pause(int port_idx);
void uni_mouse_quadrature_deinit(void);

void uni_mouse_quadrature_set_scale_factor(float scale);
float uni_mouse_quadrature_get_scale_factor(void);

#endif  // UNI_MOUSE_QUADRATURE_H
