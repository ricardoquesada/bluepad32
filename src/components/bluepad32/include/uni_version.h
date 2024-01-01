// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_VERSION_H
#define UNI_VERSION_H

#include "sdkconfig.h"

#define UNI_VERSION "3.99.0"

#ifndef UNI_BUILD
#ifdef PROJECT_VER
#define UNI_BUILD PROJECT_VER
#else
#define UNI_BUILD "<unk>"
#endif  // PROJECT_VER
#endif  // UNI_BUILD

#endif  // UNI_VERSION_H
