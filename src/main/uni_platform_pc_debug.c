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
#include "uni_gamepad.h"
#include "uni_hid_device.h"

static int g_enhanced_mode = 0;
static int g_delete_keys = 0;

// PC Debug "instance"
typedef struct pc_debug_instance_s {
  uni_gamepad_seat_t gamepad_seat;  // which "seat" (port) is being used
} pc_debug_instance_t;

// Declarations
static void set_led(uni_hid_device_t* d);
static pc_debug_instance_t* get_pc_debug_instance(uni_hid_device_t* d);

static void pc_debug_on_gamepad_data(uni_hid_device_t* d, uni_gamepad_t* gp) {
  UNUSED(d);
  uni_gamepad_dump(gp);
}

static uint8_t pc_debug_is_button_pressed() { return g_delete_keys; }

// Events.
static void pc_debug_on_init(int argc, const char** argv) {
  printf("pc_debug: on_init()\n");
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

static void pc_debug_on_init_complete(void) {
  printf("pc_debug: on_init_complete()\n");
}

static void pc_debug_on_device_connected(uni_hid_device_t* d) {
  printf("pc_debug: device connected: %p\n", d);
}

static void pc_debug_on_device_disconnected(uni_hid_device_t* d) {
  printf("pc_debug: device disconnected: %p\n", d);
}

static int pc_debug_on_device_ready(uni_hid_device_t* d) {
  printf("pc_debug: device ready: %p\n", d);
  pc_debug_instance_t* ins = get_pc_debug_instance(d);
  ins->gamepad_seat = GAMEPAD_SEAT_A;

  // FIXME: call paser->update_led() from on_device_ready. Otherwise parsers
  // like the DS4 might not enable "stream" mode and will fail.
  set_led(d);
  return 0;
}

static void pc_debug_on_device_oob_event(uni_hid_device_t* d, int event) {
  UNUSED(event);
  if (d == NULL) {
    loge("ERROR: pc_debug_on_device_gamepad_event: Invalid NULL device\n");
    return;
  }

  if (event != GAMEPAD_SYSTEM_BUTTON_PRESSED) {
    loge("ERROR: pc_debug_on_device_gamepad_event: unsupported event: 0x%04x\n",
         event);
    return;
  }

  pc_debug_instance_t* ins = get_pc_debug_instance(d);
  ins->gamepad_seat =
      ins->gamepad_seat == GAMEPAD_SEAT_A ? GAMEPAD_SEAT_B : GAMEPAD_SEAT_A;

  set_led(d);
}

static void set_led(uni_hid_device_t* d) {
  if (d->report_parser.update_led != NULL) {
    pc_debug_instance_t* ins = get_pc_debug_instance(d);
    d->report_parser.update_led(d, ins->gamepad_seat);
  }
}

//
// Helpers
//

static pc_debug_instance_t* get_pc_debug_instance(uni_hid_device_t* d) {
  return (pc_debug_instance_t*)&d->platform_data[0];
}

//
// Entry Point
//

struct uni_platform* uni_platform_pc_debug_create(void) {
  static struct uni_platform plat;

  plat.name = "PC Debug";
  plat.on_init = pc_debug_on_init;
  plat.on_init_complete = pc_debug_on_init_complete;
  plat.on_device_connected = pc_debug_on_device_connected;
  plat.on_device_disconnected = pc_debug_on_device_disconnected;
  plat.on_device_ready = pc_debug_on_device_ready;
  plat.on_device_oob_event = pc_debug_on_device_oob_event;
  plat.on_gamepad_data = pc_debug_on_gamepad_data;
  plat.is_button_pressed = pc_debug_is_button_pressed;

  return &plat;
}
