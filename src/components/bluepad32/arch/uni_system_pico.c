// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_system.h"

#include <hardware/watchdog.h>

void uni_system_reboot(void) {
    watchdog_reboot(0 /* pc */, 0 /* sp */, 0 /* delay ms */);
}