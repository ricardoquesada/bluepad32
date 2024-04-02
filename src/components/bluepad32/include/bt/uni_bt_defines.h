// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_DEFINES_H
#define UNI_BT_DEFINES_H

#include "uni_common.h"

// Bluetooth constants that are not defined in BTStack.

// Class of Device constants taken from:
// https://btprodspecificationrefs.blob.core.windows.net/assigned-numbers/Assigned%20Number%20Types/Baseband.pdf
// clang-format off
#define UNI_BT_COD_MAJOR_MASK               0b0001111100000000  // 0x1f00
#define UNI_BT_COD_MAJOR_PERIPHERAL         0b0000010100000000  // 0x0500
#define UNI_BT_COD_MAJOR_AUDIO_VIDEO        0b0000010000000000  // 0x0400

// Bits 0 and 1 must always be 0
#define UNI_BT_COD_MINOR_MASK               0b11111100
#define UNI_BT_COD_MINOR_KEYBOARD_AND_MICE  0b11000000
#define UNI_BT_COD_MINOR_MICE               0b10000000
#define UNI_BT_COD_MINOR_KEYBOARD           0b01000000

#define UNI_BT_COD_MINOR_SENSING_DEVICE     0b00010000
#define UNI_BT_COD_MINOR_REMOTE_CONTROL     0b00001100
#define UNI_BT_COD_MINOR_GAMEPAD            0b00001000
#define UNI_BT_COD_MINOR_JOYSTICK           0b00000100
// clang-format on

// Taken from Bluetooth Appearance Values:
// https://specificationrefs.bluetooth.com/assigned-values/Appearance%20Values.pdf
// clang-format off
#define UNI_BT_HID_APPEARANCE_GENERIC       0x3c0
#define UNI_BT_HID_APPEARANCE_KEYBOARD      0x3c1
#define UNI_BT_HID_APPEARANCE_MOUSE         0x3c2
#define UNI_BT_HID_APPEARANCE_JOYSTICK      0x3c3
#define UNI_BT_HID_APPEARANCE_GAMEPAD       0x3c4
// clang-format on

// Parameters used in different Bluetooth functions
#define UNI_BT_L2CAP_CHANNEL_MTU 0xffff  // Max MTU. DualShock requires at least a 79-byte packet

// Delicate balance between inquiry + pause.
// If the interval is too short, some devices won't get discovered (e.g: 8BitDo SN30 Pro in dinput/xinput modes)
// If the interval is too big, some devices won't be able to re-connect (e.g: Wii Remotes)
// These are the default values, but that be overridden from the console.
#define UNI_BT_MAX_PERIODIC_LENGTH 5  // In 1.28s unit
#define UNI_BT_MIN_PERIODIC_LENGTH 4  // In 1.28s unit
#define UNI_BT_INQUIRY_LENGTH 3       // In 1.28s unit

// Taken from 7.1.19 Remote Name Request Command
#define UNI_BT_CLOCK_OFFSET_VALID BIT(15)

#endif /* UNI_BT_DEFINES_H */
