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

#ifndef UNI_LOG_H
#define UNI_LOG_H

#include <stdio.h>

#include "uni_config.h"

#define loge(fmt, ...)                                      \
  do {                                                      \
    if (UNI_LOG_ERROR) fprintf(stderr, fmt, ##__VA_ARGS__); \
  } while (0)
#define logi(fmt, ...)                                     \
  do {                                                     \
    if (UNI_LOG_INFO) fprintf(stderr, fmt, ##__VA_ARGS__); \
  } while (0)
#define logd(fmt, ...)                                      \
  do {                                                      \
    if (UNI_LOG_DEBUG) fprintf(stderr, fmt, ##__VA_ARGS__); \
  } while (0)

#endif  // UNI_LOG_H