/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2022 Ricardo Quesada

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

#ifndef UNI_BALANCE_BOARD_H
#define UNI_BALANCE_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    uint16_t tr;      // Top right
    uint16_t br;      // Bottom right
    uint16_t tl;      // Top left
    uint16_t bl;      // Bottom left
    int temperature;  // Temperature
} uni_balance_board_t;

#ifdef __cplusplus
}
#endif

#endif  // UNI_BALANCE_BOARD_H
