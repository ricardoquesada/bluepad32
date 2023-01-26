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

// Based on libsdl "controller_type.h" file.
// http://hg.libsdl.org/SDL/file/28fcb5ef7ff1/src/joystick/controller_type.h

#ifndef UNI_HID_DEVICE_VENDORS_H
#define UNI_HID_DEVICE_VENDORS_H

#include <stdint.h>

// clang-format off
typedef enum {
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
  CONTROLLER_TYPE_XInputPS4Controller = 46,     // Client-side only, used to mark DualShock 4 style controllers
                                                // using XInput instead of the DualShock 4 controller protocol

  // Bluepad32 own extensions
  CONTROLLER_TYPE_iCadeController = 50,          // (Bluepad32)
  CONTROLLER_TYPE_SmartTVRemoteController = 51,  // (Bluepad32)
  CONTROLLER_TYPE_8BitdoController = 52,         // (Bluepad32)
  CONTROLLER_TYPE_GenericController = 53,        // (Bluepad32)
  CONTROLLER_TYPE_NimbusController = 54,         // (Bluepad32)
  CONTROLLER_TYPE_OUYAController = 55,           // (Bluepad32)

  CONTROLLER_TYPE_LastController,  // Don't add game controllers below this enumeration -
                                   // this enumeration can change value

  // Keyboards and Mice
  CONTROLLER_TYPE_GenericKeyboard = 400,
  CONTROLLER_TYPE_GenericMouse = 800,
} uni_controller_type_t;
// clang-format on

#define MAKE_CONTROLLER_ID(nVID, nPID) (uint32_t)(nVID << 16 | nPID)
typedef struct {
    uint32_t device_id;
    uni_controller_type_t controller_type;
    const char* name;
} uni_controller_description_t;

