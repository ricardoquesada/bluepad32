/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2020 Ricardo Quesada

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

#include "uni_platform.h"

#include "sdkconfig.h"
#include "uni_log.h"
#include "uni_platform_arduino.h"
#include "uni_platform_custom.h"
#include "uni_platform_mightymiggy.h"
#include "uni_platform_nina.h"
#include "uni_platform_pc_debug.h"

#ifdef CONFIG_BLUEPAD32_PLATFORM_UNIJOYSTICLE
#include "uni_platform_unijoysticle.h"
#endif

// Platform "object"
static struct uni_platform* _platform;

void uni_platform_init(int argc, const char** argv) {
    // These CONFIG_BLUEPAD32_PLATFORM_ defines are defined in the Makefile
    // and Kconfig files.
    // Premade platforms are available as part of this library. Vendors/users
    // may create a "custom" platform by providing an implementation of
    // uni_platform_custom_create() within a project file (for instance
    // uni_platform_custom.c) and select the CONFIG_BLUEPAD32_PLATFORM_CUSTOM
    // from the Makefile or the Kconfig file

#ifdef CONFIG_BLUEPAD32_PLATFORM_UNIJOYSTICLE
    _platform = uni_platform_unijoysticle_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_PC_DEBUG)
    _platform = uni_platform_pc_debug_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_AIRLIFT)
    _platform = uni_platform_airlift_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_MIGHTYMIGGY)
    _platform = uni_platform_mightymiggy_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_NINA)
    _platform = uni_platform_nina_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_ARDUINO)
    _platform = uni_platform_arduino_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_CUSTOM)
    _platform = uni_platform_custom_create();
#else
#error "Platform not defined. Set PLATFORM environment variable"
#endif

    logi("Platform: %s\n", _platform->name);
    _platform->init(argc, argv);
}

struct uni_platform* uni_get_platform(void) {
    return _platform;
}
