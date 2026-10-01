// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BTSTACK_VERSION_COMPAT_H
#define UNI_BTSTACK_VERSION_COMPAT_H

// btstack_version.h was included in v1.6.2
#if defined __has_include
#if __has_include(<btstack_version.h>)
#include <btstack_version.h>
#else
#define BTSTACK_VERSION_MAJOR 1
#define BTSTACK_VERSION_MINOR 6
#define BTSTACK_VERSION_PATCH 1
#define BTSTACK_VERSION_STRING "1.6.1"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BTSTACK_VERSION_AT_LEAST
#define BTSTACK_VERSION_AT_LEAST(major, minor, patch)                                                              \
    ((BTSTACK_VERSION_MAJOR > (major)) || (BTSTACK_VERSION_MAJOR == (major) && BTSTACK_VERSION_MINOR > (minor)) || \
     (BTSTACK_VERSION_MAJOR == (major) && BTSTACK_VERSION_MINOR == (minor) && BTSTACK_VERSION_PATCH >= (patch)))
#endif

#ifdef __cplusplus
}
#endif

#endif  // UNI_BTSTACK_VERSION_COMPAT_H
