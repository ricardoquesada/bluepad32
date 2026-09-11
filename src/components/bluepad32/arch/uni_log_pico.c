// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_log.h"

#include <stdarg.h>
#include <stdio.h>

#include "uni_config.h"

void uni_logv(const char* format, va_list args) {
#if defined(CONFIG_OGXM_DEBUG)
    fputs("OGXM: ", stdout);
#endif
    vfprintf(stdout, format, args);
    fflush(stdout);
}
