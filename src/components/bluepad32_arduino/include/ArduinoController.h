// Copyright 2021 - 2023, Ricardo Quesada, http://retro.moe
// SPDX-License-Identifier: Apache-2.0 or LGPL-2.1-or-later

#ifndef BP32_ARDUINO_CONTROLLER_H
#define BP32_ARDUINO_CONTROLLER_H

#include "sdkconfig.h"
#ifndef CONFIG_BLUEPAD32_PLATFORM_ARDUINO
#error "Must only be compiled when using Bluepad32 Arduino platform"
#endif  // !CONFIG_BLUEPAD32_PLATFORM_ARDUINO

#include <inttypes.h>
#include <uni_platform_arduino.h>

#include <Arduino.h>

#include "ArduinoControllerData.h"
#include "ArduinoControllerProperties.h"

class Controller {
   public:
    // FIXME: Should not be duplicated.
    // Must match values from "uni_hid_device_vendors.h"
    enum {
        CONTROLLER_TYPE_None = -1,
        CONTROLLER_TYPE_Unknown = 0,

        // Steam Controllers
        CONTROLLER_TYPE_UnknownSteamController = 1,
        CONTROLLER_TYPE_SteamController = 2,
        CONTROLLER_TYPE_SteamControllerV2 = 3,

        // Other Controllers
        CONTROLLER_TYPE_UnknownNonSteamController = 30,
        CONTROLLER_TYPE_XBox360Controller = 31,
        CONTROLLER_TYPE_XBoxOneController = 32,
        CONTROLLER_TYPE_PS3Controller = 33,
        CONTROLLER_TYPE_PS4Controller = 34,
        CONTROLLER_TYPE_WiiController = 35,
        CONTROLLER_TYPE_AppleController = 36,
        CONTROLLER_TYPE_AndroidController = 37,
        CONTROLLER_TYPE_SwitchProController = 38,
        CONTROLLER_TYPE_SwitchJoyConLeft = 39,
        CONTROLLER_TYPE_SwitchJoyConRight = 40,
        CONTROLLER_TYPE_SwitchJoyConPair = 41,
        CONTROLLER_TYPE_SwitchInputOnlyController = 42,
        CONTROLLER_TYPE_MobileTouch = 43,
        CONTROLLER_TYPE_XInputSwitchController = 44,  // Client-side only, used to mark Switch-compatible controllers as
                                                      // not supporting Switch controller protocol
        CONTROLLER_TYPE_PS5Controller = 45,

        // Bluepad32 own extensions
        CONTROLLER_TYPE_iCadeController = 50,          // (Bluepad32)
        CONTROLLER_TYPE_SmartTVRemoteController = 51,  // (Bluepad32)
        CONTROLLER_TYPE_EightBitdoController = 52,     // (Bluepad32)
        CONTROLLER_TYPE_GenericController = 53,        // (Bluepad32)
        CONTROLLER_TYPE_NimbusController = 54,         // (Bluepad32)
        CONTROLLER_TYPE_OUYAController = 55,           // (Bluepad32)

        CONTROLLER_TYPE_LastController,  // Don't add game controllers below this
                                         // enumeration - this enumeration can
                                         // change value

        // Keyboards and Mice
        CONTROLLER_TYPE_GenericKeyboard = 400,
        CONTROLLER_TYPE_GenericMouse = 800,
    };

    Controller();

    //
    // Gamepad Related
    //

    uint8_t dpad() const { return _data.gamepad.dpad; }

    // Axis
    int32_t axisX() const { return _data.gamepad.axis_x; }
    int32_t axisY() const { return _data.gamepad.axis_y; }
    int32_t axisRX() const { return _data.gamepad.axis_rx; }
    int32_t axisRY() const { return _data.gamepad.axis_ry; }

    // Brake & Throttle
    int32_t brake() const { return _data.gamepad.brake; }
    int32_t throttle() const { return _data.gamepad.throttle; }

    //
    // Shared between Mouse & Gamepad
    //

