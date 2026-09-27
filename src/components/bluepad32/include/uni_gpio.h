// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_GPIO_H
#define UNI_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Guard ESP-IDF GPIO types and helpers so this header can be safely included
// in cross-platform translation units and host unit tests.
#ifdef ESP_PLATFORM
#include <driver/gpio.h>

// The inconsistency between read/write with uint8_t / uint16_t is to be
// compatible with NINA protocol which has this inconsistency.
int uni_gpio_analog_write(gpio_num_t pin, uint8_t value);
uint16_t uni_gpio_analog_read(gpio_num_t pin);

void uni_gpio_register_cmds(void);

// Safe version of gpio_set_level.
esp_err_t uni_gpio_set_level(gpio_num_t gpio, int value);
#endif  // ESP_PLATFORM

#ifdef __cplusplus
}
#endif

#endif  // UNI_GPIO_H
