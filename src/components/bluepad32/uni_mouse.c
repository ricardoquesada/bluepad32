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

#include "uni_mouse.h"
#include "uni_log.h"

void uni_mouse_dump(const uni_mouse_t* ms) {
    // Don't add "\n"
    logi("delta_x=%d, delta_y=%d, buttons=%#x, misc_buttons=%#x, scroll_wheel=%#x", ms->delta_x, ms->delta_y,
         ms->buttons, ms->misc_buttons, ms->scroll_wheel);
}
