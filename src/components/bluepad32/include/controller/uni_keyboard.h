// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_KEYBOARD_H
#define UNI_KEYBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "uni_common.h"

// Array of pressed keys. Hardcode it at 10.
// We expect that keyboards won't support more than 10 press keys at the same time,
// since we have a max of 10 fingers.
#define UNI_KEYBOARD_PRESSED_KEYS_MAX 10

// Instead of using the HID_USAGE values, we use a special field for them.
// Easier to parse.
enum {
    UNI_KEYBOARD_MODIFIER_LEFT_CONTROL = BIT(0),
    UNI_KEYBOARD_MODIFIER_LEFT_SHIFT = BIT(1),
    UNI_KEYBOARD_MODIFIER_LEFT_ALT = BIT(2),
    UNI_KEYBOARD_MODIFIER_LEFT_GUI = BIT(3),
    UNI_KEYBOARD_MODIFIER_RIGHT_CONTROL = BIT(4),
    UNI_KEYBOARD_MODIFIER_RIGHT_SHIFT = BIT(5),
    UNI_KEYBOARD_MODIFIER_RIGHT_ALT = BIT(6),
    UNI_KEYBOARD_MODIFIER_RIGHT_GUI = BIT(7),
};

typedef struct {
    // Bitmap of the modifiers.
    uint8_t modifiers;
    uint8_t pressed_keys[UNI_KEYBOARD_PRESSED_KEYS_MAX];
    // Reserved for future use, like "Consumer page": eject, play, pause keyboard buttons.
    uint8_t reserved[16];
} uni_keyboard_t;

void uni_keyboard_dump(const uni_keyboard_t* kb);

#ifdef __cplusplus
}
#endif

#endif  // UNI_KEYBOARD_H
