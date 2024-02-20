// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_log.h"

#include <stdarg.h>

__attribute__((weak)) void uni_log(const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    uni_logv(fmt, args);
    va_end(args);
}

__attribute__((weak)) void uni_logv(const char* fmt, va_list args) {
    vfprintf(stdout, fmt, args);
}
