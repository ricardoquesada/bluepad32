/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2019 Ricardo Quesada

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
****************************************************************************/

#ifndef UNI_CONFIG_H
#define UNI_CONFIG_H

#include "sdkconfig.h"

#if defined(CONFIG_TARGET_LIBUSB)
// Linux + libusb
#define UNI_ENABLE_BREDR 1
#define UNI_ENABLE_BLE 1
#elif defined(CONFIG_IDF_TARGET_ESP32)
// ESP32
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

// How verbose the logging should be.
// It is safe to leave UNI_LOG_DEBUG disabled (unless you are a developer).
#define UNI_LOG_ERROR 1
#define UNI_LOG_INFO 1
#define UNI_LOG_DEBUG 0

#endif  // UNI_CONFIG_H
