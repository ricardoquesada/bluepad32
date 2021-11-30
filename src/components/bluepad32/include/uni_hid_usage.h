/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2019 Ricardo Quesada

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

// USB HID Usages table (Bluetooth uses the same table):
// https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf

#ifndef HID_USAGE_H
#define HID_USAGE_H

// clang-format off

// HID usage pages
#define HID_USAGE_PAGE_GENERIC_DESKTOP          0x01
#define HID_USAGE_PAGE_SIMULATION_CONTROLS      0x02
#define HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS  0x06
#define HID_USAGE_PAGE_KEYBOARD_KEYPAD          0x07
#define HID_USAGE_PAGE_BUTTON                   0x09
#define HID_USAGE_PAGE_CONSUMER                 0x0C

// HID usages for Generic Desktop Page
#define HID_USAGE_X                             0x30
#define HID_USAGE_Y                             0x31
#define HID_USAGE_Z                             0X32
#define HID_USAGE_RX                            0x33
#define HID_USAGE_RY                            0x34
#define HID_USAGE_RZ                            0x35
#define HID_USAGE_HAT                           0x39
#define HID_USAGE_SYSTEM_MAIN_MENU              0x85
#define HID_USAGE_DPAD_UP                       0x90
#define HID_USAGE_DPAD_DOWN                     0x91
#define HID_USAGE_DPAD_RIGHT                    0x92
#define HID_USAGE_DPAD_LEFT                     0x93

// HID usages for Simulation Controls Page
#define HID_USAGE_ACCELERATOR                   0xc4
#define HID_USAGE_BRAKE                         0xc5

// HID usages for Generic Device Controls Page
#define HID_USAGE_BATTERY_STRENGHT              0x20

// HID usages for Keyboard / Keypad Page

// HID usages for Consumer Page
#define HID_USAGE_MENU                          0x40
#define HID_USAGE_FAST_FORWARD                  0xb3
#define HID_USAGE_REWIND                        0xb4
#define HID_USAGE_PLAY_PAUSE                    0xcd
#define HID_USAGE_AC_SEARCH                     0x0221
#define HID_USAGE_AC_HOME                       0x0223
#define HID_USAGE_AC_BACK                       0x0224

// clang-format on

#endif  // HID_USAGE_H