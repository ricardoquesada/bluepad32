/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2022 Ricardo Quesada

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
****************************************************************************/

#ifndef UNI_BT_DEFINES_H
#define UNI_BT_DEFINES_H

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

#endif /* UNI_BT_DEFINES_H */
