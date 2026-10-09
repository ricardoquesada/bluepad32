// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_CONFIG_H
#define UNI_CONFIG_H

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_TARGET_POSIX) || defined(CONFIG_TARGET_PICO_W) || defined(CONFIG_SOC_BT_CLASSIC_SUPPORTED)
#define UNI_ENABLE_BREDR 1
#endif

#if defined(CONFIG_TARGET_POSIX) || defined(CONFIG_TARGET_PICO_W) || defined(CONFIG_SOC_BLE_SUPPORTED)
#define UNI_ENABLE_BLE 1
#endif

#if !defined(UNI_ENABLE_BREDR) && !defined(UNI_ENABLE_BLE)
#error "Unsupported target platform"
#endif

// For more configurations, please look at the Kconfig file, or just do:
// "idf.py menuconfig" -> "Component config" -> "Bluepad32"

#ifndef CONFIG_BLUEPAD32_BLE_SERVICE_ENABLED
#define CONFIG_BLUEPAD32_BLE_SERVICE_ENABLED 1
#endif

#ifndef CONFIG_BLUEPAD32_BLE_SERVICE_NAME
#define CONFIG_BLUEPAD32_BLE_SERVICE_NAME "Bluepad32"
#endif

#ifndef CONFIG_BLUEPAD32_BLE_SERVICE_PASSWORD
#define CONFIG_BLUEPAD32_BLE_SERVICE_PASSWORD ""
#endif

#ifdef __cplusplus
}
#endif

#endif  // UNI_CONFIG_H
