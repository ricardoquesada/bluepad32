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
    // "struct uni_platform" and calling "uni_platform_set_custom".

#ifdef CONFIG_BLUEPAD32_PLATFORM_UNIJOYSTICLE
    _platform = uni_platform_unijoysticle_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_AIRLIFT)
    _platform = uni_platform_airlift_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_MIGHTYMIGGY)
    _platform = uni_platform_mightymiggy_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_NINA)
    _platform = uni_platform_nina_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_ARDUINO)
    _platform = uni_platform_arduino_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_CUSTOM)
    if (!_platform) {
        while (1)
            loge("Error: call uni_platform_set_custom() before calling uni_main\n");
    }
#else
#error "Platform not defined. Set CONFIG_BLUEPAD32_PLATFORM"
#endif

    logi("Platform: %s\n", _platform->name);
    _platform->init(argc, argv);
}

struct uni_platform* uni_get_platform(void) {
    return _platform;
}

void uni_platform_set_custom(struct uni_platform* platform) {
#ifdef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
    _platform = platform;
#else
    ARG_UNUSED(platform);
    while (1)
        loge("Error: uni_platform_set_custom SHOULD only be called on 'custom' platform\n");
#endif
}
