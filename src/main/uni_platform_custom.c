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

#include "sdkconfig.h"
#include "uni_log.h"
#include "uni_platform.h"

extern struct uni_platform* uni_platform_custom_1_create(void);


void uni_platform_custom_create() {
    // These CONFIG_BLUEPAD32_PLATFORM_ defines are defined in the Makefile
    // and Kconfig files.

#ifdef CONFIG_BLUEPAD32_PLATFORM_CUSTOM_1
    uni_platform_custom_1_create();
#else
#error "Custom Platform not defined."
#endif

}
