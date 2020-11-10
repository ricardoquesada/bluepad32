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

#include "uni_debug.h"
#include "uni_platform_c64.h"
#include "uni_platform_pc_debug.h"

void uni_platform_init(int argc, const char** argv) {
  // Only one for the moment. Each vendor must create its own.

#ifdef UNI_PLATFORM_C64
  g_platform = uni_platform_c64_create();
#elif defined(UNI_PLATFORM_PC_DEBUG)
  g_platform = uni_platform_pc_debug_create();
#else
#error "Platform not defined"
#endif

  g_platform->init(argc, argv);

  logi("Platform: %s\n", g_platform->name);
}