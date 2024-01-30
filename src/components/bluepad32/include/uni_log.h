// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_LOG_H
#define UNI_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdio.h>

#include "sdkconfig.h"
#include "uni_config.h"

void uni_log(const char* fmt, ...);

// Should be overridden by each architecture.
void uni_logv(const char* fmt, va_list args);

/*
 * None = 0
 * Error = 1
 * Info = 2
 * Debug = 3
 */

// If UART output is disabled, then LOG_LEVEL is not enabled
#ifndef CONFIG_BLUEPAD32_LOG_LEVEL
#define CONFIG_BLUEPAD32_LOG_LEVEL 0
#endif  // !CONFIG_BLUEPAD32_LOG_LEVEL

#define loge(fmt, ...)                       \
    do {                                     \
        if (CONFIG_BLUEPAD32_LOG_LEVEL >= 1) \
            uni_log(fmt, ##__VA_ARGS__);     \
    } while (0)

#define logi(fmt, ...)                       \
    do {                                     \
        if (CONFIG_BLUEPAD32_LOG_LEVEL >= 2) \
            uni_log(fmt, ##__VA_ARGS__);     \
    } while (0)

#define logd(fmt, ...)                       \
    do {                                     \
        if (CONFIG_BLUEPAD32_LOG_LEVEL >= 3) \
            uni_log(fmt, ##__VA_ARGS__);     \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif  // UNI_LOG_H
