// Copyright 2021 - 2021, Ricardo Quesada, http://retro.moe
// SPDX-License-Identifier: Apache-2.0 or LGPL-2.1-or-later

#include "ArduinoBluepad32.h"

#include "sdkconfig.h"
#ifndef CONFIG_BLUEPAD32_PLATFORM_ARDUINO
#error "Must only be compiled when using Bluepad32 Arduino platform"
#endif  // !CONFIG_BLUEPAD32_PLATFORM_ARDUINO

#include <Arduino.h>
#include <inttypes.h>
#include <uni_bluetooth.h>
#include <uni_log.h>
#include <uni_platform_arduino.h>
#include <uni_version.h>

Bluepad32::Bluepad32() : _prevConnectedControllers(0), _controllers(), _onConnect(), _onDisconnect() {}

const char* Bluepad32::firmwareVersion() const {
    return "Bluepad32 for Arduino v" UNI_VERSION;
}

void Bluepad32::update() {
    int connectedControllers = 0;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (arduino_get_controller_data(i, &_controllers[i]._data) == -1)
            continue;
        // Update Idx in case it is the first time to get updated.
        _controllers[i]._idx = i;
        connectedControllers |= (1 << i);
    }

    // No changes in connected controllers. No need to call onConnected or onDisconnected.
    if (connectedControllers == _prevConnectedControllers)
        return;

    logi("connected in total: 0x%02x (flag)\n", connectedControllers);

    // Compare bit by bit, and find which one got connected and which one disconnected.
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        int bit = (1 << i);
        int current = connectedControllers & bit;
        int prev = _prevConnectedControllers & bit;

        // No changes in this controller, skip
        if (current == prev)
            continue;

        if (current) {
            logi("controller connected: %d\n", i);
            _controllers[i].onConnected();
            _onConnect(&_controllers[i]);
        } else {
            _onDisconnect(&_controllers[i]);
            _controllers[i].onDisconnected();
            logi("controller disconnected: %d\n", i);
        }
    }

    _prevConnectedControllers = connectedControllers;
}

void Bluepad32::forgetBluetoothKeys() {
    uni_bluetooth_del_keys_safe();
}

void Bluepad32::enableNewBluetoothConnections(bool enabled) {
    uni_bluetooth_enable_new_connections_safe(enabled);
}

void Bluepad32::setup(const GamepadCallback& onConnect, const GamepadCallback& onDisconnect) {
    _onConnect = onConnect;
    _onDisconnect = onDisconnect;
}

Bluepad32 BP32;
