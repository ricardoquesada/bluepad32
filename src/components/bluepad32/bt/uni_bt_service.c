// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2


#include "bt/uni_bt_service.h"

static bool service_enabled;

void uni_bt_service_init(void) {

}

void uni_bt_service_set_enabled(bool enabled) {
    service_enabled = enabled;
}