// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_log.h"

#include <esp_log.h>

void uni_logv(const char* format, va_list args) {
    esp_log_writev(ESP_LOG_INFO, "bp32", format, args);
}
