// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "controller/uni_mouse.h"

#include "uni_log.h"

void uni_mouse_dump(const uni_mouse_t* ms) {
    // Don't add "\n"
    logi("delta_x=%4d, delta_y=%4d, buttons=%#x, misc_buttons=%#x, scroll_wheel=%#x", ms->delta_x, ms->delta_y,
         ms->buttons, ms->misc_buttons, ms->scroll_wheel);
}
