// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

// USB HID Usages table (Bluetooth uses the same table):
// https://www.usb.org/sites/default/files/hut1_5.pdf

#ifndef HID_USAGE_H
#define HID_USAGE_H

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off

// HID usage pages
#define HID_USAGE_PAGE_GENERIC_DESKTOP          0x01
#define HID_USAGE_PAGE_SIMULATION_CONTROLS      0x02
#define HID_USAGE_PAGE_GENERIC_DEVICE_CONTROLS  0x06
#define HID_USAGE_PAGE_KEYBOARD_KEYPAD          0x07
#define HID_USAGE_PAGE_BUTTON                   0x09
#define HID_USAGE_PAGE_CONSUMER                 0x0c
#define HID_USAGE_PAGE_DIGITIZER                0x0d

// HID usages for Generic Desktop Page
#define HID_USAGE_AXIS_X                        0x30
#define HID_USAGE_AXIS_Y                        0x31
#define HID_USAGE_AXIS_Z                        0x32
#define HID_USAGE_AXIS_RX                       0x33
#define HID_USAGE_AXIS_RY                       0x34
#define HID_USAGE_AXIS_RZ                       0x35
#define HID_USAGE_WHEEL                         0x38
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
#define HID_USAGE_BATTERY_STRENGTH              0x20

