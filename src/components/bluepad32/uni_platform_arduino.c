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

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "sdkconfig.h"
#include "uni_bluetooth.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_debug.h"
#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_platform.h"
#include "uni_platform_arduino_bootstrap.h"

//
// Globals
//
#define MAX_PENDING_REQUESTS 16

// Arduino device "instance"
typedef struct arduino_instance_s {
    // Gamepad index, from 0 to CONFIG_BLUEPAD32_MAX_DEVICES
    // -1 means gamepad was not assigned yet.
    // It is used to map "_gamepads" to the uni_hid_device.
    int8_t gamepad_idx;
} arduino_instance_t;
_Static_assert(sizeof(arduino_instance_t) < HID_DEVICE_MAX_PLATFORM_DATA, "Arduino intance too big");

static QueueHandle_t _pending_queue = NULL;
static SemaphoreHandle_t _gamepad_mutex = NULL;
static arduino_gamepad_t _gamepads[CONFIG_BLUEPAD32_MAX_DEVICES];
static int _used_gamepads = 0;

static arduino_instance_t* get_arduino_instance(uni_hid_device_t* d);
static uint8_t predicate_arduino_index(uni_hid_device_t* d, void* data);

//
// Shared by CPU 0 (bluetooth) / CPU1 (Arduino)
//
// BTStack / Bluepad32 are not thread safe.
// This code is the bridge between CPU1 and CPU0.
//

typedef enum {
    PENDING_REQUEST_CMD_NONE = 0,
    PENDING_REQUEST_CMD_LIGHTBAR_COLOR = 1,
    PENDING_REQUEST_CMD_PLAYER_LEDS = 2,
    PENDING_REQUEST_CMD_RUMBLE = 3,
} pending_request_cmd_t;

typedef struct {
    // Gamepad index: from 0 to CONFIG_BLUEPAD32_MAX_DEVICES-1
    uint8_t gamepad_idx;
    pending_request_cmd_t cmd;
    // Must have enough space to hold at least 3 arguments: red, green, blue
    uint8_t args[3];
} pending_request_t;

//
// CPU 0 - Bluepad32 process
//

// BTStack / Bluepad32 are not thread safe.
// Be extra careful when calling code that runs on the other CPU

static void arduino_init(int argc, const char** argv) {
    memset(&_gamepads, 0, sizeof(_gamepads));
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        _gamepads[i].idx = UNI_ARDUINO_GAMEPAD_INVALID;
    }
}

static uint8_t predicate_arduino_index(uni_hid_device_t* d, void* data) {
    int wanted_idx = (int)data;
    arduino_instance_t* ins = get_arduino_instance(d);
    if (ins->gamepad_idx != wanted_idx)
        return 0;
    return 1;
}

static void process_pending_requests(void) {
    pending_request_t request;

    while (xQueueReceive(_pending_queue, &request, (TickType_t)0) == pdTRUE) {
        int idx = request.gamepad_idx;
        uni_hid_device_t* d = uni_hid_device_get_instance_with_predicate(predicate_arduino_index, (void*)idx);
        if (d == NULL) {
            loge("Arduino: device cannot be found while processing pending request\n");
            return;
        }
        switch (request.cmd) {
            case PENDING_REQUEST_CMD_LIGHTBAR_COLOR:
                if (d->report_parser.set_lightbar_color != NULL)
                    d->report_parser.set_lightbar_color(d, request.args[0], request.args[1], request.args[2]);
                break;
            case PENDING_REQUEST_CMD_PLAYER_LEDS:
                if (d->report_parser.set_player_leds != NULL)
                    d->report_parser.set_player_leds(d, request.args[0]);
                break;

            case PENDING_REQUEST_CMD_RUMBLE:
                if (d->report_parser.set_rumble != NULL)
                    d->report_parser.set_rumble(d, request.args[0], request.args[1]);
                break;

            default:
                loge("Arduino: Invalid pending command: %d\n", request.cmd);
        }
    }
}

//
// Platform Overrides
//
static void arduino_on_init_complete(void) {
    _gamepad_mutex = xSemaphoreCreateMutex();
    assert(_gamepad_mutex != NULL);

    _pending_queue = xQueueCreate(MAX_PENDING_REQUESTS, sizeof(pending_request_t));
    assert(_pending_queue != NULL);

    arduino_bootstrap();
}

