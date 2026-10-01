// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_MOUSE_QUADRATURE_H
#define UNI_MOUSE_QUADRATURE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file uni_mouse_quadrature.h
 * @brief Quadrature emulation driver for retro mice.
 *
 * Simulates Amiga/Atari/C64 quadrature mouse signals on ESP32 GPIO pins using hardware timers.
 */

/**
 * @brief Port enumeration for mice.
 * In this case, up to two mice are supported.
 * Required, at least, for Lemmings. In Amiga, it has a 2-player mode that uses two mice.
 */
enum {
    UNI_MOUSE_QUADRATURE_PORT_0,
    UNI_MOUSE_QUADRATURE_PORT_1,
    UNI_MOUSE_QUADRATURE_PORT_MAX,
};

/**
 * @brief Quadrature encoders for each port: Horizontal and Vertical movements.
 */
enum {
    UNI_MOUSE_QUADRATURE_ENCODER_H,
    UNI_MOUSE_QUADRATURE_ENCODER_V,
    UNI_MOUSE_QUADRATURE_ENCODER_MAX,
};

/**
 * @brief Each encoder requires two GPIOs.
 */
struct uni_mouse_quadrature_encoder_gpios {
    int a; /**< GPIO A */
    int b; /**< GPIO B */
};

/**
 * @brief Initializes the quadrature task on a specific CPU.
 * @param cpu_id The ID of the CPU where the quadrature task will run.
 */
void uni_mouse_quadrature_init(int cpu_id);

/**
 * @brief Configures the GPIOs for a given quadrature port.
 * @param port_idx The port index (e.g. UNI_MOUSE_QUADRATURE_PORT_0).
 * @param h GPIO structure for horizontal movement.
 * @param v GPIO structure for vertical movement.
 */
void uni_mouse_quadrature_setup_port(int port_idx,
                                     struct uni_mouse_quadrature_encoder_gpios h,
                                     struct uni_mouse_quadrature_encoder_gpios v);

/**
 * @brief Updates the accumulated delta for horizontal and vertical movements.
 * @param port_idx The port index.
 * @param dx Horizontal delta.
 * @param dy Vertical delta.
 */
void uni_mouse_quadrature_update(int port_idx, int32_t dx, int32_t dy);

/**
 * @brief Starts processing quadrature movements for the given port.
 * @param port_idx The port index.
 */
void uni_mouse_quadrature_start(int port_idx);

/**
 * @brief Pauses processing quadrature movements for the given port.
 * @param port_idx The port index.
 */
void uni_mouse_quadrature_pause(int port_idx);

/**
 * @brief Deinitializes the quadrature subsystem and frees resources.
 */
void uni_mouse_quadrature_deinit(void);

/**
 * @brief Sets the global scaling factor for mouse quadrature movements.
 * @param scale The scaling multiplier to apply to deltas.
 */
void uni_mouse_quadrature_set_scale_factor(float scale);

/**
 * @brief Gets the global scaling factor for mouse quadrature movements.
 * @return The current scaling multiplier.
 */
float uni_mouse_quadrature_get_scale_factor(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_MOUSE_QUADRATURE_H
