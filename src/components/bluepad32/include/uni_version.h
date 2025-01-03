// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_VERSION_H
#define UNI_VERSION_H

// String version
#define UNI_VERSION_STRING "4.2.0"

// Number version, in case a 3rd party needs to check it
#define UNI_VERSION_MAJOR 4
#define UNI_VERSION_MINOR 2
#define UNI_VERSION_PATCH 0

extern const char* uni_version;

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

#endif  // UNI_VERSION_H