// HID usages for Keyboard / Keypad Page
// KB=Keyboard, KP=Keypad
#define HID_USAGE_KB_NONE                       0x00
#define HID_USAGE_KB_ERROR_ROLL_OVER            0x01
#define HID_USAGE_KB_POST_FAIL                  0x02
#define HID_USAGE_KB_ERROR_UNDEFINED            0x03
#define HID_USAGE_KB_A                          0x04
#define HID_USAGE_KB_B                          0x05
#define HID_USAGE_KB_C                          0x06
#define HID_USAGE_KB_D                          0x07
#define HID_USAGE_KB_E                          0x08
#define HID_USAGE_KB_F                          0x09
#define HID_USAGE_KB_G                          0x0A
#define HID_USAGE_KB_H                          0x0B
#define HID_USAGE_KB_I                          0x0C
#define HID_USAGE_KB_J                          0x0D
#define HID_USAGE_KB_K                          0x0E
#define HID_USAGE_KB_L                          0x0F
#define HID_USAGE_KB_M                          0x10
#define HID_USAGE_KB_N                          0x11
#define HID_USAGE_KB_O                          0x12
#define HID_USAGE_KB_P                          0x13
#define HID_USAGE_KB_Q                          0x14
#define HID_USAGE_KB_R                          0x15
#define HID_USAGE_KB_S                          0x16
#define HID_USAGE_KB_T                          0x17
#define HID_USAGE_KB_U                          0x18
#define HID_USAGE_KB_V                          0x19
#define HID_USAGE_KB_W                          0x1A
#define HID_USAGE_KB_X                          0x1B
#define HID_USAGE_KB_Y                          0x1C
#define HID_USAGE_KB_Z                          0x1D
#define HID_USAGE_KB_1_EXCLAMATION_MARK         0x1E
#define HID_USAGE_KB_2_AT                       0x1F
#define HID_USAGE_KB_3_NUMBER_SIGN              0x20
#define HID_USAGE_KB_4_DOLLAR                   0x21
#define HID_USAGE_KB_5_PERCENT                  0x22
#define HID_USAGE_KB_6_CARET                    0x23
#define HID_USAGE_KB_7_AMPERSAND                0x24
#define HID_USAGE_KB_8_ASTERISK                 0x25
#define HID_USAGE_KB_9_OPARENTHESIS             0x26
#define HID_USAGE_KB_0_CPARENTHESIS             0x27
#define HID_USAGE_KB_ENTER                      0x28
#define HID_USAGE_KB_ESCAPE                     0x29
#define HID_USAGE_KB_BACKSPACE                  0x2A
#define HID_USAGE_KB_TAB                        0x2B
#define HID_USAGE_KB_SPACEBAR                   0x2C
#define HID_USAGE_KB_MINUS_UNDERSCORE           0x2D
#define HID_USAGE_KB_EQUAL_PLUS                 0x2E
#define HID_USAGE_KB_OBRACKET_OBRACE            0x2F
#define HID_USAGE_KB_CBRACKET_CBRACE            0x30
#define HID_USAGE_KB_BACKSLASH_VERTICAL_BAR     0x31
#define HID_USAGE_KB_NONUS_NUMBER_SIGN_TILDE    0x32
#define HID_USAGE_KB_SEMICOLON_COLON            0x33
#define HID_USAGE_KB_SINGLE_DOUBLE_QUOTE        0x34
#define HID_USAGE_KB_GRAVE_ACCENT_TILDE         0x35
#define HID_USAGE_KB_COMMA_LESS                 0x36
#define HID_USAGE_KB_DOT_GREATER                0x37
#define HID_USAGE_KB_SLASH_QUESTION             0x38
#define HID_USAGE_KB_CAPS_LOCK                  0x39
#define HID_USAGE_KB_F1                         0x3A
#define HID_USAGE_KB_F2                         0x3B
#define HID_USAGE_KB_F3                         0x3C
#define HID_USAGE_KB_F4                         0x3D
#define HID_USAGE_KB_F5                         0x3E
#define HID_USAGE_KB_F6                         0x3F
#define HID_USAGE_KB_F7                         0x40
#define HID_USAGE_KB_F8                         0x41
#define HID_USAGE_KB_F9                         0x42
#define HID_USAGE_KB_F10                        0x43
#define HID_USAGE_KB_F11                        0x44
#define HID_USAGE_KB_F12                        0x45
#define HID_USAGE_KB_PRINT_SCREEN               0x46
#define HID_USAGE_KB_SCROLL_LOCK                0x47
#define HID_USAGE_KB_PAUSE                      0x48
#define HID_USAGE_KB_INSERT                     0x49
#define HID_USAGE_KB_HOME                       0x4A
#define HID_USAGE_KB_PAGE_UP                    0x4B
#define HID_USAGE_KB_DELETE                     0x4C
#define HID_USAGE_KB_END                        0x4D
#define HID_USAGE_KB_PAGE_DOWN                  0x4E
#define HID_USAGE_KB_RIGHT_ARROW                0x4F
#define HID_USAGE_KB_LEFT_ARROW                 0x50
#define HID_USAGE_KB_DOWN_ARROW                 0x51
#define HID_USAGE_KB_UP_ARROW                   0x52
#define HID_USAGE_KP_NUM_LOCK_CLEAR             0x53
#define HID_USAGE_KP_SLASH                      0x54
#define HID_USAGE_KP_ASTERISK                   0x55
#define HID_USAGE_KP_MINUS                      0x56
#define HID_USAGE_KP_PLUS                       0x57
#define HID_USAGE_KP_ENTER                      0x58
#define HID_USAGE_KP_1_END                      0x59
#define HID_USAGE_KP_2_DOWN_ARROW               0x5A
#define HID_USAGE_KP_3_PAGEDN                   0x5B
#define HID_USAGE_KP_4_LEFT_ARROW               0x5C
#define HID_USAGE_KP_5                          0x5D
#define HID_USAGE_KP_6_RIGHT_ARROW              0x5E
#define HID_USAGE_KP_7_HOME                     0x5F
#define HID_USAGE_KP_8_UP_ARROW                 0x60
#define HID_USAGE_KP_9_PAGEUP                   0x61
#define HID_USAGE_KP_0_INSERT                   0x62
#define HID_USAGE_KP_DOT_DELETE                 0x63
#define HID_USAGE_KB_NONUS_BACK_SLASH_VERTICAL_BAR      0x64
#define HID_USAGE_KB_APPLICATION                0x65
#define HID_USAGE_KB_POWER                      0x66
#define HID_USAGE_KP_EQUAL                      0x67
#define HID_USAGE_KB_F13                        0x68
#define HID_USAGE_KB_F14                        0x69
#define HID_USAGE_KB_F15                        0x6A
#define HID_USAGE_KB_F16                        0x6B
#define HID_USAGE_KB_F17                        0x6C
#define HID_USAGE_KB_F18                        0x6D
#define HID_USAGE_KB_F19                        0x6E
#define HID_USAGE_KB_F20                        0x6F
#define HID_USAGE_KB_F21                        0x70
#define HID_USAGE_KB_F22                        0x71
#define HID_USAGE_KB_F23                        0x72
#define HID_USAGE_KB_F24                        0x73
#define HID_USAGE_KB_EXECUTE                    0x74
#define HID_USAGE_KB_HELP                       0x75
#define HID_USAGE_KB_MENU                       0x76
#define HID_USAGE_KB_SELECT                     0x77
#define HID_USAGE_KB_STOP                       0x78
#define HID_USAGE_KB_AGAIN                      0x79
#define HID_USAGE_KB_UNDO                       0x7A
#define HID_USAGE_KB_CUT                        0x7B
#define HID_USAGE_KB_COPY                       0x7C
#define HID_USAGE_KB_PASTE                      0x7D
#define HID_USAGE_KB_FIND                       0x7E
#define HID_USAGE_KB_MUTE                       0x7F
#define HID_USAGE_KB_VOLUME_UP                  0x80
#define HID_USAGE_KB_VOLUME_DOWN                0x81
#define HID_USAGE_KB_LOCKING_CAPS_LOCK          0x82
#define HID_USAGE_KB_LOCKING_NUM_LOCK           0x83
#define HID_USAGE_KB_LOCKING_SCROLL_LOCK        0x84
#define HID_USAGE_KP_COMMA                      0x85
#define HID_USAGE_KP_EQUAL_SIGN                 0x86
#define HID_USAGE_KB_INTERNATIONAL1             0x87
#define HID_USAGE_KB_INTERNATIONAL2             0x88
#define HID_USAGE_KB_INTERNATIONAL3             0x89
#define HID_USAGE_KB_INTERNATIONAL4             0x8A
#define HID_USAGE_KB_INTERNATIONAL5             0x8B
#define HID_USAGE_KB_INTERNATIONAL6             0x8C
#define HID_USAGE_KB_INTERNATIONAL7             0x8D
#define HID_USAGE_KB_INTERNATIONAL8             0x8E
#define HID_USAGE_KB_INTERNATIONAL9             0x8F
#define HID_USAGE_KB_LANG1                      0x90
#define HID_USAGE_KB_LANG2                      0x91
#define HID_USAGE_KB_LANG3                      0x92
#define HID_USAGE_KB_LANG4                      0x93
#define HID_USAGE_KB_LANG5                      0x94
#define HID_USAGE_KB_LANG6                      0x95
#define HID_USAGE_KB_LANG7                      0x96
#define HID_USAGE_KB_LANG8                      0x97
#define HID_USAGE_KB_LANG9                      0x98
#define HID_USAGE_KB_ALTERNATE_ERASE            0x99
#define HID_USAGE_KB_SYSREQ                     0x9A
#define HID_USAGE_KB_CANCEL                     0x9B
#define HID_USAGE_KB_CLEAR                      0x9C
#define HID_USAGE_KB_PRIOR                      0x9D
#define HID_USAGE_KB_RETURN                     0x9E
#define HID_USAGE_KB_SEPARATOR                  0x9F
#define HID_USAGE_KB_OUT                        0xA0
#define HID_USAGE_KB_OPER                       0xA1
#define HID_USAGE_KB_CLEAR_AGAIN                0xA2
#define HID_USAGE_KB_CRSEL                      0xA3
#define HID_USAGE_KB_EXSEL                      0xA4
#define HID_USAGE_KP_00                         0xB0
#define HID_USAGE_KP_000                        0xB1
#define HID_USAGE_KB_THOUSANDS_SEPARATOR        0xB2
#define HID_USAGE_KB_DECIMAL_SEPARATOR          0xB3
#define HID_USAGE_KB_CURRENCY_UNIT              0xB4
#define HID_USAGE_KB_CURRENCY_SUB_UNIT          0xB5
#define HID_USAGE_KP_OPARENTHESIS               0xB6
#define HID_USAGE_KP_CPARENTHESIS               0xB7
#define HID_USAGE_KP_OBRACE                     0xB8
#define HID_USAGE_KP_CBRACE                     0xB9
#define HID_USAGE_KP_TAB                        0xBA
#define HID_USAGE_KP_BACKSPACE                  0xBB
#define HID_USAGE_KP_A                          0xBC
#define HID_USAGE_KP_B                          0xBD
#define HID_USAGE_KP_C                          0xBE
#define HID_USAGE_KP_D                          0xBF
#define HID_USAGE_KP_E                          0xC0
#define HID_USAGE_KP_F                          0xC1
#define HID_USAGE_KP_XOR                        0xC2
#define HID_USAGE_KP_CARET                      0xC3
#define HID_USAGE_KP_PERCENT                    0xC4
#define HID_USAGE_KP_LESS                       0xC5
#define HID_USAGE_KP_GREATER                    0xC6
#define HID_USAGE_KP_AMPERSAND                  0xC7
#define HID_USAGE_KP_LOGICAL_AND                0xC8
#define HID_USAGE_KP_VERTICAL_BAR               0xC9
#define HID_USAGE_KP_LOGIACL_OR                 0xCA
#define HID_USAGE_KP_COLON                      0xCB
#define HID_USAGE_KP_NUMBER_SIGN                0xCC
#define HID_USAGE_KP_SPACE                      0xCD
#define HID_USAGE_KP_AT                         0xCE
#define HID_USAGE_KP_EXCLAMATION_MARK           0xCF
#define HID_USAGE_KP_MEMORY_STORE               0xD0
#define HID_USAGE_KP_MEMORY_RECALL              0xD1
#define HID_USAGE_KP_MEMORY_CLEAR               0xD2
#define HID_USAGE_KP_MEMORY_ADD                 0xD3
#define HID_USAGE_KP_MEMORY_SUBTRACT            0xD4
#define HID_USAGE_KP_MEMORY_MULTIPLY            0xD5
#define HID_USAGE_KP_MEMORY_DIVIDE              0xD6
#define HID_USAGE_KP_PLUSMINUS                  0xD7
#define HID_USAGE_KP_CLEAR                      0xD8
#define HID_USAGE_KP_CLEAR_ENTRY                0xD9
#define HID_USAGE_KP_BINARY                     0xDA
#define HID_USAGE_KP_OCTAL                      0xDB
#define HID_USAGE_KP_DECIMAL                    0xDC
#define HID_USAGE_KP_HEXADECIMAL                0xDD
#define HID_USAGE_KB_LEFT_CONTROL               0xE0
#define HID_USAGE_KB_LEFT_SHIFT                 0xE1
#define HID_USAGE_KB_LEFT_ALT                   0xE2
#define HID_USAGE_KB_LEFT_GUI                   0xE3
#define HID_USAGE_KB_RIGHT_CONTROL              0xE4
#define HID_USAGE_KB_RIGHT_SHIFT                0xE5
#define HID_USAGE_KB_RIGHT_ALT                  0xE6
#define HID_USAGE_KB_RIGHT_GUI                  0xE7
/* E8 - FFFF: reserved */

