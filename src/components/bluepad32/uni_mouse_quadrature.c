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

/*
 * Code based on SmallyMouse2 by Simon Inns
 * https://github.com/simoninns/SmallyMouse2
 */
#include "uni_mouse_quadrature.h"

enum direction {
    DIRECTION_NEG,
    DIRECTION_NEUTRAL,
    DIRECTION_POS,
};

// Storing in its own data structure in case in the future we want to support more than one mouse
struct output_pins {
    int x1;
    int x2;
    int y1;
    int y2;
};
struct quadrature_state {
    // Current direction
    enum direction dir_x;
    enum direction dir_y;
    // Current value
    int x;
    int y;
    // Current quadrature phase
    int phase_x;
    int phase_Y;
};

static struct output_pins g_out_pins;
static struct quadrature_state g_state;

void uni_mouse_quadrature_init(int x1, int x2, int y1, int y2) {
    // Assumes these GPIOs have already been configured as output pins.
    g_out_pins = (struct output_pins){
        .x1 = x1,
        .x2 = x2,
        .y1 = y1,
        .y2 = y2,
    };
    // Start the timers
}

void uni_mouse_quadrature_stop() {
    // Stop the timers
}

// Should be called everytime that mouse report is received.
void unit_mouse_quadrature_update(int8_t dx, int8_t dy) {
    g_state.x = dx;
    g_state.y = dy;
}

void isr_timer_x() {
    // TODO: encode current x value
}

void isr_timer_y() {
    // TODO: encode current y value
}