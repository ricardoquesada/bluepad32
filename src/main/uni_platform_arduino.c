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

#include "uni_platform_arduino.h"

// Only compile this file when Arduino is selected.
// Requires the Arduino.h file which is only avaiable when the Arduino component
// is included in the ESP-IDF as a component.
// #ifndef UNI_PLATFORM_ARDUINO
// #define UNI_PLATFORM_ARDUINO
// #endif

#ifndef UNI_PLATFORM_ARDUINO
struct uni_platform* uni_platform_arduino_create(void) {
  return NULL;
}
#else  // UNI_PLATFORM_ARDUINO

// Sanity check. It seems that when Arduino is added as a component, this
// define is present.
#include <sdkconfig.h>
#if !defined(CONFIG_ENABLE_ARDUINO_DEPENDS)
#error \
    "Arduino not enabled as a component. Check: https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html"
#endif

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <math.h>

#include "uni_bluetooth.h"
#include "uni_config.h"
#include "uni_debug.h"
#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_main_esp32.h"
#include "uni_platform.h"
#include "uni_version.h"

//
// Globals
//

// Arbitrary max number of gamepads that can be connected at the same time
#define ARDUINO_MAX_GAMEPADS 4

// Arduino device "instance"
typedef struct arduino_instance_s {
  // Gamepad index, from 0 to ARDUINO_MAX_GAMEPADS
  // -1 means gamepad was not assigned yet.
  int8_t gamepad_idx;
} arduino_instance_t;
_Static_assert(sizeof(arduino_instance_t) < HID_DEVICE_MAX_PLATFORM_DATA,
               "Arduino intance too big");

static TaskHandle_t loopTaskHandle = NULL;
static arduino_gamepad_t _gamepads[ARDUINO_MAX_GAMEPADS];
static SemaphoreHandle_t _gamepad_mutex = NULL;
static volatile uni_gamepad_seat_t _gamepad_seats;

static arduino_instance_t* get_arduino_instance(uni_hid_device_t* d);
static uint8_t predicate_arduino_index(uni_hid_device_t* d, void* data);

//
// CPU 0 - Bluepad32 process
//

// BTStack / Bluepad32 are not thread safe.
// Be extra careful when calling code that runs on the other CPU
//
//

//
// Platform Overrides
//

static void setup(void) {}
static void loop(void) {}

void loopTask(void* pvParameters) {
  setup();
  for (;;) {
    loop();
  }
}

static void arduino_init(int argc, const char** argv) {
  UNUSED(argc);
  UNUSED(argv);
}

static void arduino_on_init_complete(void) {
  initArduino();

  _gamepad_mutex = xSemaphoreCreateMutex();
  assert(_gamepad_mutex != NULL);

  xTaskCreateUniversal(loopTask, "loopTask", CONFIG_ARDUINO_LOOP_STACK_SIZE,
                       NULL, 1, &loopTaskHandle, CONFIG_ARDUINO_RUNNING_CORE);
}

static void arduino_on_device_connected(uni_hid_device_t* d) {
  arduino_instance_t* ins = get_arduino_instance(d);
  memset(ins, 0, sizeof(*ins));
  ins->gamepad_idx = -1;
}

static void arduino_on_device_disconnected(uni_hid_device_t* d) {
  arduino_instance_t* ins = get_arduino_instance(d);
  // Only process it if the gamepad has been assigned before
  if (ins->gamepad_idx != -1) {
    if (ins->gamepad_idx < 0 || ins->gamepad_idx >= ARDUINO_MAX_GAMEPADS) {
      loge("Arduino: unexpected gamepad idx, got: %d, want: [0-%d]\n",
           ins->gamepad_idx, ARDUINO_MAX_GAMEPADS);
      return;
    }
    _gamepad_seats &= ~(1 << ins->gamepad_idx);

    ins->gamepad_idx = -1;
  }
}

