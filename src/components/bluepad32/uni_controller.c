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

#include "uni_controller.h"
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
        default:
            logi("uni_controller_dump: Unsupported controller class: %d\n", ctl->klass);
            break;
    }
    logi(", battery=%d\n", ctl->battery);
}