    // Returns the state of all buttons.
    uint16_t buttons() const {
        if (_data.klass == UNI_CONTROLLER_CLASS_GAMEPAD)
            return _data.gamepad.buttons;
        if (_data.klass == UNI_CONTROLLER_CLASS_MOUSE)
            return _data.mouse.buttons;
        // Not supported in other controllers
        return 0;
    }

    // Returns the state of all misc buttons.
    uint16_t miscButtons() const {
        if (_data.klass == UNI_CONTROLLER_CLASS_GAMEPAD)
            return _data.gamepad.misc_buttons;
        if (_data.klass == UNI_CONTROLLER_CLASS_MOUSE)
            return _data.mouse.misc_buttons;
        // Not supported in other controllers
        return 0;
    }

    // To test one button at the time.
    bool a() const { return buttons() & BUTTON_A; }
    bool b() const { return buttons() & BUTTON_B; }
    bool x() const { return buttons() & BUTTON_X; }
    bool y() const { return buttons() & BUTTON_Y; }
    bool l1() const { return buttons() & BUTTON_SHOULDER_L; }
    bool l2() const { return buttons() & BUTTON_TRIGGER_L; }
    bool r1() const { return buttons() & BUTTON_SHOULDER_R; }
    bool r2() const { return buttons() & BUTTON_TRIGGER_R; }
    bool thumbL() const { return buttons() & BUTTON_THUMB_L; }
    bool thumbR() const { return buttons() & BUTTON_THUMB_R; }

    // Misc buttons
    bool miscSystem() const { return miscButtons() & MISC_BUTTON_SYSTEM; }
    bool miscBack() const { return miscButtons() & MISC_BUTTON_BACK; }
    bool miscHome() const { return miscButtons() & MISC_BUTTON_HOME; }

    //
    // Mouse related
    //
    int32_t deltaX() const { return _data.mouse.delta_x; }
    int32_t deltaY() const { return _data.mouse.delta_y; }
    int8_t scrollWheel() const { return _data.mouse.scroll_wheel; }

    //
    // Wii Balance Board related
    //
    uint16_t topLeft() const { return _data.balance_board.tl; }
    uint16_t topRight() const { return _data.balance_board.tr; }
    uint16_t bottomLeft() const { return _data.balance_board.bl; }
    uint16_t bottomRight() const { return _data.balance_board.br; }
    int temperature() const { return _data.balance_board.temperature; }

    //
    // Shared among all
    //

    // 0 = Unknown Battery state
    // 1 = Battery Empty
    // 255 = Battery full
    uint8_t battery() const { return _data.battery; }

    bool isGamepad() const { return _data.klass == UNI_CONTROLLER_CLASS_GAMEPAD; }
    bool isMouse() const { return _data.klass == UNI_CONTROLLER_CLASS_MOUSE; }
    bool isBalanceBoard() const { return _data.klass == UNI_CONTROLLER_CLASS_BALANCE_BOARD; }
    int8_t index() const { return _idx; }

    bool isConnected() const;

    uni_controller_class_t getClass() const { return _data.klass; }
    // Returns the controller model.
    int getModel() const { return _properties.type; }
    String getModelName() const;
    ControllerProperties getProperties() const { return _properties; }

    // "Output" functions.
    void setPlayerLEDs(uint8_t led) const;
    void setColorLED(uint8_t red, uint8_t green, uint8_t blue) const;
    void setRumble(uint8_t force, uint8_t duration) const;

   private:
    void onConnected();
    void onDisconnected();

    bool _connected;
    // Controller index, from 0 to 3.
    int8_t _idx;
    ControllerData _data;
    ControllerProperties _properties;

    // Delete copy constructor to avoid copying the state by mistake. If so,
    // chances are that the controller won't get updated automatically.
    Controller(const Controller&) = delete;

    // For converting controller types to names.
    struct controllerNames {
        int type;
        const char* name;
    };
    static const struct controllerNames _controllerNames[];

    // Friends
    friend class Bluepad32;
};

typedef Controller* ControllerPtr;

#endif  // BP32_ARDUINO_CONTROLLER_H
