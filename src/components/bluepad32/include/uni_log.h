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

#include "uni_config.h"

void uni_log(const char* fmt, ...);
void uni_logv(const char* fmt, va_list args);

#define loge(fmt, ...)                   \
    do {                                 \
        if (UNI_LOG_ERROR)               \
            uni_log(fmt, ##__VA_ARGS__); \
    } while (0)

#define logi(fmt, ...)                   \
    do {                                 \
        if (UNI_LOG_INFO)                \
            uni_log(fmt, ##__VA_ARGS__); \
    } while (0)

#define logd(fmt, ...)                   \
    do {                                 \
        if (UNI_LOG_DEBUG)               \
            uni_log(fmt, ##__VA_ARGS__); \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif  // UNI_LOG_H
