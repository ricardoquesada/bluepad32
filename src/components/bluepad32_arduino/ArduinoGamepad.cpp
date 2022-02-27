// Copyright 2021 - 2022, Ricardo Quesada, http://retro.moe
// SPDX-License-Identifier: Apache-2.0 or LGPL-2.1-or-later

#include "ArduinoGamepad.h"

#include <inttypes.h>
#include <uni_debug.h>
#include <uni_platform_arduino.h>

#include "sdkconfig.h"
#ifndef CONFIG_BLUEPAD32_PLATFORM_ARDUINO
#error "Must only be compiled when using Bluepad32 Arduino platform"
#endif // !CONFIG_BLUEPAD32_PLATFORM_ARDUINO

Gamepad::Gamepad() : _connected(false), _state() {}

bool Gamepad::isConnected() const {
    return _connected;
}

void Gamepad::setPlayerLEDs(uint8_t led) const {
    if (!isConnected()) {
        loge("gamepad not connected");
        return;
    }

    if (arduino_set_player_leds(_state.idx, led) == -1)
        loge("error setting player LEDs");
}

void Gamepad::setColorLED(uint8_t red, uint8_t green, uint8_t blue) const {
    if (!isConnected()) {
        loge("gamepad not connected");
        return;
    }

    if (arduino_set_lightbar_color(_state.idx, red, green, blue) == -1)
        loge("error setting lightbar color");
}

void Gamepad::setRumble(uint8_t force, uint8_t duration) const {
    if (!isConnected()) {
        loge("gamepad not connected");
        return;
    }

    if (arduino_set_rumble(_state.idx, force, duration) == -1)
        loge("error setting rumble");
}
