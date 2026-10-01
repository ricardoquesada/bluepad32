// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_INIT_H
#define UNI_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file uni_init.h
 * @brief Initialization functions for the Bluepad32 core.
 */

/**
 * @brief Initialize Bluepad32.
 *
 * @param argc The argument count.
 * @param argv The argument array (passed to the configured platform).
 * @return 0 on success, or a non-zero error code on failure.
 */
int uni_init(int argc, const char** argv);

#ifdef __cplusplus
}
#endif

#endif  // UNI_INIT_H