static void arduino_on_device_connected(uni_hid_device_t* d) {
    arduino_instance_t* ins = get_arduino_instance(d);
    memset(ins, 0, sizeof(*ins));
    ins->gamepad_idx = UNI_ARDUINO_GAMEPAD_INVALID;
}

static void arduino_on_device_disconnected(uni_hid_device_t* d) {
    arduino_instance_t* ins = get_arduino_instance(d);
    // Only process it if the gamepad has been assigned before
    if (ins->gamepad_idx != UNI_ARDUINO_GAMEPAD_INVALID) {
        if (ins->gamepad_idx < 0 || ins->gamepad_idx >= CONFIG_BLUEPAD32_MAX_DEVICES) {
            loge("Arduino: unexpected gamepad idx, got: %d, want: [0-%d]\n", ins->gamepad_idx,
                 CONFIG_BLUEPAD32_MAX_DEVICES);
            return;
        }
        _used_gamepads--;

        memset(&_gamepads[ins->gamepad_idx], 0, sizeof(_gamepads[0]));
        _gamepads[ins->gamepad_idx].idx = UNI_ARDUINO_GAMEPAD_INVALID;

        ins->gamepad_idx = UNI_ARDUINO_GAMEPAD_INVALID;
    }
}

static int arduino_on_device_ready(uni_hid_device_t* d) {
    if (_used_gamepads == CONFIG_BLUEPAD32_MAX_DEVICES) {
        // No more available seats, reject connection
        logi("Arduino: More available seats\n");
        return -1;
    }

    arduino_instance_t* ins = get_arduino_instance(d);
    if (ins->gamepad_idx != UNI_ARDUINO_GAMEPAD_INVALID) {
        loge("Arduino: unexpected value for on_device_ready; got: %d, want: -1\n", ins->gamepad_idx);
        return -1;
    }

    // Find first available gamepad
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        if (_gamepads[i].idx == UNI_ARDUINO_GAMEPAD_INVALID) {
            _gamepads[i].idx = i;

            memcpy(_gamepads[i].properties.btaddr, d->conn.btaddr, sizeof(_gamepads[0].properties.btaddr));
            _gamepads[i].properties.type = d->controller_type;
            _gamepads[i].properties.subtype = d->controller_type;
            _gamepads[i].properties.vendor_id = d->vendor_id;
            _gamepads[i].properties.product_id = d->product_id;
            _gamepads[i].properties.flags =
                (d->report_parser.set_player_leds ? ARDUINO_PROPERTY_FLAG_PLAYER_LEDS : 0) |
                (d->report_parser.set_rumble ? ARDUINO_PROPERTY_FLAG_RUMBLE : 0) |
                (d->report_parser.set_lightbar_color ? ARDUINO_PROPERTY_FLAG_PLAYER_LIGHTBAR : 0);

            ins->gamepad_idx = i;
            _used_gamepads++;
            break;
        }
    }

    logd("Arduino: assigned gampead idx is: %d\n", ins->gamepad_idx);

    if (d->report_parser.set_player_leds != NULL) {
        arduino_instance_t* ins = get_arduino_instance(d);
        d->report_parser.set_player_leds(d, BIT(ins->gamepad_idx));
    }
    return 0;
}

static void arduino_on_gamepad_data(uni_hid_device_t* d, uni_gamepad_t* gp) {
    process_pending_requests();

    arduino_instance_t* ins = get_arduino_instance(d);
    if (ins->gamepad_idx < 0 || ins->gamepad_idx >= CONFIG_BLUEPAD32_MAX_DEVICES) {
        loge("Arduino: unexpected gamepad idx, got: %d, want: [0-%d]\n", ins->gamepad_idx,
             CONFIG_BLUEPAD32_MAX_DEVICES);
        return;
    }

    // Populate gamepad data on shared struct.
    xSemaphoreTake(_gamepad_mutex, portMAX_DELAY);
    _gamepads[ins->gamepad_idx].data = *gp;
    xSemaphoreGive(_gamepad_mutex);
}

