
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

static void to_single_joy(const uni_gamepad_t* gp, uni_joystick_t* out_joy);

static void to_single_joy(const uni_gamepad_t* gp, uni_joystick_t* out_joy) {
  // Button A is "fire"
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_A) {
    out_joy->fire |= ((gp->buttons & BUTTON_A) != 0);
  }
  // Thumb left is "fire"
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_THUMB_L) {
    out_joy->fire |= ((gp->buttons & BUTTON_THUMB_L) != 0);
  }

  // Shoulder right is "auto fire"
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_SHOULDER_R) {
    out_joy->auto_fire |= ((gp->buttons & BUTTON_SHOULDER_R) != 0);
  }

  // Dpad
  if (gp->updated_states & GAMEPAD_STATE_DPAD) {
    if (gp->dpad & 0x01) out_joy->up |= 1;
    if (gp->dpad & 0x02) out_joy->down |= 1;
    if (gp->dpad & 0x04) out_joy->right |= 1;
    if (gp->dpad & 0x08) out_joy->left |= 1;
  }

  // Axis: X and Y
  if (gp->updated_states & GAMEPAD_STATE_AXIS_X) {
    out_joy->left |= (gp->axis_x < -AXIS_THRESHOLD);
    out_joy->right |= (gp->axis_x > AXIS_THRESHOLD);
  }
  if (gp->updated_states & GAMEPAD_STATE_AXIS_Y) {
    out_joy->up |= (gp->axis_y < -AXIS_THRESHOLD);
    out_joy->down |= (gp->axis_y > AXIS_THRESHOLD);
  }

  // Pots
  if (gp->updated_states & GAMEPAD_STATE_BRAKE) {
    out_joy->pot_x = (gp->brake >> 2);  // convert from 1024 to 256
  }
  if (gp->updated_states & GAMEPAD_STATE_ACCELERATOR) {
    out_joy->pot_y = (gp->accelerator >> 2);  // convert from 1024 to 256
  }
}

void uni_gamepad_to_single_joy(const uni_gamepad_t* gp,
                               uni_joystick_t* out_joy) {
  to_single_joy(gp, out_joy);

#if UNIJOYSTICLE_SINGLE_PORT == 0
  // Buttom B is "jump"
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_B) {
    out_joy->up |= ((gp->buttons & BUTTON_B) != 0);
  }
#else   // UNIJOYSTICLE_SINGLE_PORT == 1
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_B) {
    out_joy->pot_y |= ((gp->buttons & BUTTON_B) != 0);
  }
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_X) {
    out_joy->pot_x |= ((gp->buttons & BUTTON_X) != 0);
  }
#endif  // UNIJOYSTICLE_SINGLE_PORT == 1
}

void uni_gamepad_to_combo_joy_joy(const uni_gamepad_t* gp,
                                  uni_joystick_t* out_joy1,
                                  uni_joystick_t* out_joy2) {
  to_single_joy(gp, out_joy1);

  // Buttom B is "fire"
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_B) {
    out_joy2->fire |= ((gp->buttons & BUTTON_B) != 0);
  }
  // Thumb right is "fire"
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_THUMB_R) {
    out_joy2->fire |= ((gp->buttons & BUTTON_THUMB_R) != 0);
  }

  // Swap "auto fire" in Combo Joy Joy.
  // "left" belongs to joy1 while "right" to joy2.
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_SHOULDER_L) {
    out_joy1->auto_fire = ((gp->buttons & BUTTON_SHOULDER_L) != 0);
  }
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_SHOULDER_R) {
    out_joy2->auto_fire = ((gp->buttons & BUTTON_SHOULDER_R) != 0);
  }

  // Axis: RX and RY
  if (gp->updated_states & GAMEPAD_STATE_AXIS_RX) {
    out_joy2->left |= (gp->axis_rx < -AXIS_THRESHOLD);
    out_joy2->right |= (gp->axis_rx > AXIS_THRESHOLD);
  }
  if (gp->updated_states & GAMEPAD_STATE_AXIS_RY) {
    out_joy2->up |= (gp->axis_ry < -AXIS_THRESHOLD);
    out_joy2->down |= (gp->axis_ry > AXIS_THRESHOLD);
  }
}

void uni_gamepad_to_single_mouse(const uni_gamepad_t* gp,
                                 uni_joystick_t* out_mouse) {
  to_single_joy(gp, out_mouse);
}

void uni_gamepad_to_combo_joy_mouse(const uni_gamepad_t* gp,
                                    uni_joystick_t* out_joy,
                                    uni_joystick_t* out_mouse) {
  to_single_joy(gp, out_joy);

  // Axis: RX and RY
  if (gp->updated_states & GAMEPAD_STATE_AXIS_RX) {
    out_mouse->left |= (gp->axis_rx < -AXIS_THRESHOLD);
    out_mouse->right |= (gp->axis_rx > AXIS_THRESHOLD);
  }
  if (gp->updated_states & GAMEPAD_STATE_AXIS_RY) {
    out_mouse->up |= (gp->axis_ry < -AXIS_THRESHOLD);
    out_mouse->down |= (gp->axis_ry > AXIS_THRESHOLD);
  }

  // Buttom B is "mouse left button"
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_B) {
    out_mouse->fire |= ((gp->buttons & BUTTON_B) != 0);
  }

  // Buttom X is "mouse middle button"
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_X) {
    out_mouse->pot_x |= ((gp->buttons & BUTTON_X) != 0);
  }

  // Buttom Y is "mouse right button"
  if (gp->updated_states & GAMEPAD_STATE_BUTTON_Y) {
    out_mouse->pot_y |= ((gp->buttons & BUTTON_Y) != 0);
  }
}

void uni_gamepad_dump(const uni_gamepad_t* gp) {
  logd(
      "(0x%04x) dpad=0x%02x, x=%d, y=%d, rx=%d, ry=%d, brake=%d, accel=%d, "
      "buttons=0x%08x, misc=0x%02x\n",
      gp->updated_states, gp->dpad, gp->axis_x, gp->axis_y, gp->axis_rx,
      gp->axis_ry, gp->brake, gp->accelerator, gp->buttons, gp->misc_buttons);
}
