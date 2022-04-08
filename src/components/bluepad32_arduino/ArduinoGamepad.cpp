// Copyright 2021 - 2022, Ricardo Quesada, http://retro.moe
// SPDX-License-Identifier: Apache-2.0 or LGPL-2.1-or-later

#include "ArduinoGamepad.h"

#include <inttypes.h>
#include <uni_common.h>
#include <uni_debug.h>
#include <uni_platform_arduino.h>

#include "sdkconfig.h"
#ifndef CONFIG_BLUEPAD32_PLATFORM_ARDUINO
#error "Must only be compiled when using Bluepad32 Arduino platform"
#endif  // !CONFIG_BLUEPAD32_PLATFORM_ARDUINO

const struct Gamepad::controllerNames Gamepad::_controllerNames[] = {
    {Gamepad::CONTROLLER_TYPE_UnknownSteamController, "Unknown Steam"},
    {Gamepad::CONTROLLER_TYPE_SteamController, "Steam"},
    {Gamepad::CONTROLLER_TYPE_SteamControllerV2, "Steam V2"},

    {Gamepad::CONTROLLER_TYPE_XBox360Controller, "XBox 360"},
    {Gamepad::CONTROLLER_TYPE_XBoxOneController, "XBox One"},
    {Gamepad::CONTROLLER_TYPE_PS3Controller, "DualShock 3"},
    {Gamepad::CONTROLLER_TYPE_PS4Controller, "DualShock 4"},
    {Gamepad::CONTROLLER_TYPE_WiiController, "Wii"},
    {Gamepad::CONTROLLER_TYPE_AppleController, "Apple"},
    {Gamepad::CONTROLLER_TYPE_AndroidController, "Android"},
    {Gamepad::CONTROLLER_TYPE_SwitchProController, "Switch Pro"},
    {Gamepad::CONTROLLER_TYPE_SwitchJoyConLeft, "Switch JoyCon Left"},
    {Gamepad::CONTROLLER_TYPE_SwitchJoyConRight, "Switch JoyCon Right"},
    {Gamepad::CONTROLLER_TYPE_SwitchJoyConPair, "Switch JoyCon Pair"},
    {Gamepad::CONTROLLER_TYPE_SwitchInputOnlyController, "Switch Input Only"},
    {Gamepad::CONTROLLER_TYPE_MobileTouch, "Mobile Touch"},
    {Gamepad::CONTROLLER_TYPE_XInputSwitchController, "XInput Switch"},
    {Gamepad::CONTROLLER_TYPE_PS5Controller, "DualSense"},

    {Gamepad::CONTROLLER_TYPE_iCadeController, "iCade"},
    {Gamepad::CONTROLLER_TYPE_SmartTVRemoteController, "Smart TV Remote"},
    {Gamepad::CONTROLLER_TYPE_EightBitdoController, "8BitDo"},
    {Gamepad::CONTROLLER_TYPE_GenericController, "Generic"},
    {Gamepad::CONTROLLER_TYPE_NimbusController, "Nimbus"},
    {Gamepad::CONTROLLER_TYPE_OUYAController, "OUYA"},

    {Gamepad::CONTROLLER_TYPE_GenericKeyboard, "Keyboard"},
    {Gamepad::CONTROLLER_TYPE_GenericMouse, "Mouse"},
};

Gamepad::Gamepad() : _connected(false), _idx(-1), _data(), _properties() {}

bool Gamepad::isConnected() const {
    return _connected;
}

void Gamepad::setPlayerLEDs(uint8_t led) const {
    if (!isConnected()) {
        loge("gamepad not connected");
        return;
    }

    if (arduino_set_player_leds(_idx, led) == -1)
        loge("error setting player LEDs");
}

void Gamepad::setColorLED(uint8_t red, uint8_t green, uint8_t blue) const {
    if (!isConnected()) {
        loge("gamepad not connected");
        return;
    }

    if (arduino_set_lightbar_color(_idx, red, green, blue) == -1)
        loge("error setting lightbar color");
}

void Gamepad::setRumble(uint8_t force, uint8_t duration) const {
    if (!isConnected()) {
        loge("gamepad not connected");
        return;
    }

    if (arduino_set_rumble(_idx, force, duration) == -1)
        loge("error setting rumble");
}

String Gamepad::getModelName() const {
    for (int i = 0; i < ARRAY_SIZE(_controllerNames); i++) {
        if (_properties.type == _controllerNames[i].type)
            return _controllerNames[i].name;
    }
    return "Unknown";
}

GamepadProperties Gamepad::getProperties() const {
    return _properties;
}

// Private functions
void Gamepad::onConnected() {
    _connected = true;
    // Fetch properties, and have them cached.
    if (arduino_get_gamepad_properties(_idx, &_properties) != UNI_ARDUINO_OK) {
        loge("failed to get gamepad properties");
    }
}

void Gamepad::onDisconnected() {
    _connected = false;
}