static void arduino_on_device_oob_event(uni_platform_oob_event_t event, void* data) {
    ARG_UNUSED(event);
    ARG_UNUSED(data);
    // TODO: Do something ?
}

static int32_t arduino_get_property(uni_platform_property_t key) {
    // FIXME: support well-known uni_platform_property_t keys
    return 0;
}

//
// CPU 1 - Application (Arduino) process
//
int arduino_get_gamepad_data(int idx, arduino_gamepad_data_t* out_data) {
    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
        return UNI_ARDUINO_ERROR;
    if (_gamepads[idx].idx == UNI_ARDUINO_GAMEPAD_INVALID)
        return UNI_ARDUINO_ERROR;

    xSemaphoreTake(_gamepad_mutex, portMAX_DELAY);
    *out_data = _gamepads[idx].data;
    xSemaphoreGive(_gamepad_mutex);

    return UNI_ARDUINO_OK;
}

int arduino_get_gamepad_properties(int idx, arduino_gamepad_properties_t* out_properties) {
    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
        return UNI_ARDUINO_ERROR;
    if (_gamepads[idx].idx == UNI_ARDUINO_GAMEPAD_INVALID)
        return UNI_ARDUINO_ERROR;

    xSemaphoreTake(_gamepad_mutex, portMAX_DELAY);
    *out_properties = _gamepads[idx].properties;
    xSemaphoreGive(_gamepad_mutex);

    return UNI_ARDUINO_OK;
}

int arduino_set_player_leds(int idx, uint8_t leds) {
    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
        return UNI_ARDUINO_ERROR;
    if (_gamepads[idx].idx == UNI_ARDUINO_GAMEPAD_INVALID)
        return UNI_ARDUINO_ERROR;

    pending_request_t request = (pending_request_t){
        .gamepad_idx = idx,
        .cmd = PENDING_REQUEST_CMD_PLAYER_LEDS,
        .args[0] = leds,
    };
    xQueueSendToBack(_pending_queue, &request, (TickType_t)0);

    return UNI_ARDUINO_OK;
}

int arduino_set_lightbar_color(int idx, uint8_t r, uint8_t g, uint8_t b) {
    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
        return UNI_ARDUINO_ERROR;
    if (_gamepads[idx].idx == UNI_ARDUINO_GAMEPAD_INVALID)
        return UNI_ARDUINO_ERROR;

    pending_request_t request = (pending_request_t){
        .gamepad_idx = idx,
        .cmd = PENDING_REQUEST_CMD_LIGHTBAR_COLOR,
        .args[0] = r,
        .args[1] = g,
        .args[2] = b,
    };
    xQueueSendToBack(_pending_queue, &request, (TickType_t)0);

    return UNI_ARDUINO_OK;
}

int arduino_set_rumble(int idx, uint8_t force, uint8_t duration) {
    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
        return UNI_ARDUINO_ERROR;
    if (_gamepads[idx].idx == UNI_ARDUINO_GAMEPAD_INVALID)
        return UNI_ARDUINO_ERROR;

    pending_request_t request = (pending_request_t){
        .gamepad_idx = idx,
        .cmd = PENDING_REQUEST_CMD_RUMBLE,
        .args[0] = force,
        .args[1] = duration,
    };
    xQueueSendToBack(_pending_queue, &request, (TickType_t)0);

    return UNI_ARDUINO_OK;
}

int arduino_forget_bluetooth_keys(void) {
    uni_bluetooth_del_keys_safe();
    return UNI_ARDUINO_OK;
}

//
// Helpers
//
static arduino_instance_t* get_arduino_instance(uni_hid_device_t* d) {
    return (arduino_instance_t*)&d->platform_data[0];
}

//
// Entry Point
//
struct uni_platform* uni_platform_arduino_create(void) {
    static struct uni_platform plat = {
        .name = "Arduino",
        .init = arduino_init,
        .on_init_complete = arduino_on_init_complete,
        .on_device_connected = arduino_on_device_connected,
        .on_device_disconnected = arduino_on_device_disconnected,
        .on_device_ready = arduino_on_device_ready,
        .on_oob_event = arduino_on_device_oob_event,
        .on_gamepad_data = arduino_on_gamepad_data,
        .get_property = arduino_get_property,
    };

    return &plat;
}
