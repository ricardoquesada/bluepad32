// Copyright 2021 - 2021, Ricardo Quesada, http://retro.moe
// SPDX-License-Identifier: Apache 2.0 or LGPL-2.1-or-later

#ifndef BP32_ARDUINO_BLUEPAD32_H
#define BP32_ARDUINO_BLUEPAD32_H

#include "sdkconfig.h"
#ifndef CONFIG_BLUEPAD32_PLATFORM_ARDUINO
#error "Must only be compiled when using Bluepad32 Arduino platform"
#endif  // !CONFIG_BLUEPAD32_PLATFORM_ARDUINO

#include <inttypes.h>
#include <uni_platform_arduino.h>

#include <functional>

#include "ArduinoController.h"

// Arduino friendly define. Matches the one used in "bluepad32-arduino" (NINA) project.
#define BP32_MAX_CONTROLLERS CONFIG_BLUEPAD32_MAX_DEVICES
#define BP32_MAX_GAMEPADS BP32_MAX_CONTROLLERS

typedef std::function<void(ControllerPtr controller)> ControllerCallback;
using GamepadCallback = ControllerCallback;

class Bluepad32 {
    // This is used internally by SPI, and then copied into the Controller::State of
    // each controller
    int _prevConnectedControllers;

    // This is what the user receives
    Controller _controllers[BP32_MAX_CONTROLLERS];

    ControllerCallback _onConnect;
    ControllerCallback _onDisconnect;

   public:
    Bluepad32();
    /*
     * Get the firmware version
     * result: version as string with this format a.b.c
     */
    const char* firmwareVersion() const;
    void setDebug(uint8_t on);

    // Controller
    void update();

    // When a controller is paired to the ESP32, the ESP32 stores keys to enable reconnection.
    // If you want to "forget" (delete) the keys from ESP32, you should call this
    // function.
    void forgetBluetoothKeys();

    // Enable / Disable new Bluetooth connections.
    // When enabled, the device is put in Discovery mode, and new pairs are accepted.
    // When disabled, only devices that have paired before can connect.
    // Established connections are not affected.
    void enableNewBluetoothConnections(bool enabled);

    void setup(const ControllerCallback& onConnect, const ControllerCallback& onDisconnect);

   private:
    void checkProtocol();
};

extern Bluepad32 BP32;

#endif  // BP32_ARDUINO_BLUEPAD32_H
