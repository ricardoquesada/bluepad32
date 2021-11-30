
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

#include "uni_gamepad.h"

#include "uni_config.h"
#include "uni_debug.h"

// extern
const int AXIS_NORMALIZE_RANGE = 1024;  // 10-bit resolution (1024)
const int AXIS_THRESHOLD = (1024 / 8);

void uni_gamepad_dump(const uni_gamepad_t* gp) {
  logi(
      "(0x%04x) dpad=0x%02x, x=%d, y=%d, rx=%d, ry=%d, brake=%d, accel=%d, "
      "buttons=0x%08x, misc=0x%02x\n",
      gp->updated_states, gp->dpad, gp->axis_x, gp->axis_y, gp->axis_rx,
      gp->axis_ry, gp->brake, gp->throttle, gp->buttons, gp->misc_buttons);
}