static int arduino_on_device_ready(uni_hid_device_t* d) {
  if (_gamepad_seats ==
      (GAMEPAD_SEAT_A | GAMEPAD_SEAT_B | GAMEPAD_SEAT_C | GAMEPAD_SEAT_D)) {
    // No more available seats, reject connection
    logi("Arduino: More available seats\n");
    return -1;
  }

  arduino_instance_t* ins = get_arduino_instance(d);
  if (ins->gamepad_idx != -1) {
    loge("Arduino: unexpected value for on_device_ready; got: %d, want: -1\n",
         ins->gamepad_idx);
    return -1;
  }

  // Find first available gamepad
  for (int i = 0; i < ARDUINO_MAX_GAMEPADS; i++) {
    if ((_gamepad_seats & (1 << i)) == 0) {
      ins->gamepad_idx = i;
      _gamepad_seats |= (1 << i);
      break;
    }
  }

  if (d->report_parser.set_player_leds != NULL) {
    arduino_instance_t* ins = get_arduino_instance(d);
    d->report_parser.set_player_leds(d, (1 << ins->gamepad_idx));
  }
  return 0;
}

static void arduino_on_gamepad_data(uni_hid_device_t* d, uni_gamepad_t* gp) {
  arduino_instance_t* ins = get_arduino_instance(d);
  if (ins->gamepad_idx < 0 || ins->gamepad_idx >= ARDUINO_MAX_GAMEPADS) {
    loge("Arduino: unexpected gamepad idx, got: %d, want: [0-%d]\n",
         ins->gamepad_idx, ARDUINO_MAX_GAMEPADS);
    return;
  }

  // Populate gamepad data on shared struct.
  xSemaphoreTake(_gamepad_mutex, portMAX_DELAY);
  _gamepads[ins->gamepad_idx] = (arduino_gamepad_t){
      .idx = ins->gamepad_idx,
      .type = d->controller_type,
      .data = *gp,
  };
  xSemaphoreGive(_gamepad_mutex);
}

static void arduino_on_device_oob_event(uni_hid_device_t* d,
                                        uni_platform_oob_event_t event) {
  if (event != UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON) return;

  // TODO: Do something ?
}

static int32_t arduino_get_property(uni_platform_property_t key) {
  // FIXME: support well-known uni_platform_property_t keys
  return 0;
}

//
// CPU 1 - Application (Arduino) process
//
arduino_gamepad_t arduino_get_gamepad_data(int idx) { return _gamepads[idx]; }

void arduino_set_player_leds(int idx, uint8_t leds) {
  uni_hid_device_t* d = uni_hid_device_get_instance_with_predicate(
      predicate_arduino_index, (void*)idx);
  if (d == NULL) {
    loge("Arduino: device cannot be found while processing pending requests\n");
    return;
  }
}

void arduino_set_lightbar_color(int idx, uint8_t r, uint8_t g, uint8_t b) {}

void arduino_set_rumble(int idx, uint8_t force, uint8_t duration) {}

//
// Helpers
//
static arduino_instance_t* get_arduino_instance(uni_hid_device_t* d) {
  return (arduino_instance_t*)&d->platform_data[0];
}

static uint8_t predicate_arduino_index(uni_hid_device_t* d, void* data) {
  int wanted_idx = (int)data;
  arduino_instance_t* ins = get_arduino_instance(d);
  if (ins->gamepad_idx != wanted_idx) return 0;
  return 1;
}

//
// Entry Point
//
struct uni_platform* uni_platform_arduino_create(void) {
  static struct uni_platform plat;

  plat.name = "Arduino";
  plat.init = arduino_init;
  plat.on_init_complete = arduino_on_init_complete;
  plat.on_device_connected = arduino_on_device_connected;
  plat.on_device_disconnected = arduino_on_device_disconnected;
  plat.on_device_ready = arduino_on_device_ready;
  plat.on_device_oob_event = arduino_on_device_oob_event;
  plat.on_gamepad_data = arduino_on_gamepad_data;
  plat.get_property = arduino_get_property;

  return &plat;
}

#endif  // UNI_PLATFORM_ARDUINO
