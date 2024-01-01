// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_VIRTUAL_DEVICE_H
#define UNI_VIRTUAL_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

void uni_virtual_device_init(void);
void uni_virtual_device_set_enabled(bool enabled);
bool uni_virtual_device_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_VIRTUAL_DEVICE_H