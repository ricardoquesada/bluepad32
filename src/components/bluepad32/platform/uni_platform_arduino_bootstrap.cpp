// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "platform/uni_platform_arduino_bootstrap.h"

#include "sdkconfig.h"
#ifndef CONFIG_BLUEPAD32_PLATFORM_ARDUINO
extern "C" {
void arduino_bootstrap() {}
}
#else  // UNI_PLATFORM_ARDUINO
// Sanity check. It seems that when Arduino is added as a component, this
// define is present.
#include "sdkconfig.h"
#if !defined(CONFIG_ENABLE_ARDUINO_DEPENDS)
#error \
    "Arduino not enabled as a component. Check: https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html"
#endif

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static TaskHandle_t _arduino_task = NULL;

extern "C" {
static void arduino_task(void* params) {
    setup();
    for (;;) {
        loop();
    }
}

void arduino_bootstrap() {
    initArduino();

    xTaskCreateUniversal(arduino_task, "ArduinoTask", CONFIG_ARDUINO_LOOP_STACK_SIZE, NULL, 1, &_arduino_task,
                         CONFIG_ARDUINO_RUNNING_CORE);
    assert(_arduino_task != NULL);
}
}
#endif  // CONFIG_BLUEPAD32_PLATFORM_ARDUINO
