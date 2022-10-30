
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

#ifndef UNI_GPIO_H
#define UNI_GPIO_H

#include <stdbool.h>
#include <stdint.h>

void uni_gpio_set_level(int pin, bool value);
bool uni_gpio_get_level(int pin);
void uni_gpio_set_analog(int pin, uint8_t value);
uint8_t uni_gpio_get_analog(int pin);

void uni_gpio_register_cmds(void);

#endif  // UNI_GPIO_H