// clang-format off
static const uni_controller_description_t arrControllers[] = {
	{ MAKE_CONTROLLER_ID( 0x0000, 0x0000 ), CONTROLLER_TYPE_Unknown, NULL },  // Bluepad32: Make it first entry

	{ MAKE_CONTROLLER_ID( 0x0079, 0x181a ), CONTROLLER_TYPE_PS3Controller, NULL },	// Venom Arcade Stick
	{ MAKE_CONTROLLER_ID( 0x0079, 0x1844 ), CONTROLLER_TYPE_PS3Controller, NULL },	// From SDL
	{ MAKE_CONTROLLER_ID( 0x044f, 0xb315 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Firestorm Dual Analog 3
	{ MAKE_CONTROLLER_ID( 0x044f, 0xd007 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Thrustmaster wireless 3-1
	//{ MAKE_CONTROLLER_ID( 0x046d, 0xc24f ), CONTROLLER_TYPE_PS3Controller, NULL },	// Logitech G29 (PS3)
	{ MAKE_CONTROLLER_ID( 0x054c, 0x0268 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Sony PS3 Controller
	{ MAKE_CONTROLLER_ID( 0x056e, 0x200f ), CONTROLLER_TYPE_PS3Controller, NULL },	// From SDL
	{ MAKE_CONTROLLER_ID( 0x056e, 0x2013 ), CONTROLLER_TYPE_PS3Controller, NULL },	// JC-U4113SBK
	{ MAKE_CONTROLLER_ID( 0x05b8, 0x1004 ), CONTROLLER_TYPE_PS3Controller, NULL },	// From SDL
	{ MAKE_CONTROLLER_ID( 0x05b8, 0x1006 ), CONTROLLER_TYPE_PS3Controller, NULL },	// JC-U3412SBK
	{ MAKE_CONTROLLER_ID( 0x06a3, 0xf622 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Cyborg V3
	{ MAKE_CONTROLLER_ID( 0x0738, 0x3180 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Mad Catz Alpha PS3 mode
	{ MAKE_CONTROLLER_ID( 0x0738, 0x3250 ), CONTROLLER_TYPE_PS3Controller, NULL },	// madcats fightpad pro ps3
	{ MAKE_CONTROLLER_ID( 0x0738, 0x3481 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Mad Catz FightStick TE 2+ PS3
	{ MAKE_CONTROLLER_ID( 0x0738, 0x8180 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Mad Catz Alpha PS4 mode (no touchpad on device)
	{ MAKE_CONTROLLER_ID( 0x0738, 0x8838 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Madcatz Fightstick Pro
	{ MAKE_CONTROLLER_ID( 0x0810, 0x0001 ), CONTROLLER_TYPE_PS3Controller, NULL },	// actually ps2 - maybe break out later
	{ MAKE_CONTROLLER_ID( 0x0810, 0x0003 ), CONTROLLER_TYPE_PS3Controller, NULL },	// actually ps2 - maybe break out later
	{ MAKE_CONTROLLER_ID( 0x0925, 0x0005 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Sony PS3 Controller
	{ MAKE_CONTROLLER_ID( 0x0925, 0x8866 ), CONTROLLER_TYPE_PS3Controller, NULL },	// PS2 maybe break out later
	{ MAKE_CONTROLLER_ID( 0x0925, 0x8888 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Actually ps2 -maybe break out later Lakeview Research WiseGroup Ltd, MP-8866 Dual Joypad
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0109 ), CONTROLLER_TYPE_PS3Controller, NULL },	// PDP Versus Fighting Pad
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x011e ), CONTROLLER_TYPE_PS3Controller, NULL },	// Rock Candy PS4
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0128 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Rock Candy PS3
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0203 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Victrix Pro FS (PS4 peripheral but no trackpad/lightbar)
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0214 ), CONTROLLER_TYPE_PS3Controller, NULL },	// afterglow ps3
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x1314 ), CONTROLLER_TYPE_PS3Controller, NULL },	// PDP Afterglow Wireless PS3 controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x6302 ), CONTROLLER_TYPE_PS3Controller, NULL },	// From SDL
	{ MAKE_CONTROLLER_ID( 0x0e8f, 0x0008 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Green Asia
	{ MAKE_CONTROLLER_ID( 0x0e8f, 0x3075 ), CONTROLLER_TYPE_PS3Controller, NULL },	// SpeedLink Strike FX
	{ MAKE_CONTROLLER_ID( 0x0e8f, 0x310d ), CONTROLLER_TYPE_PS3Controller, NULL },	// From SDL
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0009 ), CONTROLLER_TYPE_PS3Controller, NULL },	// HORI BDA GP1
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x004d ), CONTROLLER_TYPE_PS3Controller, NULL },	// Horipad 3
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x005f ), CONTROLLER_TYPE_PS3Controller, NULL },	// HORI Fighting Commander 4 PS3
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x006a ), CONTROLLER_TYPE_PS3Controller, NULL },	// Real Arcade Pro 4
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x006e ), CONTROLLER_TYPE_PS3Controller, NULL },	// HORI horipad4 ps3
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0085 ), CONTROLLER_TYPE_PS3Controller, NULL },	// HORI Fighting Commander PS3
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0086 ), CONTROLLER_TYPE_PS3Controller, NULL },	// HORI Fighting Commander PC (Uses the Xbox 360 protocol, but has PS3 buttons)
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0088 ), CONTROLLER_TYPE_PS3Controller, NULL },	// HORI Fighting Stick mini 4
	{ MAKE_CONTROLLER_ID( 0x0f30, 0x1100 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Qanba Q1 fight stick
	{ MAKE_CONTROLLER_ID( 0x11ff, 0x3331 ), CONTROLLER_TYPE_PS3Controller, NULL },	// SRXJ-PH2400
	{ MAKE_CONTROLLER_ID( 0x1345, 0x1000 ), CONTROLLER_TYPE_PS3Controller, NULL },	// PS2 ACME GA-D5
	{ MAKE_CONTROLLER_ID( 0x1345, 0x6005 ), CONTROLLER_TYPE_PS3Controller, NULL },	// ps2 maybe break out later
	{ MAKE_CONTROLLER_ID( 0x146b, 0x5500 ), CONTROLLER_TYPE_PS3Controller, NULL },	// From SDL
	{ MAKE_CONTROLLER_ID( 0x1a34, 0x0836 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Afterglow PS3
	{ MAKE_CONTROLLER_ID( 0x20bc, 0x5500 ), CONTROLLER_TYPE_PS3Controller, NULL },	// ShanWan PS3
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x576d ), CONTROLLER_TYPE_PS3Controller, NULL },	// Power A PS3
	{ MAKE_CONTROLLER_ID( 0x20d6, 0xca6d ), CONTROLLER_TYPE_PS3Controller, NULL },	// From SDL
	{ MAKE_CONTROLLER_ID( 0x2563, 0x0523 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Digiflip GP006
	{ MAKE_CONTROLLER_ID( 0x2563, 0x0575 ), CONTROLLER_TYPE_PS3Controller, NULL },	// From SDL
	{ MAKE_CONTROLLER_ID( 0x25f0, 0x83c3 ), CONTROLLER_TYPE_PS3Controller, NULL },	// gioteck vx2
	{ MAKE_CONTROLLER_ID( 0x25f0, 0xc121 ), CONTROLLER_TYPE_PS3Controller, NULL },	//
	{ MAKE_CONTROLLER_ID( 0x2c22, 0x2003 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Qanba Drone
	{ MAKE_CONTROLLER_ID( 0x2c22, 0x2302 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Qanba Obsidian
	{ MAKE_CONTROLLER_ID( 0x2c22, 0x2502 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Qanba Dragon
	{ MAKE_CONTROLLER_ID( 0x8380, 0x0003 ), CONTROLLER_TYPE_PS3Controller, NULL },	// BTP 2163
	{ MAKE_CONTROLLER_ID( 0x8888, 0x0308 ), CONTROLLER_TYPE_PS3Controller, NULL },	// Sony PS3 Controller

	{ MAKE_CONTROLLER_ID( 0x0079, 0x181b ), CONTROLLER_TYPE_PS4Controller, NULL },	// Venom Arcade Stick - XXX:this may not work and may need to be called a ps3 controller
	//{ MAKE_CONTROLLER_ID( 0x046d, 0xc260 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Logitech G29 (PS4)
	{ MAKE_CONTROLLER_ID( 0x044f, 0xd00e ), CONTROLLER_TYPE_PS4Controller, NULL },	// Thrustmaster Eswap Pro - No gyro and lightbar doesn't change color. Works otherwise
	{ MAKE_CONTROLLER_ID( 0x054c, 0x05c4 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Sony PS4 Controller
	{ MAKE_CONTROLLER_ID( 0x054c, 0x05c5 ), CONTROLLER_TYPE_PS4Controller, NULL },	// STRIKEPAD PS4 Grip Add-on
	{ MAKE_CONTROLLER_ID( 0x054c, 0x09cc ), CONTROLLER_TYPE_PS4Controller, NULL },	// Sony PS4 Slim Controller
	{ MAKE_CONTROLLER_ID( 0x054c, 0x0ba0 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Sony PS4 Controller (Wireless dongle)
	{ MAKE_CONTROLLER_ID( 0x0738, 0x8250 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Mad Catz FightPad Pro PS4
	{ MAKE_CONTROLLER_ID( 0x0738, 0x8384 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Mad Catz FightStick TE S+ PS4
	{ MAKE_CONTROLLER_ID( 0x0738, 0x8480 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Mad Catz FightStick TE 2 PS4
	{ MAKE_CONTROLLER_ID( 0x0738, 0x8481 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Mad Catz FightStick TE 2+ PS4
	{ MAKE_CONTROLLER_ID( 0x0c12, 0x0e10 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Armor Armor 3 Pad PS4
	{ MAKE_CONTROLLER_ID( 0x0c12, 0x0e13 ), CONTROLLER_TYPE_PS4Controller, NULL },	// ZEROPLUS P4 Wired Gamepad
	{ MAKE_CONTROLLER_ID( 0x0c12, 0x0e15 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Game:Pad 4
	{ MAKE_CONTROLLER_ID( 0x0c12, 0x0e20 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Brook Mars Controller - needs FW update to show up as Ps4 controller on PC. Has Gyro but touchpad is a single button.
	{ MAKE_CONTROLLER_ID( 0x0c12, 0x0ef6 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Hitbox Arcade Stick
	{ MAKE_CONTROLLER_ID( 0x0c12, 0x1cf6 ), CONTROLLER_TYPE_PS4Controller, NULL },	// EMIO PS4 Elite Controller
	{ MAKE_CONTROLLER_ID( 0x0c12, 0x1e10 ), CONTROLLER_TYPE_PS4Controller, NULL },	// P4 Wired Gamepad generic knock off - lightbar but not trackpad or gyro
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0207 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Victrix Pro Fightstick w/ Touchpad for PS4
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0055 ), CONTROLLER_TYPE_PS4Controller, NULL },	// HORIPAD 4 FPS
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x005e ), CONTROLLER_TYPE_PS4Controller, NULL },	// HORI Fighting Commander 4 PS4
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0066 ), CONTROLLER_TYPE_PS4Controller, NULL },	// HORIPAD 4 FPS Plus 
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0084 ), CONTROLLER_TYPE_PS4Controller, NULL },	// HORI Fighting Commander PS4
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0087 ), CONTROLLER_TYPE_PS4Controller, NULL },	// HORI Fighting Stick mini 4
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x008a ), CONTROLLER_TYPE_PS4Controller, NULL },	// HORI Real Arcade Pro 4
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x009c ), CONTROLLER_TYPE_PS4Controller, NULL },	// HORI TAC PRO mousething
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00a0 ), CONTROLLER_TYPE_PS4Controller, NULL },	// HORI TAC4 mousething
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00ed ), CONTROLLER_TYPE_XInputPS4Controller, NULL },	// Hori Fighting Stick mini 4 kai - becomes an Xbox 360 controller on PC
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00ee ), CONTROLLER_TYPE_PS4Controller, NULL },	// Hori mini wired https://www.playstation.com/en-us/explore/accessories/gaming-controllers/mini-wired-gamepad/
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x011c ), CONTROLLER_TYPE_PS4Controller, NULL },	// Hori Fighting Stick α
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0123 ), CONTROLLER_TYPE_PS4Controller, NULL },	// HORI Wireless Controller Light (Japan only) - only over bt- over usb is xbox and pid 0x0124
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0162 ), CONTROLLER_TYPE_PS4Controller, NULL },	// HORI Fighting Commander OCTA
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0164 ), CONTROLLER_TYPE_XInputPS4Controller, NULL },	// HORI Fighting Commander OCTA
	{ MAKE_CONTROLLER_ID( 0x11c0, 0x4001 ), CONTROLLER_TYPE_PS4Controller, NULL },	// "PS4 Fun Controller" added from user log
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0603 ), CONTROLLER_TYPE_XInputPS4Controller, NULL },	// Nacon PS4 Compact Controller
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0604 ), CONTROLLER_TYPE_XInputPS4Controller, NULL },	// NACON Daija Arcade Stick
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0605 ), CONTROLLER_TYPE_XInputPS4Controller, NULL },	// NACON PS4 controller in Xbox mode - might also be other bigben brand xbox controllers
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0606 ), CONTROLLER_TYPE_XInputPS4Controller, NULL },	// NACON Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0609 ), CONTROLLER_TYPE_XInputPS4Controller, NULL },	// NACON Wireless Controller for PS4
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0d01 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Nacon Revolution Pro Controller - has gyro
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0d02 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Nacon Revolution Pro Controller v2 - has gyro
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0d06 ), CONTROLLER_TYPE_PS4Controller, NULL },	// NACON Asymetrical Controller Wireless Dongle -- show up as ps4 until you connect controller to it then it reboots into Xbox controller with different vvid/pid
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0d08 ), CONTROLLER_TYPE_PS4Controller, NULL },	// NACON Revolution Unlimited Wireless Dongle 
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0d09 ), CONTROLLER_TYPE_PS4Controller, NULL },	// NACON Daija Fight Stick - touchpad but no gyro/rumble
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0d10 ), CONTROLLER_TYPE_PS4Controller, NULL },	// NACON Revolution Infinite - has gyro
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0d10 ), CONTROLLER_TYPE_PS4Controller, NULL },	// NACON Revolution Unlimited
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0d13 ), CONTROLLER_TYPE_PS4Controller, NULL },	// NACON Revolution Pro Controller 3
	{ MAKE_CONTROLLER_ID( 0x146b, 0x1103 ), CONTROLLER_TYPE_PS4Controller, NULL },	// NACON Asymetrical Controller -- on windows this doesn't enumerate
	{ MAKE_CONTROLLER_ID( 0x1532, 0X0401 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Razer Panthera PS4 Controller
	{ MAKE_CONTROLLER_ID( 0x1532, 0x1000 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Razer Raiju PS4 Controller
	{ MAKE_CONTROLLER_ID( 0x1532, 0x1004 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Razer Raiju 2 Ultimate USB
	{ MAKE_CONTROLLER_ID( 0x1532, 0x1007 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Razer Raiju 2 Tournament edition USB
	{ MAKE_CONTROLLER_ID( 0x1532, 0x1008 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Razer Panthera Evo Fightstick
	{ MAKE_CONTROLLER_ID( 0x1532, 0x1009 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Razer Raiju 2 Ultimate BT
	{ MAKE_CONTROLLER_ID( 0x1532, 0x100A ), CONTROLLER_TYPE_PS4Controller, NULL },	// Razer Raiju 2 Tournament edition BT
	{ MAKE_CONTROLLER_ID( 0x1532, 0x1100 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Razer RAION Fightpad - Trackpad, no gyro, lightbar hardcoded to green
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x792a ), CONTROLLER_TYPE_PS4Controller, NULL },	// PowerA Fusion Fight Pad
	{ MAKE_CONTROLLER_ID( 0x2c22, 0x2000 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Qanba Drone
	{ MAKE_CONTROLLER_ID( 0x2c22, 0x2300 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Qanba Obsidian
	{ MAKE_CONTROLLER_ID( 0x2c22, 0x2303 ), CONTROLLER_TYPE_XInputPS4Controller, NULL },	// Qanba Obsidian Arcade Joystick
	{ MAKE_CONTROLLER_ID( 0x2c22, 0x2500 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Qanba Dragon
	{ MAKE_CONTROLLER_ID( 0x2c22, 0x2503 ), CONTROLLER_TYPE_XInputPS4Controller, NULL },	// Qanba Dragon Arcade Joystick
	{ MAKE_CONTROLLER_ID( 0x7545, 0x0104 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Armor 3 or Level Up Cobra - At least one variant has gyro
	{ MAKE_CONTROLLER_ID( 0x9886, 0x0025 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Astro C40
	// Removing the Giotek because there were a bunch of help tickets from users w/ issues including from non-PS4 controller users. This VID/PID is probably used in different FW's
//	{ MAKE_CONTROLLER_ID( 0x7545, 0x1122 ), CONTROLLER_TYPE_PS4Controller, NULL },	// Giotek VX4 - trackpad/gyro don't work. Had to not filter on interface info. Light bar is flaky, but works.

	{ MAKE_CONTROLLER_ID( 0x054c, 0x0ce6 ), CONTROLLER_TYPE_PS5Controller, NULL },	// Sony PS5 Controller
	{ MAKE_CONTROLLER_ID( 0x054c, 0x0df2 ), CONTROLLER_TYPE_PS5Controller, NULL },	// Sony DualSense Edge Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0163 ), CONTROLLER_TYPE_PS5Controller, NULL },	// HORI Fighting Commander OCTA
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0184 ), CONTROLLER_TYPE_PS5Controller, NULL },	// Hori Fighting Stick α

	{ MAKE_CONTROLLER_ID( 0x0079, 0x0006 ), CONTROLLER_TYPE_UnknownNonSteamController, NULL },	// DragonRise Generic USB PCB, sometimes configured as a PC Twin Shock Controller - looks like a DS3 but the face buttons are 1-4 instead of symbols

	{ MAKE_CONTROLLER_ID( 0x0079, 0x18d4 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// GPD Win 2 X-Box Controller
	{ MAKE_CONTROLLER_ID( 0x03eb, 0xff02 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Wooting Two
	{ MAKE_CONTROLLER_ID( 0x044f, 0xb326 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Thrustmaster Gamepad GP XID
	{ MAKE_CONTROLLER_ID( 0x045e, 0x028e ), CONTROLLER_TYPE_XBox360Controller, "Xbox 360 Controller" },	// Microsoft X-Box 360 pad
	{ MAKE_CONTROLLER_ID( 0x045e, 0x028f ), CONTROLLER_TYPE_XBox360Controller, "Xbox 360 Controller" },	// Microsoft X-Box 360 pad v2
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0291 ), CONTROLLER_TYPE_XBox360Controller, "Xbox 360 Wireless Controller" },	// Xbox 360 Wireless Receiver (XBOX)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02a0 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Microsoft X-Box 360 Big Button IR
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02a1 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Microsoft X-Box 360 Wireless Controller with XUSB driver on Windows
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02a9 ), CONTROLLER_TYPE_XBox360Controller, "Xbox 360 Wireless Controller" },	// Xbox 360 Wireless Receiver (third party knockoff)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0719 ), CONTROLLER_TYPE_XBox360Controller, "Xbox 360 Wireless Controller" },	// Xbox 360 Wireless Receiver
	{ MAKE_CONTROLLER_ID( 0x046d, 0xc21d ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Logitech Gamepad F310
	{ MAKE_CONTROLLER_ID( 0x046d, 0xc21e ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Logitech Gamepad F510
	{ MAKE_CONTROLLER_ID( 0x046d, 0xc21f ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Logitech Gamepad F710
	{ MAKE_CONTROLLER_ID( 0x046d, 0xc242 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Logitech Chillstream Controller
	{ MAKE_CONTROLLER_ID( 0x056e, 0x2004 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Elecom JC-U3613M
	{ MAKE_CONTROLLER_ID( 0x06a3, 0xf51a ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Saitek P3600
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4716 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Wired Xbox 360 Controller
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4718 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Street Fighter IV FightStick SE
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4726 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Xbox 360 Controller
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4728 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Street Fighter IV FightPad
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4736 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz MicroCon Gamepad
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4738 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Wired Xbox 360 Controller (SFIV)
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4740 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Beat Pad
	{ MAKE_CONTROLLER_ID( 0x0738, 0xb726 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Xbox controller - MW2
	{ MAKE_CONTROLLER_ID( 0x0738, 0xbeef ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz JOYTECH NEO SE Advanced GamePad
	{ MAKE_CONTROLLER_ID( 0x0738, 0xcb02 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Saitek Cyborg Rumble Pad - PC/Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0738, 0xcb03 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Saitek P3200 Rumble Pad - PC/Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0738, 0xf738 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Super SFIV FightStick TE S
	{ MAKE_CONTROLLER_ID( 0x0955, 0x7210 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Nvidia Shield local controller
	{ MAKE_CONTROLLER_ID( 0x0955, 0xb400 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// NVIDIA Shield streaming controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0105 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// HSM3 Xbox360 dancepad
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0113 ), CONTROLLER_TYPE_XBox360Controller, "PDP Xbox 360 Afterglow" },	// PDP Afterglow Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x011f ), CONTROLLER_TYPE_XBox360Controller, "PDP Xbox 360 Rock Candy" },	// PDP Rock Candy Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0125 ), CONTROLLER_TYPE_XBox360Controller, "PDP INJUSTICE FightStick" },	// PDP INJUSTICE FightStick for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0127 ), CONTROLLER_TYPE_XBox360Controller, "PDP INJUSTICE FightPad" },	// PDP INJUSTICE FightPad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0131 ), CONTROLLER_TYPE_XBox360Controller, "PDP EA Soccer Controller" },	// PDP EA Soccer Gamepad
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0133 ), CONTROLLER_TYPE_XBox360Controller, "PDP Battlefield 4 Controller" },	// PDP Battlefield 4 Gamepad
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0143 ), CONTROLLER_TYPE_XBox360Controller, "PDP MK X Fight Stick" },	// PDP MK X Fight Stick for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0147 ), CONTROLLER_TYPE_XBox360Controller, "PDP Xbox 360 Marvel Controller" },	// PDP Marvel Controller for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0201 ), CONTROLLER_TYPE_XBox360Controller, "PDP Xbox 360 Controller" },	// PDP Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0213 ), CONTROLLER_TYPE_XBox360Controller, "PDP Xbox 360 Afterglow" },	// PDP Afterglow Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x021f ), CONTROLLER_TYPE_XBox360Controller, "PDP Xbox 360 Rock Candy" },	// PDP Rock Candy Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0301 ), CONTROLLER_TYPE_XBox360Controller, "PDP Xbox 360 Controller" },	// PDP Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0313 ), CONTROLLER_TYPE_XBox360Controller, "PDP Xbox 360 Afterglow" },	// PDP Afterglow Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0314 ), CONTROLLER_TYPE_XBox360Controller, "PDP Xbox 360 Afterglow" },	// PDP Afterglow Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0401 ), CONTROLLER_TYPE_XBox360Controller, "PDP Xbox 360 Controller" },	// PDP Gamepad for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0413 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// PDP Afterglow AX.1 (unlisted)
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0501 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// PDP Xbox 360 Controller (unlisted)
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0xf900 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// PDP Afterglow AX.1 (unlisted)
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x000a ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Co. DOA4 FightStick
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x000c ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori PadEX Turbo
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x000d ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Fighting Stick EX2
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0016 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Real Arcade Pro.EX
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x001b ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Real Arcade Pro VX
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x008c ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Real Arcade Pro 4
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00db ), CONTROLLER_TYPE_XBox360Controller, "HORI Slime Controller" },	// Hori Dragon Quest Slime Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x011e ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Fighting Stick α
	{ MAKE_CONTROLLER_ID( 0x1038, 0x1430 ), CONTROLLER_TYPE_XBox360Controller, "SteelSeries Stratus Duo" },	// SteelSeries Stratus Duo
	{ MAKE_CONTROLLER_ID( 0x1038, 0x1431 ), CONTROLLER_TYPE_XBox360Controller, "SteelSeries Stratus Duo" },	// SteelSeries Stratus Duo
	{ MAKE_CONTROLLER_ID( 0x1038, 0xb360 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// SteelSeries Nimbus/Stratus XL
	{ MAKE_CONTROLLER_ID( 0x11c9, 0x55f0 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Nacon GC-100XF
	{ MAKE_CONTROLLER_ID( 0x12ab, 0x0004 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Honey Bee Xbox360 dancepad
	{ MAKE_CONTROLLER_ID( 0x12ab, 0x0301 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// PDP AFTERGLOW AX.1
	{ MAKE_CONTROLLER_ID( 0x12ab, 0x0303 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mortal Kombat Klassic FightStick
	{ MAKE_CONTROLLER_ID( 0x1430, 0x02a0 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// RedOctane Controller Adapter
	{ MAKE_CONTROLLER_ID( 0x1430, 0x4748 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// RedOctane Guitar Hero X-plorer
	{ MAKE_CONTROLLER_ID( 0x1430, 0xf801 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// RedOctane Controller
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0601 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// BigBen Interactive XBOX 360 Controller
//	{ MAKE_CONTROLLER_ID( 0x1532, 0x0037 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Razer Sabertooth
	{ MAKE_CONTROLLER_ID( 0x15e4, 0x3f00 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Power A Mini Pro Elite
	{ MAKE_CONTROLLER_ID( 0x15e4, 0x3f0a ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Xbox Airflo wired controller
	{ MAKE_CONTROLLER_ID( 0x15e4, 0x3f10 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Batarang Xbox 360 controller
	{ MAKE_CONTROLLER_ID( 0x162e, 0xbeef ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Joytech Neo-Se Take2
	{ MAKE_CONTROLLER_ID( 0x1689, 0xfd00 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Razer Onza Tournament Edition
	{ MAKE_CONTROLLER_ID( 0x1689, 0xfd01 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Razer Onza Classic Edition
	{ MAKE_CONTROLLER_ID( 0x1689, 0xfe00 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Razer Sabertooth
	{ MAKE_CONTROLLER_ID( 0x1949, 0x041a ), CONTROLLER_TYPE_XBox360Controller, "Amazon Luna Controller" },	// Amazon Luna Controller
	{ MAKE_CONTROLLER_ID( 0x1bad, 0x0002 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Harmonix Rock Band Guitar
	{ MAKE_CONTROLLER_ID( 0x1bad, 0x0003 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Harmonix Rock Band Drumkit
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf016 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Xbox 360 Controller
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf018 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Street Fighter IV SE Fighting Stick
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf019 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Brawlstick for Xbox 360
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf021 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Cats Ghost Recon FS GamePad
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf023 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// MLG Pro Circuit Controller (Xbox)
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf025 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Call Of Duty
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf027 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz FPS Pro
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf028 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Street Fighter IV FightPad
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf02e ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Fightpad
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf036 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz MicroCon GamePad Pro
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf038 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Street Fighter IV FightStick TE
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf039 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz MvC2 TE
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf03a ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz SFxT Fightstick Pro
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf03d ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Street Fighter IV Arcade Stick TE - Chun Li
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf03e ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz MLG FightStick TE
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf03f ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz FightStick SoulCaliber
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf042 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz FightStick TES+
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf080 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz FightStick TE2
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf501 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// HoriPad EX2 Turbo
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf502 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Real Arcade Pro.VX SA
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf503 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Fighting Stick VX
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf504 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Real Arcade Pro. EX
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf505 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Fighting Stick EX2B
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf506 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Real Arcade Pro.EX Premium VLX
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf900 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Harmonix Xbox 360 Controller
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf901 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Gamestop Xbox 360 Controller
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf902 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Mad Catz Gamepad2
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf903 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Tron Xbox 360 controller
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf904 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// PDP Versus Fighting Pad
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xf906 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// MortalKombat FightStick
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xfa01 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// MadCatz GamePad
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xfd00 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Razer Onza TE
	{ MAKE_CONTROLLER_ID( 0x1bad, 0xfd01 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Razer Onza
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5000 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Razer Atrox Arcade Stick
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5300 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// PowerA MINI PROEX Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5303 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Xbox Airflo wired controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x530a ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Xbox 360 Pro EX Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x531a ), CONTROLLER_TYPE_XBox360Controller, NULL },	// PowerA Pro Ex
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5397 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// FUS1ON Tournament Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5500 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori XBOX 360 EX 2 with Turbo
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5501 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Real Arcade Pro VX-SA
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5502 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Fighting Stick VX Alt
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5503 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Fighting Edge
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5506 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori SOULCALIBUR V Stick
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x550d ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori GEM Xbox controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x550e ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Real Arcade Pro V Kai 360
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5508 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori PAD A
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5510 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Hori Fighting Commander ONE
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5b00 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// ThrustMaster Ferrari Italia 458 Racing Wheel
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5b02 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Thrustmaster, Inc. GPX Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5b03 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Thrustmaster Ferrari 458 Racing Wheel
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x5d04 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Razer Sabertooth
	{ MAKE_CONTROLLER_ID( 0x24c6, 0xfafa ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Aplay Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0xfafb ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Aplay Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0xfafc ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Afterglow Gamepad 1
	{ MAKE_CONTROLLER_ID( 0x24c6, 0xfafd ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Afterglow Gamepad 3
	{ MAKE_CONTROLLER_ID( 0x24c6, 0xfafe ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Rock Candy Gamepad for Xbox 360

	{ MAKE_CONTROLLER_ID( 0x044f, 0xd012 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// ThrustMaster eSwap PRO Controller Xbox
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02d1 ), CONTROLLER_TYPE_XBoxOneController, "Xbox One Controller" },	// Microsoft X-Box One pad
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02dd ), CONTROLLER_TYPE_XBoxOneController, "Xbox One Controller" },	// Microsoft X-Box One pad (Firmware 2015)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02e0 ), CONTROLLER_TYPE_XBoxOneController, "Xbox One S Controller" },	// Microsoft X-Box One S pad (Bluetooth)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02e3 ), CONTROLLER_TYPE_XBoxOneController, "Xbox One Elite Controller" },	// Microsoft X-Box One Elite pad
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02ea ), CONTROLLER_TYPE_XBoxOneController, "Xbox One S Controller" },	// Microsoft X-Box One S pad
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02fd ), CONTROLLER_TYPE_XBoxOneController, "Xbox One S Controller" },	// Microsoft X-Box One S pad (Bluetooth)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02ff ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Microsoft X-Box One controller with XBOXGIP driver on Windows
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0b00 ), CONTROLLER_TYPE_XBoxOneController, "Xbox One Elite 2 Controller" },	// Microsoft X-Box One Elite Series 2 pad
//	{ MAKE_CONTROLLER_ID( 0x045e, 0x0b02 ), CONTROLLER_TYPE_XBoxOneController, "Xbox One Elite 2 Controller" },	// The virtual keyboard generated by XboxGip drivers for Xbox One Controllers (see https://github.com/libsdl-org/SDL/pull/5121 for details)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0b05 ), CONTROLLER_TYPE_XBoxOneController, "Xbox One Elite 2 Controller" },	// Microsoft X-Box One Elite Series 2 pad (Bluetooth)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0b0a ), CONTROLLER_TYPE_XBoxOneController, "Xbox Adaptive Controller" },	// Microsoft X-Box Adaptive pad
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0b0c ), CONTROLLER_TYPE_XBoxOneController, "Xbox Adaptive Controller" },	// Microsoft X-Box Adaptive pad (Bluetooth)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0b12 ), CONTROLLER_TYPE_XBoxOneController, "Xbox Series X Controller" },	// Microsoft X-Box Series X pad
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0b13 ), CONTROLLER_TYPE_XBoxOneController, "Xbox Series X Controller" },	// Microsoft X-Box Series X pad (BLE)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0b20 ), CONTROLLER_TYPE_XBoxOneController, "Xbox One S Controller" },	// Microsoft X-Box One S pad (BLE)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0b21 ), CONTROLLER_TYPE_XBoxOneController, "Xbox Adaptive Controller" },	// Microsoft X-Box Adaptive pad (BLE)
	{ MAKE_CONTROLLER_ID( 0x045e, 0x0b22 ), CONTROLLER_TYPE_XBoxOneController, "Xbox One Elite 2 Controller" },	// Microsoft X-Box One Elite Series 2 pad (BLE)
	{ MAKE_CONTROLLER_ID( 0x0738, 0x4a01 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Mad Catz FightStick TE 2
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0139 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Afterglow" },	// PDP Afterglow Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x013B ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Face-Off Controller" },	// PDP Face-Off Gamepad for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x013a ), CONTROLLER_TYPE_XBoxOneController, NULL },	// PDP Xbox One Controller (unlisted)
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0145 ), CONTROLLER_TYPE_XBoxOneController, "PDP MK X Fight Pad" },	// PDP MK X Fight Pad for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0146 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Rock Candy" },	// PDP Rock Candy Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x015b ), CONTROLLER_TYPE_XBoxOneController, "PDP Fallout 4 Vault Boy Controller" },	// PDP Fallout 4 Vault Boy Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x015c ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One @Play Controller" },	// PDP @Play Wired Controller for Xbox One 
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x015d ), CONTROLLER_TYPE_XBoxOneController, "PDP Mirror's Edge Controller" },	// PDP Mirror's Edge Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x015f ), CONTROLLER_TYPE_XBoxOneController, "PDP Metallic Controller" },	// PDP Metallic Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0160 ), CONTROLLER_TYPE_XBoxOneController, "PDP NFL Face-Off Controller" },	// PDP NFL Official Face-Off Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0161 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Camo" },	// PDP Camo Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0162 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Controller" },	// PDP Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0163 ), CONTROLLER_TYPE_XBoxOneController, "PDP Deliverer of Truth" },	// PDP Legendary Collection: Deliverer of Truth
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0164 ), CONTROLLER_TYPE_XBoxOneController, "PDP Battlefield 1 Controller" },	// PDP Battlefield 1 Official Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0165 ), CONTROLLER_TYPE_XBoxOneController, "PDP Titanfall 2 Controller" },	// PDP Titanfall 2 Official Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0166 ), CONTROLLER_TYPE_XBoxOneController, "PDP Mass Effect: Andromeda Controller" },	// PDP Mass Effect: Andromeda Official Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0167 ), CONTROLLER_TYPE_XBoxOneController, "PDP Halo Wars 2 Face-Off Controller" },	// PDP Halo Wars 2 Official Face-Off Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0205 ), CONTROLLER_TYPE_XBoxOneController, "PDP Victrix Pro Fight Stick" },	// PDP Victrix Pro Fight Stick
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0206 ), CONTROLLER_TYPE_XBoxOneController, "PDP Mortal Kombat Controller" },	// PDP Mortal Kombat 25 Anniversary Edition Stick (Xbox One)
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0246 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Rock Candy" },	// PDP Rock Candy Wired Controller for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0261 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Camo" },	// PDP Camo Wired Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0262 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Controller" },	// PDP Wired Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02a0 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Midnight Blue" },	// PDP Wired Controller for Xbox One - Midnight Blue
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02a1 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Verdant Green" },	// PDP Wired Controller for Xbox One - Verdant Green
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02a2 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Crimson Red" },	// PDP Wired Controller for Xbox One - Crimson Red
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02a3 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Arctic White" },	// PDP Wired Controller for Xbox One - Arctic White
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02a4 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Phantom Black" },	// PDP Wired Controller for Xbox One - Stealth Series | Phantom Black
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02a5 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Ghost White" },	// PDP Wired Controller for Xbox One - Stealth Series | Ghost White
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02a6 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Revenant Blue" },	// PDP Wired Controller for Xbox One - Stealth Series | Revenant Blue
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02a7 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Raven Black" },	// PDP Wired Controller for Xbox One - Raven Black
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02a8 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Arctic White" },	// PDP Wired Controller for Xbox One - Arctic White
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02a9 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Midnight Blue" },	// PDP Wired Controller for Xbox One - Midnight Blue
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02aa ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Verdant Green" },	// PDP Wired Controller for Xbox One - Verdant Green
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02ab ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Crimson Red" },	// PDP Wired Controller for Xbox One - Crimson Red
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02ac ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Ember Orange" },	// PDP Wired Controller for Xbox One - Ember Orange
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02ad ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Phantom Black" },	// PDP Wired Controller for Xbox One - Stealth Series | Phantom Black
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02ae ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Ghost White" },	// PDP Wired Controller for Xbox One - Stealth Series | Ghost White
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02af ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Revenant Blue" },	// PDP Wired Controller for Xbox One - Stealth Series | Revenant Blue
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02b0 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Raven Black" },	// PDP Wired Controller for Xbox One - Raven Black
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02b1 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Arctic White" },	// PDP Wired Controller for Xbox One - Arctic White
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02b3 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Afterglow" },	// PDP Afterglow Prismatic Wired Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02b5 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One GAMEware Controller" },	// PDP GAMEware Wired Controller Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02b6 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// PDP One-Handed Joystick Adaptive Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02bd ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Royal Purple" },	// PDP Wired Controller for Xbox One - Royal Purple
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02be ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Raven Black" },	// PDP Deluxe Wired Controller for Xbox One - Raven Black
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02bf ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Midnight Blue" },	// PDP Deluxe Wired Controller for Xbox One - Midnight Blue
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02c0 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Phantom Black" },	// PDP Deluxe Wired Controller for Xbox One - Stealth Series | Phantom Black
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02c1 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Ghost White" },	// PDP Deluxe Wired Controller for Xbox One - Stealth Series | Ghost White
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02c2 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Revenant Blue" },	// PDP Deluxe Wired Controller for Xbox One - Stealth Series | Revenant Blue
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02c3 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Verdant Green" },	// PDP Deluxe Wired Controller for Xbox One - Verdant Green
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02c4 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Ember Orange" },	// PDP Deluxe Wired Controller for Xbox One - Ember Orange
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02c5 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Royal Purple" },	// PDP Deluxe Wired Controller for Xbox One - Royal Purple
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02c6 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Crimson Red" },	// PDP Deluxe Wired Controller for Xbox One - Crimson Red
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02c7 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Arctic White" },	// PDP Deluxe Wired Controller for Xbox One - Arctic White
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02c8 ), CONTROLLER_TYPE_XBoxOneController, "PDP Kingdom Hearts Controller" },	// PDP Kingdom Hearts Wired Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02c9 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Phantasm Red" },	// PDP Deluxe Wired Controller for Xbox One - Stealth Series | Phantasm Red
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02ca ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Specter Violet" },	// PDP Deluxe Wired Controller for Xbox One - Stealth Series | Specter Violet
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02cb ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Specter Violet" },	// PDP Wired Controller for Xbox One - Stealth Series | Specter Violet
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02cd ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Blu-merang" },	// PDP Rock Candy Wired Controller for Xbox One - Blu-merang
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02ce ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Cranblast" },	// PDP Rock Candy Wired Controller for Xbox One - Cranblast
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02cf ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Aqualime" },	// PDP Rock Candy Wired Controller for Xbox One - Aqualime
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02d5 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One Red Camo" },	// PDP Wired Controller for Xbox One - Red Camo
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0346 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One RC Gamepad" },	// PDP RC Gamepad for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0446 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox One RC Gamepad" },	// PDP RC Gamepad for Xbox One
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02da ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox Series X Afterglow" },	// PDP Xbox Series X Afterglow
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02d6 ), CONTROLLER_TYPE_XBoxOneController, "Victrix Gambit Tournament Controller" },	// Victrix Gambit Tournament Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x02d9 ), CONTROLLER_TYPE_XBoxOneController, "PDP Xbox Series X Midnight Blue" },	// PDP Xbox Series X Midnight Blue
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0063 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Hori Real Arcade Pro Hayabusa (USA) Xbox One
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0067 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// HORIPAD ONE
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0078 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Hori Real Arcade Pro V Kai Xbox One
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00c5 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// HORI Fighting Commander
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0150 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// HORI Fighting Commander OCTA for Xbox Series X
	{ MAKE_CONTROLLER_ID( 0x1532, 0x0a00 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Razer Atrox Arcade Stick
	{ MAKE_CONTROLLER_ID( 0x1532, 0x0a03 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Razer Wildcat
	{ MAKE_CONTROLLER_ID( 0x1532, 0x0a14 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Razer Wolverine Ultimate
	{ MAKE_CONTROLLER_ID( 0x1532, 0x0a15 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Razer Wolverine Tournament Edition
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2001 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller - Black Inline
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2002 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller Gray/White Inline
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2003 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller Green Inline
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2004 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller Pink inline
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2005 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X Wired Controller Core - Black
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2006 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X Wired Controller Core - White
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2009 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller Red inline
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x200a ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller Blue inline
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x200b ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller Camo Metallic Red
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x200c ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller Camo Metallic Blue
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x200d ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller Seafoam Fade
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x200e ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller Midnight Blue
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x200f ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Soldier Green
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2011 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired - Metallic Ice
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2012 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X Cuphead EnWired Controller - Mugman
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2015 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller - Blue Hint
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2016 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller - Green Hint
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2017 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Cntroller - Arctic Camo
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2018 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller Arc Lightning
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x2019 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller Royal Purple
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x201a ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox Series X Controller" },       // PowerA Xbox Series X EnWired Controller Nebula
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x4001 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Fusion Pro 2 Controller" },	// PowerA Fusion Pro 2 Wired Controller (Xbox Series X style)
	{ MAKE_CONTROLLER_ID( 0x20d6, 0x4002 ), CONTROLLER_TYPE_XBoxOneController, "PowerA Spectra Infinity Controller" },	// PowerA Spectra Infinity Wired Controller (Xbox Series X style)
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x541a ), CONTROLLER_TYPE_XBoxOneController, NULL },	// PowerA Xbox One Mini Wired Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x542a ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Xbox ONE spectra
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x543a ), CONTROLLER_TYPE_XBoxOneController, "PowerA Xbox One Controller" },	// PowerA Xbox ONE liquid metal controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x551a ), CONTROLLER_TYPE_XBoxOneController, NULL },	// PowerA FUSION Pro Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x561a ), CONTROLLER_TYPE_XBoxOneController, NULL },	// PowerA FUSION Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x581a ), CONTROLLER_TYPE_XBoxOneController, NULL },	// BDA XB1 Classic Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x591a ), CONTROLLER_TYPE_XBoxOneController, NULL },	// PowerA FUSION Pro Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x592a ), CONTROLLER_TYPE_XBoxOneController, NULL },	// BDA XB1 Spectra Pro
	{ MAKE_CONTROLLER_ID( 0x24c6, 0x791a ), CONTROLLER_TYPE_XBoxOneController, NULL },	// PowerA Fusion Fight Pad
	{ MAKE_CONTROLLER_ID( 0x2dc8, 0x2002 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// 8BitDo Ultimate Wired Controller for Xbox
	{ MAKE_CONTROLLER_ID( 0x2e24, 0x0652 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Hyperkin Duke
	{ MAKE_CONTROLLER_ID( 0x2e24, 0x1618 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Hyperkin Duke
	{ MAKE_CONTROLLER_ID( 0x2e24, 0x1688 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Hyperkin X91
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0611 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Xbox Controller Mode for NACON Revolution 3

	// These have been added via Minidump for unrecognized Xinput controller assert
	{ MAKE_CONTROLLER_ID( 0x0000, 0x0000 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x045e, 0x02a2 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller - Microsoft VID
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x1414 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0159 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x24c6, 0xfaff ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x006d ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00a4 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x1832 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x187f ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x1883 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller	
	{ MAKE_CONTROLLER_ID( 0x03eb, 0xff01 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0c12, 0x0ef8 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Homemade fightstick based on brook pcb (with XInput driver??)
	{ MAKE_CONTROLLER_ID( 0x046d, 0x1000 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x1345, 0x6006 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller

	{ MAKE_CONTROLLER_ID( 0x056e, 0x2012 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x146b, 0x0602 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00ae ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x046d, 0x0401 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// logitech xinput
	{ MAKE_CONTROLLER_ID( 0x046d, 0x0301 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// logitech xinput
	{ MAKE_CONTROLLER_ID( 0x046d, 0xcaa3 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// logitech xinput
	{ MAKE_CONTROLLER_ID( 0x046d, 0xc261 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// logitech xinput
	{ MAKE_CONTROLLER_ID( 0x046d, 0x0291 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// logitech xinput
	{ MAKE_CONTROLLER_ID( 0x0079, 0x18d3 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00b1 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0001, 0x0001 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x188e ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x187c ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x189c ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x0079, 0x1874 ), CONTROLLER_TYPE_XBox360Controller, NULL },	// Unknown Controller

	{ MAKE_CONTROLLER_ID( 0x2f24, 0x0050 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x2f24, 0x2e ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x9886, 0x24 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x2f24, 0x91 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x1430, 0x719 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xf0d, 0xed ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xf0d, 0xc0 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f, 0x152 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f, 0x2a7 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x46d, 0x1007 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f, 0x2b8 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f, 0x2a8 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x79, 0x18a1 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller

	/* Added from Minidumps 10-9-19 */
	{ MAKE_CONTROLLER_ID( 0x0,		0x6686 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x11ff,	0x511 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x12ab,	0x304 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x1430,	0x291 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x1430,	0x2a9 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x1430,	0x70b ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x1bad,	0x28e ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x1bad,	0x2a0 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x1bad,	0x5500 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x20ab,	0x55ef ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x24c6,	0x5509 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x2516,	0x69 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x25b1,	0x360 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x2c22,	0x2203 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x2f24,	0x11 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x2f24,	0x53 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x2f24,	0xb7 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x46d,	0x0 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x46d,	0x1004 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x46d,	0x1008 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x46d,	0xf301 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x738,	0x2a0 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x738,	0x7263 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x738,	0xb738 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x738,	0xcb29 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x738,	0xf401 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x79,		0x18c2 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x79,		0x18c8 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x79,		0x18cf ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xc12,	0xe17 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xc12,	0xe1c ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xc12,	0xe22 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xc12,	0xe30 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xd2d2,	0xd2d2 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xd62,	0x9a1a ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xd62,	0x9a1b ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe00,	0xe00 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f,	0x12a ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f,	0x2a1 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f,	0x2a2 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f,	0x2a5 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f,	0x2b2 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f,	0x2bd ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f,	0x2bf ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f,	0x2c0 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f,	0x2c6 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xf0d,	0x97 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xf0d,	0xba ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xf0d,	0xd8 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xfff,	0x2a1 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x45e,	0x867 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	// Added 12-17-2020
	{ MAKE_CONTROLLER_ID( 0x16d0,	0xf3f ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0x2f24,	0x8f ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
	{ MAKE_CONTROLLER_ID( 0xe6f,	0xf501 ), CONTROLLER_TYPE_XBoxOneController, NULL },	// Unknown Controller
																							
	//{ MAKE_CONTROLLER_ID( 0x1949, 0x0402 ), /*android*/, NULL },	// Unknown Controller

	{ MAKE_CONTROLLER_ID( 0x05ac, 0x0001 ), CONTROLLER_TYPE_AppleController, NULL },	// MFI Extended Gamepad (generic entry for iOS/tvOS)
	{ MAKE_CONTROLLER_ID( 0x05ac, 0x0002 ), CONTROLLER_TYPE_AppleController, NULL },	// MFI Standard Gamepad (generic entry for iOS/tvOS)

    { MAKE_CONTROLLER_ID( 0x057e, 0x2006 ), CONTROLLER_TYPE_SwitchJoyConLeft, NULL },    // Nintendo Switch Joy-Con (Left)
    { MAKE_CONTROLLER_ID( 0x057e, 0x2007 ), CONTROLLER_TYPE_SwitchJoyConRight, NULL },   // Nintendo Switch Joy-Con (Right)
    { MAKE_CONTROLLER_ID( 0x057e, 0x2008 ), CONTROLLER_TYPE_SwitchJoyConPair, NULL },    // Nintendo Switch Joy-Con (Left+Right Combined)

    // This same controller ID is spoofed by many 3rd-party Switch controllers.
    // The ones we currently know of are:
    // * Any 8bitdo controller with Switch support
    // * ORTZ Gaming Wireless Pro Controller
    // * ZhiXu Gamepad Wireless
    // * Sunwaytek Wireless Motion Controller for Nintendo Switch
	{ MAKE_CONTROLLER_ID( 0x057e, 0x2009 ), CONTROLLER_TYPE_SwitchProController, NULL },        // Nintendo Switch Pro Controller
    //{ MAKE_CONTROLLER_ID( 0x057e, 0x2017 ), CONTROLLER_TYPE_SwitchProController, NULL },        // Nintendo Online SNES Controller
    //{ MAKE_CONTROLLER_ID( 0x057e, 0x2019 ), CONTROLLER_TYPE_SwitchProController, NULL },        // Nintendo Online N64 Controller
    //{ MAKE_CONTROLLER_ID( 0x057e, 0x201e ), CONTROLLER_TYPE_SwitchProController, NULL },        // Nintendo Online SEGA Genesis Controller

	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00c1 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // HORIPAD for Nintendo Switch
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x0092 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // HORI Pokken Tournament DX Pro Pad
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00f6 ), CONTROLLER_TYPE_SwitchProController, NULL },		// HORI Wireless Switch Pad
	// The HORIPAD S, which comes in multiple styles:
	// - NSW-108, classic GameCube controller
	// - NSW-244, Fighting Commander arcade pad
	// - NSW-278, Hori Pad Mini gamepad
	// - NSW-326, HORIPAD FPS for Nintendo Switch
	//
	// The first two, at least, shouldn't have their buttons remapped, and since we
	// can't tell which model we're actually using, we won't do any button remapping
	// for any of them.
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00dc ), CONTROLLER_TYPE_XInputSwitchController, NULL },	 // HORIPAD S - Looks like a Switch controller but uses the Xbox 360 controller protocol
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0180 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // PDP Faceoff Wired Pro Controller for Nintendo Switch
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0181 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // PDP Faceoff Deluxe Wired Pro Controller for Nintendo Switch
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0184 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // PDP Faceoff Wired Deluxe+ Audio Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0185 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // PDP Wired Fight Pad Pro for Nintendo Switch
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0186 ), CONTROLLER_TYPE_SwitchProController, NULL },        // PDP Afterglow Wireless Switch Controller - working gyro. USB is for charging only. Many later "Wireless" line devices w/ gyro also use this vid/pid
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0187 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // PDP Rockcandy Wired Controller
	{ MAKE_CONTROLLER_ID( 0x0e6f, 0x0188 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // PDP Afterglow Wired Deluxe+ Audio Controller
	{ MAKE_CONTROLLER_ID( 0x0f0d, 0x00aa ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // HORI Real Arcade Pro V Hayabusa in Switch Mode
	{ MAKE_CONTROLLER_ID( 0x20d6, 0xa711 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // PowerA Wired Controller Plus/PowerA Wired Controller Nintendo GameCube Style
	{ MAKE_CONTROLLER_ID( 0x20d6, 0xa712 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // PowerA Nintendo Switch Fusion Fight Pad
	{ MAKE_CONTROLLER_ID( 0x20d6, 0xa713 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // PowerA Super Mario Controller
	{ MAKE_CONTROLLER_ID( 0x20d6, 0xa714 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // PowerA Nintendo Switch Spectra Controller
	{ MAKE_CONTROLLER_ID( 0x20d6, 0xa715 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // Power A Fusion Wireless Arcade Stick (USB Mode) Over BT is shows up as 057e 2009
	{ MAKE_CONTROLLER_ID( 0x20d6, 0xa716 ), CONTROLLER_TYPE_SwitchInputOnlyController, NULL },  // PowerA Nintendo Switch Fusion Pro Controller - USB requires toggling switch on back of device

	// Valve products
	{ MAKE_CONTROLLER_ID( 0x0000, 0x11fb ), CONTROLLER_TYPE_MobileTouch, NULL },	// Streaming mobile touch virtual controls
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1101 ), CONTROLLER_TYPE_SteamController, NULL },	// Valve Legacy Steam Controller (CHELL)
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1102 ), CONTROLLER_TYPE_SteamController, NULL },	// Valve wired Steam Controller (D0G)
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1105 ), CONTROLLER_TYPE_SteamController, NULL },	// Valve Bluetooth Steam Controller (D0G)
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1106 ), CONTROLLER_TYPE_SteamController, NULL },	// Valve Bluetooth Steam Controller (D0G)
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1142 ), CONTROLLER_TYPE_SteamController, NULL },	// Valve wireless Steam Controller
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1201 ), CONTROLLER_TYPE_SteamControllerV2, NULL },	// Valve wired Steam Controller (HEADCRAB)
	{ MAKE_CONTROLLER_ID( 0x28de, 0x1202 ), CONTROLLER_TYPE_SteamControllerV2, NULL },	// Valve Bluetooth Steam Controller (HEADCRAB)

	// Bluepad32 addons from here:

	// OUYA
	{ MAKE_CONTROLLER_ID(0x2836, 0x0001), CONTROLLER_TYPE_OUYAController, NULL},  // OUYA 1st Controller

	// ION iCade
	{ MAKE_CONTROLLER_ID(0x15e4, 0x0132), CONTROLLER_TYPE_iCadeController, NULL},  // ION iCade
	{ MAKE_CONTROLLER_ID(0x0a5c, 0x8502), CONTROLLER_TYPE_iCadeController, NULL},  // iCade 8-bitty

	// Android
	{ MAKE_CONTROLLER_ID(0x20d6, 0x6271), CONTROLLER_TYPE_AndroidController, NULL},  // MOGA Controller, using HID mode
	{ MAKE_CONTROLLER_ID(0x0b05, 0x4500), CONTROLLER_TYPE_AndroidController, NULL},  // Asus Controller
	{ MAKE_CONTROLLER_ID(0x1949, 0x0402), CONTROLLER_TYPE_AndroidController, NULL},  // Amazon Fire gamepad Controller 1st gen
	{ MAKE_CONTROLLER_ID(0x18d1, 0x9400), CONTROLLER_TYPE_AndroidController, NULL},  // Stadia BLE mode

	// Smart TV remotes
	{ MAKE_CONTROLLER_ID(0x1949, 0x0401), CONTROLLER_TYPE_SmartTVRemoteController, NULL},  // Amazon Fire TV remote Controlelr 1st gen

	// 8-Bitdo controllers
	{ MAKE_CONTROLLER_ID(0x2820, 0x0009), CONTROLLER_TYPE_8BitdoController, NULL},  // 8Bitdo NES30 Gamepro
	{ MAKE_CONTROLLER_ID(0x2dc8, 0x0651), CONTROLLER_TYPE_8BitdoController, NULL},  // 8Bitdo M30
	{ MAKE_CONTROLLER_ID(0x2dc8, 0x2830), CONTROLLER_TYPE_8BitdoController, NULL},  // 8Bitdo SFC30
	{ MAKE_CONTROLLER_ID(0x2dc8, 0x2840), CONTROLLER_TYPE_8BitdoController, NULL},  // 8Bitdo SNES30
	{ MAKE_CONTROLLER_ID(0x2dc8, 0x3230), CONTROLLER_TYPE_8BitdoController, NULL},  // 8Bitdo Zero 2
	{ MAKE_CONTROLLER_ID(0x2dc8, 0x6100), CONTROLLER_TYPE_8BitdoController, NULL},  // 8Bitdo SF30 Pro
	{ MAKE_CONTROLLER_ID(0x2dc8, 0x6101), CONTROLLER_TYPE_8BitdoController, NULL},  // 8Bitdo SN30 Pro

	// Generic gamepad
	{ MAKE_CONTROLLER_ID(0x0a5c, 0x4502), CONTROLLER_TYPE_GenericController, NULL},  // White-label mini gamepad received as gift in conference

	// SteelSeries
	{ MAKE_CONTROLLER_ID(0x0111, 0x1420), CONTROLLER_TYPE_NimbusController, NULL},   // SteelSeries Nimbus
	{ MAKE_CONTROLLER_ID(0x0111, 0x1431), CONTROLLER_TYPE_AndroidController, NULL},  // SteelSeries Stratus Duo (Bluetooth)

	// Nintendo
	{ MAKE_CONTROLLER_ID(0x057e, 0x0330), CONTROLLER_TYPE_WiiController, NULL},        // Nintendo Wii U Pro
	{ MAKE_CONTROLLER_ID(0x057e, 0x0306), CONTROLLER_TYPE_WiiController, NULL},        // Nintendo Wii Remote
    { MAKE_CONTROLLER_ID(0x057e, 0x2017), CONTROLLER_TYPE_SwitchProController, NULL }, // Nintendo Online SNES Controller
    { MAKE_CONTROLLER_ID(0x057e, 0x2019), CONTROLLER_TYPE_SwitchProController, NULL }, // Nintendo Online N64 Controller
    { MAKE_CONTROLLER_ID(0x057e, 0x201e), CONTROLLER_TYPE_SwitchProController, NULL }, // Nintendo Online SEGA Genesis Controller


	// Bluepad32 addons to here.
};
// clang-format on

static inline uni_controller_type_t guess_controller_type(uint16_t nVID, uint16_t nPID) {
    uint32_t device_id = MAKE_CONTROLLER_ID(nVID, nPID);
    for (uint32_t i = 0; i < sizeof(arrControllers) / sizeof(arrControllers[0]); ++i) {
        if (device_id == arrControllers[i].device_id) {
            return arrControllers[i].controller_type;
        }
    }
#undef MAKE_CONTROLLER_ID
    return CONTROLLER_TYPE_Unknown;
}

#endif  // UNI_HID_DEVICE_VENDORS_H
