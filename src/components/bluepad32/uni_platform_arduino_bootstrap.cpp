/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2021 Ricardo Quesada

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

#include "uni_platform_arduino_bootstrap.h"

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
