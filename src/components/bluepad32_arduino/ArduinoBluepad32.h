// Copyright 2021 - 2021, Ricardo Quesada, http://retro.moe
// SPDX-License-Identifier: Apache 2.0 or LGPL-2.1-or-later

#ifndef BP32_ARDUINO_BLUEPAD32_H
#define BP32_ARDUINO_BLUEPAD32_H

#include "sdkconfig.h"
#ifdef CONFIG_BLUEPAD32_PLATFORM_ARDUINO

#include <inttypes.h>
#include <uni_platform_arduino.h>

#include <functional>

#include "ArduinoGamepad.h"

typedef std::function<void(GamepadPtr gamepad)> GamepadCallback;

class Bluepad32 {
    // This is used internally by SPI, and then copied into the Gamepad::State of
    // each gamepad
    int _prevConnectedGamepads;

    // This is what the user receives
    Gamepad _gamepads[ARDUINO_MAX_GAMEPADS];

    GamepadCallback _onConnect;
    GamepadCallback _onDisconnect;

   public:
    Bluepad32();
    /*
     * Get the firmware version
     * result: version as string with this format a.b.c
     */
    const char* firmwareVersion() const;
    void setDebug(uint8_t on);

    // Gamepad
    void update();

    // When a gamepad connects to the ESP32, the ESP32 stores keys to make it
    // easier the reconnection.
    // If you want to "forget" (delete) the keys from ESP32, you should call this
    // function.
    void forgetBluetoothKeys();

    void setup(const GamepadCallback& onConnect, const GamepadCallback& onDisconnect);

   private:
    void checkProtocol();
};

extern Bluepad32 BP32;

#endif  // CONFIG_BLUEPAD32_PLATFORM_ARDUINO
#endif  // BP32_ARDUINO_BLUEPAD32_H