// HID usages for Consumer Page
#define HID_USAGE_POWER                         0x30
#define HID_USAGE_MENU                          0x40
#define HID_USAGE_ASSIGN_SELECTION              0x81
#define HID_USAGE_ORDER_MOVIE                   0x85
#define HID_USAGE_MEDIA_SELECT_TV               0x89
#define HID_USAGE_MEDIA_SELECT_SECURITY         0x99
#define HID_USAGE_PLAY                          0xb0
#define HID_USAGE_PAUSE                         0xb1
#define HID_USAGE_RECORD                        0xb2
#define HID_USAGE_FAST_FORWARD                  0xb3
#define HID_USAGE_REWIND                        0xb4
#define HID_USAGE_SCAN_NEXT_TRACK               0xb5
#define HID_USAGE_SCAN_PREVIOUS_TRACK           0xb6
#define HID_USAGE_STOP                          0xb7
#define HID_USAGE_EJECT                         0xb8
#define HID_USAGE_RANDOM_PLAY                   0xb9
#define HID_USAGE_STOP_EJECT                    0xcc
#define HID_USAGE_PLAY_PAUSE                    0xcd
#define HID_USAGE_MUTE                          0xe2
#define HID_USAGE_VOLUME_UP                     0xe9
#define HID_USAGE_VOLUME_DOWN                   0xea
// >= 0x0180: "AL" == Application Launch Buttons
#define HID_USAGE_AL_CALCULATOR                 0x0192
#define HID_USAGE_AL_KEYBOARD_LAYOUT            0x01ae
// >= 0x0200: "AC" == Generic GUI Application Control
#define HID_USAGE_AC_SEARCH                     0x0221
#define HID_USAGE_AC_HOME                       0x0223
#define HID_USAGE_AC_BACK                       0x0224
#define HID_USAGE_AC_SCROLL_UP                  0x0233
#define HID_USAGE_AC_SCROLL_DOWN                0x0234
#define HID_USAGE_AC_SCROLL                     0x0235
#define HID_USAGE_AC_PAN                        0x0238

// HID usages for Digitizer page
#define HID_USAGE_IN_RANGE                      0x32
#define HID_USAGE_TIP_SWITCH                    0x42
#define HID_USAGE_CONTACT_IDENTIFIER            0x51
#define HID_USAGE_CONTACT_COUNT                 0x54

// clang-format on

#ifdef __cplusplus
}
#endif

#endif  // HID_USAGE_H
