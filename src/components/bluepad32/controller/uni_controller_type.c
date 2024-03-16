/*
  Copyright (C) Valve Corporation

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

// Taken from:
// https://github.com/libsdl-org/SDL/blob/main/src/joystick/controller_type.c

#include "controller/uni_controller_type.h"

#include <stdbool.h>
#include <stdio.h>

#include "controller/uni_controller_list.h"
#include "sdkconfig.h"
#include "uni_common.h"
#include "uni_log.h"

uni_controller_type_t uni_guess_controller_type(uint16_t vid, uint16_t pid) {
#ifdef CONFIG_TARGET_POSIX
    //  Verify that there are no duplicates in the controller list
    //  If the list were sorted, we could do this much more efficiently, as well as improve lookup speed.
    static bool checked_for_duplicates;
    if (!checked_for_duplicates) {
        checked_for_duplicates = true;
        int i, j;
        for (i = 0; i < ARRAY_SIZE(arrControllers); ++i) {
            for (j = i + 1; j < ARRAY_SIZE(arrControllers); ++j) {
                if (arrControllers[i].device_id == arrControllers[j].device_id) {
                    loge("Duplicate controller entry found for VID 0x%.4x PID 0x%.4x (%d,%d)\n",
                         (arrControllers[i].device_id >> 16), arrControllers[i].device_id & 0xffff, i, j);
                }
            }
        }
    }
#endif  // CONFIG_TARGET_POSIX

    uint32_t device_id = MAKE_CONTROLLER_ID(vid, pid);
    int i;

    for (i = 0; i < ARRAY_SIZE(arrControllers); ++i) {
        if (device_id == arrControllers[i].device_id) {
            return arrControllers[i].controller_type;
        }
    }

    return k_eControllerType_UnknownNonSteamController;
}

const char* uni_guess_controller_name(uint16_t vid, uint16_t pid) {
    uint32_t device_id = MAKE_CONTROLLER_ID(vid, pid);
    int i;
    for (i = 0; i < sizeof(arrControllers) / sizeof(arrControllers[0]); ++i) {
        if (device_id == arrControllers[i].device_id) {
            return arrControllers[i].name;
        }
    }
    return NULL;
}

#undef MAKE_CONTROLLER_ID
