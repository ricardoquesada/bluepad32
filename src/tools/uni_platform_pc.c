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

// Stub version

#include <stdio.h>
#include <string.h>

#include "uni_debug.h"
#include "uni_hid_device.h"
#include "uni_platform.h"

static int g_enhanced_mode = 0;
static int g_delete_keys = 0;
static uni_joystick_t prev_joys[2] = {{.auto_fire = 255}, {.auto_fire = 255}};

static void print_joystick(int joy_port, uni_joystick_t* joy) {
  if (memcmp(joy, &prev_joys[joy_port], sizeof(*joy)) != 0) {
    printf(
        "Joy %d: up=%d, down=%d, left=%d, right=%d, fire=%d, potx=%d, poty=%d, "
        "autofire=%d\n",
        joy_port, joy->up, joy->down, joy->left, joy->right, joy->fire,
        joy->pot_x, joy->pot_y, joy->auto_fire);
    memcpy(&prev_joys[joy_port], joy, sizeof(*joy));
  }
}

void uni_platform_init(int argc, const char** argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--enhanced") == 0 || strcmp(argv[i], "-e") == 0) {
      g_enhanced_mode = 1;
      logi("Enhanced mode enabled\n");
    }
    if (strcmp(argv[i], "--delete") == 0 || strcmp(argv[i], "-d") == 0) {
      g_delete_keys = 1;
      logi("Stored keys will be deleted\n");
    }
  }
}

void uni_platform_on_joy_a_data(uni_joystick_t* joy) { print_joystick(0, joy); }
void uni_platform_on_joy_b_data(uni_joystick_t* joy) { print_joystick(1, joy); }
void uni_platform_on_mouse_data(int32_t delta_x, int32_t delta_y,
                                uint16_t buttons) {
  printf("mouse: x=%d, y=%d, buttons=0x%4x\n", delta_x, delta_y, buttons);
}

uint8_t uni_platform_is_button_pressed() { return g_delete_keys; }

// Events.
void uni_platform_on_init_complete(void) {}
void uni_platform_on_port_assign_changed(uni_joystick_port_t port) {
  uint8_t port_status_a = ((port & JOYSTICK_PORT_A) != 0);
  uint8_t port_status_b = ((port & JOYSTICK_PORT_B) != 0);
  printf("LED A = %d, LED B = %d\n", port_status_a, port_status_b);

  // Enable enhanced mode
  if (g_enhanced_mode && port != (JOYSTICK_PORT_A | JOYSTICK_PORT_B)) {
    uni_hid_device_on_emu_mode_change();
  }
}