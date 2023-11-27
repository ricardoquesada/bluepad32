// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_SERVICE_H
#define UNI_BT_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

void uni_bt_service_init(void);
void uni_bt_service_set_enabled(bool enabled);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_SERVICE_H