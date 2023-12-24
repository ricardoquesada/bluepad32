// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "controller/uni_controller.h"
#include "uni_log.h"

void uni_controller_dump(const uni_controller_t* ctl) {
    switch (ctl->klass) {
        case UNI_CONTROLLER_CLASS_BALANCE_BOARD:
            uni_balance_board_dump(&ctl->balance_board);
            break;
        case UNI_CONTROLLER_CLASS_GAMEPAD:
            uni_gamepad_dump(&ctl->gamepad);
            break;
        case UNI_CONTROLLER_CLASS_MOUSE:
            uni_mouse_dump(&ctl->mouse);
            break;
        case UNI_CONTROLLER_CLASS_KEYBOARD:
            uni_keyboard_dump(&ctl->keyboard);
            break;
        default:
            logi("uni_controller_dump: Unsupported controller class: %d\n", ctl->klass);
            break;
    }
    logi(", battery=%d\n", ctl->battery);
}
