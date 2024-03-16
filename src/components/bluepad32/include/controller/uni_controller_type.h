/*
  Copyright (C) Valve Corporation

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

// Taken from:
// https://github.com/libsdl-org/SDL/blob/main/src/joystick/controller_type.h

#ifndef UNI_CONTROLLER_TYPE_H
#define UNI_CONTROLLER_TYPE_H

#include <stdint.h>

//-----------------------------------------------------------------------------
// Purpose: Steam Controller models
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN A DATABASE
//-----------------------------------------------------------------------------
typedef enum {
    k_eControllerType_None = -1,
    k_eControllerType_Unknown = 0,

    // Steam Controllers
    k_eControllerType_UnknownSteamController = 1,
    k_eControllerType_SteamController = 2,
    k_eControllerType_SteamControllerV2 = 3,
    k_eControllerType_SteamControllerNeptune = 4,

    // Other Controllers
    k_eControllerType_UnknownNonSteamController = 30,
    k_eControllerType_XBox360Controller = 31,
    k_eControllerType_XBoxOneController = 32,
    k_eControllerType_PS3Controller = 33,
    k_eControllerType_PS4Controller = 34,
    k_eControllerType_WiiController = 35,
    k_eControllerType_AppleController = 36,
    k_eControllerType_AndroidController = 37,
    k_eControllerType_SwitchProController = 38,
    k_eControllerType_SwitchJoyConLeft = 39,
    k_eControllerType_SwitchJoyConRight = 40,
    k_eControllerType_SwitchJoyConPair = 41,
    k_eControllerType_SwitchInputOnlyController = 42,
    k_eControllerType_MobileTouch = 43,
    k_eControllerType_XInputSwitchController = 44,  // Client-side only, used to mark Nintendo Switch style controllers
                                                    // as using XInput instead of the Nintendo Switch protocol
    k_eControllerType_PS5Controller = 45,
    k_eControllerType_XInputPS4Controller = 46,  // Client-side only, used to mark DualShock 4 style controllers using
                                                 // XInput instead of the DualShock 4 controller protocol

    // Bluepad32 own extensions
    k_eControllerType_iCadeController = 50,          // (Bluepad32)
    k_eControllerType_SmartTVRemoteController = 51,  // (Bluepad32)
    k_eControllerType_8BitdoController = 52,         // (Bluepad32)
    k_eControllerType_GenericController = 53,        // (Bluepad32)
    k_eControllerType_NimbusController = 54,         // (Bluepad32)
    k_eControllerType_OUYAController = 55,           // (Bluepad32)
    k_eControllerType_PSMoveController = 56,         // (Bluepad32)
    k_eControllerType_AtariJoystick = 57,            // (Bluepad32)

    k_eControllerType_LastController,  // Don't add game controllers below this enumeration - this enumeration can
    // change value

    // Keyboards and Mice
    k_eControllertype_GenericKeyboard = 400,
    k_eControllertype_GenericMouse = 800,
} uni_controller_type_t;

typedef struct {
    uint32_t device_id;
    uni_controller_type_t controller_type;
    const char* name;
} uni_controller_description_t;

uni_controller_type_t uni_guess_controller_type(uint16_t vid, uint16_t pid);
const char* uni_guess_controller_name(uint16_t vid, uint16_t pid);

// Bluepad32 start
#define CONTROLLER_TYPE_None k_eControllerType_None
#define CONTROLLER_TYPE_Unknown k_eControllerType_Unknown
#define CONTROLLER_TYPE_UnknownSteamController k_eControllerType_UnknownSteamController
#define CONTROLLER_TYPE_SteamController k_eControllerType_SteamController
#define CONTROLLER_TYPE_SteamControllerV2 k_eControllerType_SteamControllerV2
#define CONTROLLER_TYPE_SteamControllerNeptune k_eControllerType_SteamControllerNeptune
#define CONTROLLER_TYPE_UnknownNonSteamController k_eControllerType_UnknownNonSteamController
#define CONTROLLER_TYPE_XBox360Controller k_eControllerType_XBox360Controller
#define CONTROLLER_TYPE_XBoxOneController k_eControllerType_XBoxOneController
#define CONTROLLER_TYPE_PS3Controller k_eControllerType_PS3Controller
#define CONTROLLER_TYPE_PS4Controller k_eControllerType_PS4Controller
#define CONTROLLER_TYPE_WiiController k_eControllerType_WiiController
#define CONTROLLER_TYPE_AppleController k_eControllerType_AppleController
#define CONTROLLER_TYPE_AndroidController k_eControllerType_AndroidController
#define CONTROLLER_TYPE_SwitchProController k_eControllerType_SwitchProController
#define CONTROLLER_TYPE_SwitchJoyConLeft k_eControllerType_SwitchJoyConLeft
#define CONTROLLER_TYPE_SwitchJoyConRight k_eControllerType_SwitchJoyConRight
#define CONTROLLER_TYPE_SwitchJoyConPair k_eControllerType_SwitchJoyConPair
#define CONTROLLER_TYPE_SwitchInputOnlyController k_eControllerType_SwitchInputOnlyController
#define CONTROLLER_TYPE_MobileTouch k_eControllerType_MobileTouch
#define CONTROLLER_TYPE_XInputSwitchController k_eControllerType_XInputSwitchController
#define CONTROLLER_TYPE_PS5Controller k_eControllerType_PS5Controller
#define CONTROLLER_TYPE_XInputPS4Controller k_eControllerType_XInputPS4Controller
#define CONTROLLER_TYPE_iCadeController k_eControllerType_iCadeController
#define CONTROLLER_TYPE_SmartTVRemoteController k_eControllerType_SmartTVRemoteController
#define CONTROLLER_TYPE_8BitdoController k_eControllerType_8BitdoController
#define CONTROLLER_TYPE_GenericController k_eControllerType_GenericController
#define CONTROLLER_TYPE_NimbusController k_eControllerType_NimbusController
#define CONTROLLER_TYPE_OUYAController k_eControllerType_OUYAController
#define CONTROLLER_TYPE_PSMoveController k_eControllerType_PSMoveController
#define CONTROLLER_TYPE_AtariJoystick k_eControllerType_AtariJoystick
#define CONTROLLER_TYPE_LastController k_eControllerType_LastController
#define CONTROLLER_TYPE_GenericKeyboard k_eControllertype_GenericKeyboard
#define CONTROLLER_TYPE_GenericMouse k_eControllertype_GenericMouse
// Bluepad32 end

#endif  // UNI_CONTROLLER_TYPE_H