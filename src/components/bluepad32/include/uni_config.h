// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_CONFIG_H
#define UNI_CONFIG_H

#include "sdkconfig.h"

#if defined(CONFIG_TARGET_POSIX) || defined(CONFIG_TARGET_PICO_W) || defined(CONFIG_IDF_TARGET_ESP32)
// Pico W, original ESP32 and Posix all support both BR/EDR and BLE
#define UNI_ENABLE_BREDR 1
#define UNI_ENABLE_BLE 1
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
// ESP32-S3 / C3
#define UNI_ENABLE_BLE 1
#else
#error "Unsupported target platform"
#endif

// For more configurations, please look at the Kconfig file, or just do:
// "idf.py menuconfig" -> "Component config" -> "Bluepad32"

#endif  // UNI_CONFIG_H
