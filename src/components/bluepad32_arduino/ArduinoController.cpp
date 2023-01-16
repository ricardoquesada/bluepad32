// Copyright 2021 - 2023, Ricardo Quesada, http://retro.moe
// SPDX-License-Identifier: Apache-2.0 or LGPL-2.1-or-later

#include "ArduinoGamepad.h"

#include <inttypes.h>
#include <uni_common.h>
#include <uni_log.h>
#include <uni_platform_arduino.h>

#include "sdkconfig.h"
#ifndef CONFIG_BLUEPAD32_PLATFORM_ARDUINO
#error "Must only be compiled when using Bluepad32 Arduino platform"
#endif  // !CONFIG_BLUEPAD32_PLATFORM_ARDUINO

const struct Controller::controllerNames Controller::_controllerNames[] = {
    {Controller::CONTROLLER_TYPE_UnknownSteamController, "Unknown Steam"},
    {Controller::CONTROLLER_TYPE_SteamController, "Steam"},
    {Controller::CONTROLLER_TYPE_SteamControllerV2, "Steam V2"},

    {Controller::CONTROLLER_TYPE_XBox360Controller, "XBox 360"},
    {Controller::CONTROLLER_TYPE_XBoxOneController, "XBox One"},
    {Controller::CONTROLLER_TYPE_PS3Controller, "DualShock 3"},
    {Controller::CONTROLLER_TYPE_PS4Controller, "DualShock 4"},
    {Controller::CONTROLLER_TYPE_WiiController, "Wii"},
    {Controller::CONTROLLER_TYPE_AppleController, "Apple"},
    {Controller::CONTROLLER_TYPE_AndroidController, "Android"},
    {Controller::CONTROLLER_TYPE_SwitchProController, "Switch Pro"},
    {Controller::CONTROLLER_TYPE_SwitchJoyConLeft, "Switch JoyCon Left"},
    {Controller::CONTROLLER_TYPE_SwitchJoyConRight, "Switch JoyCon Right"},
    {Controller::CONTROLLER_TYPE_SwitchJoyConPair, "Switch JoyCon Pair"},
    {Controller::CONTROLLER_TYPE_SwitchInputOnlyController, "Switch Input Only"},
    {Controller::CONTROLLER_TYPE_MobileTouch, "Mobile Touch"},
    {Controller::CONTROLLER_TYPE_XInputSwitchController, "XInput Switch"},
    {Controller::CONTROLLER_TYPE_PS5Controller, "DualSense"},

    {Controller::CONTROLLER_TYPE_iCadeController, "iCade"},
    {Controller::CONTROLLER_TYPE_SmartTVRemoteController, "Smart TV Remote"},
    {Controller::CONTROLLER_TYPE_EightBitdoController, "8BitDo"},
    {Controller::CONTROLLER_TYPE_GenericController, "Generic"},
    {Controller::CONTROLLER_TYPE_NimbusController, "Nimbus"},
    {Controller::CONTROLLER_TYPE_OUYAController, "OUYA"},

    {Controller::CONTROLLER_TYPE_GenericKeyboard, "Keyboard"},
    {Controller::CONTROLLER_TYPE_GenericMouse, "Mouse"},
};

Controller::Controller() : _connected(false), _idx(-1), _data(), _properties() {}

bool Controller::isConnected() const {
    return _connected;
}

void Controller::setPlayerLEDs(uint8_t led) const {
    if (!isConnected()) {
        loge("controller not connected");
        return;
    }

    if (arduino_set_player_leds(_idx, led) == -1)
        loge("error setting player LEDs");
}

void Controller::setColorLED(uint8_t red, uint8_t green, uint8_t blue) const {
    if (!isConnected()) {
        loge("controller not connected");
        return;
    }

    if (arduino_set_lightbar_color(_idx, red, green, blue) == -1)
        loge("error setting lightbar color");
}

void Controller::setRumble(uint8_t force, uint8_t duration) const {
    if (!isConnected()) {
        loge("controller not connected");
        return;
    }

    if (arduino_set_rumble(_idx, force, duration) == -1)
        loge("error setting rumble");
}

String Controller::getModelName() const {
    for (int i = 0; i < ARRAY_SIZE(_controllerNames); i++) {
        if (_properties.type == _controllerNames[i].type)
            return _controllerNames[i].name;
    }
    return "Unknown";
}

// Private functions
void Controller::onConnected() {
    _connected = true;
    // Fetch properties, and have them cached.
    if (arduino_get_controller_properties(_idx, &_properties) != UNI_ARDUINO_OK) {
        loge("failed to get controller properties");
    }
}

void Controller::onDisconnected() {
    _connected = false;
}
