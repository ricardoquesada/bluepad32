// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include <esp_system.h>

void uni_system_reboot(void) {
    esp_restart();
}