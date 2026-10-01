// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_VERSION_H
#define UNI_VERSION_H

/**
 * @file uni_version.h
 * @brief Bluepad32 version definitions.
 */

#include "uni_btstack_version_compat.h"

#if defined(BTSTACK_VERSION_MAJOR) && !BTSTACK_VERSION_AT_LEAST(1, 6, 2)
#error "Bluepad32 requires BTstack >= 1.6.2. Please run: git submodule update --init --recursive"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief String version of Bluepad32 */
#define UNI_VERSION_STRING "5.0.0"

// Number version, in case a 3rd party needs to check it
/** @brief Major version number */
#define UNI_VERSION_MAJOR 5
/** @brief Minor version number */
#define UNI_VERSION_MINOR 0
/** @brief Patch version number */
#define UNI_VERSION_PATCH 0

/** @brief Global version string pointer */
extern const char* uni_version;

#ifdef __cplusplus
}
#endif

#endif  // UNI_VERSION_H
