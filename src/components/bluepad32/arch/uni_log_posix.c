// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_log.h"

void uni_logv(const char* fmt, va_list args) {
    vfprintf(stdout, fmt, args);
}