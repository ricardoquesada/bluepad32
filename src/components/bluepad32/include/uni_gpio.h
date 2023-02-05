
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

#include <stdint.h>

#include <driver/gpio.h>

// The inconsistency between read/write with uint8_t / uint16_t is to be
// compatible with NINA protocol which has this inconsistency.
int uni_gpio_analog_write(gpio_num_t pin, uint8_t value);
uint16_t uni_gpio_analog_read(gpio_num_t pin);

void uni_gpio_register_cmds(void);

// Safe version of gpio_set_level.
esp_err_t uni_gpio_set_level(gpio_num_t gpio, int value);

#endif  // UNI_GPIO_H
