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

// Debug version

#include "uni_platform_pc_debug.h"

#include <stdio.h>
#include <string.h>

#include "uni_debug.h"
#include "uni_hid_device.h"

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

static void pc_debug_on_joy_a_data(uni_joystick_t* joy) {
  print_joystick(0, joy);
}
static void pc_debug_on_joy_b_data(uni_joystick_t* joy) {
  print_joystick(1, joy);
}
static void pc_debug_on_mouse_data(int32_t delta_x, int32_t delta_y,
                                   uint16_t buttons) {
  printf("pc_debug: mouse: x=%d, y=%d, buttons=0x%4x\n", delta_x, delta_y,
         buttons);
}

static uint8_t pc_debug_is_button_pressed() { return g_delete_keys; }

// Events.
static void pc_debug_on_init(int argc, const char** argv) {
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

static void pc_debug_on_init_complete(void) {}

static void pc_debug_on_device_connected(uni_hid_device_t* d) {
  printf("pc_debug: device connected: %p\n", d);
}

static void pc_debug_on_device_disconnected(uni_hid_device_t* d) {
  printf("pc_debug: device disconnected: %p\n", d);
}

static int pc_debug_on_device_ready(uni_hid_device_t* d) {
  printf("pc_debug: device ready: %p\n", d);
  return 0;
}

struct uni_platform* uni_platform_pc_debug_create(void) {
  static struct uni_platform plat;

  plat.name = "PC Debug";
  plat.on_init = pc_debug_on_init;
  plat.on_init_complete = pc_debug_on_init_complete;
  plat.on_device_connected = pc_debug_on_device_connected;
  plat.on_device_disconnected = pc_debug_on_device_disconnected;
  plat.on_device_ready = pc_debug_on_device_ready;
  plat.on_joy_a_data = pc_debug_on_joy_a_data;
  plat.on_joy_b_data = pc_debug_on_joy_b_data;
  plat.on_mouse_data = pc_debug_on_mouse_data;
  plat.is_button_pressed = pc_debug_is_button_pressed;

  return &plat;
}
