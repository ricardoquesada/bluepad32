// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_log.h"

#include <stdarg.h>

#include "uni_config.h"

#ifdef CONFIG_IDF_TARGET
#include <esp_log.h>
#endif  // CONFIG_IDF_TARGET

void uni_log(const char* format, ...) {
    va_list args;

    va_start(args, format);
    uni_logv(format, args);
    va_end(args);
}

void uni_logv(const char* format, va_list args) {
#ifdef CONFIG_IDF_TARGET
    esp_log_writev(ESP_LOG_WARN, "bp32", format, args);
#else
    vfprintf(stderr, format, args);
#endif  // CONFIG_IDF_TARGET
}
