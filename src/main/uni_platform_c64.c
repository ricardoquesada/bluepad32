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

// C64 - ESP32 version
#include "uni_platform_c64.h"

#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <math.h>

#include "uni_config.h"
#include "uni_debug.h"
#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_joystick.h"

// --- Consts

// 20 milliseconds ~= 1 frame in PAL
// 17 milliseconds ~= 1 frame in NTSC
static const int AUTOFIRE_FREQ_MS = 20 * 4;  // change every ~4 frames

static const int MOUSE_DELAY_BETWEEN_EVENT_US = 1200;  // microseconds
// static const int MOUSE_MAX_DELTA = 32;

// GPIO map for MH-ET Live mini-kit board.
static const int GPIO_LED_J1 = GPIO_NUM_5;
static const int GPIO_LED_J2 = GPIO_NUM_13;
static const int GPIO_PUSH_BUTTON = GPIO_NUM_10;

enum {
  GPIO_JOY_A_UP = GPIO_NUM_26,
  GPIO_JOY_A_DOWN = GPIO_NUM_18,
  GPIO_JOY_A_LEFT = GPIO_NUM_19,
  GPIO_JOY_A_RIGHT = GPIO_NUM_23,

  GPIO_JOY_A_FIRE = GPIO_NUM_14,
  GPIO_MOUSE_BUTTON_L = GPIO_NUM_14,

  GPIO_JOY_A_POT_Y = GPIO_NUM_16,
  GPIO_MOUSE_BUTTON_M = GPIO_NUM_16,

  GPIO_JOY_A_POT_X = GPIO_NUM_33,
  GPIO_MOUSE_BUTTON_R = GPIO_NUM_33,

  GPIO_JOY_B_UP = GPIO_NUM_27,
  GPIO_JOY_B_DOWN = GPIO_NUM_25,
  GPIO_JOY_B_LEFT = GPIO_NUM_32,
  GPIO_JOY_B_RIGHT = GPIO_NUM_17,
  GPIO_JOY_B_FIRE = GPIO_NUM_12,
};

// GPIO_NUM_12 (input) used as input for Pot in esp32.
// GPIO_NUM_13 (output) used to feed the Pot in the c64
enum {
  // Event group
  EVENT_BIT_MOUSE = (1 << 0),
  EVENT_BIT_POT = (1 << 1),
  EVENT_BIT_BUTTON = (1 << 2),

  // Autofire Group
  EVENT_BIT_AUTOFIRE = (1 << 0),
};

// Different emulation modes
typedef enum {
  EMULATION_MODE_SINGLE_JOY,
  EMULATION_MODE_SINGLE_MOUSE,
  EMULATION_MODE_COMBO_JOY_JOY,
  EMULATION_MODE_COMBO_JOY_MOUSE,
} emulation_mode_t;

// C64 "instance"
typedef struct c64_instance_s {
  emulation_mode_t emu_mode;             // type of controller to emulate
  uni_gamepad_seat_t gamepad_seat;       // which "seat" (port) is being used
  uni_gamepad_seat_t prev_gamepad_seat;  // which "seat" (port) was used before
                                         // switching emu mode
} c64_instance_t;

// --- Constants
static const gpio_num_t JOY_A_PORTS[] = {
    GPIO_JOY_A_UP,   GPIO_JOY_A_DOWN,  GPIO_JOY_A_LEFT, GPIO_JOY_A_RIGHT,
    GPIO_JOY_A_FIRE, GPIO_JOY_A_POT_X, GPIO_JOY_A_POT_Y};
static const gpio_num_t JOY_B_PORTS[] = {
    GPIO_JOY_B_UP,   GPIO_JOY_B_DOWN,  GPIO_JOY_B_LEFT, GPIO_JOY_B_RIGHT,
    GPIO_JOY_B_FIRE, GPIO_JOY_A_POT_X, GPIO_JOY_A_POT_Y};

static const bd_addr_t zero_addr = {0, 0, 0, 0, 0, 0};

// --- Globals

static int64_t g_last_time_pressed_us = 0;  // in microseconds
static EventGroupHandle_t g_event_group;
static EventGroupHandle_t g_auto_fire_group;

// Mouse "shared data from main task to mouse task.
static int32_t g_delta_x = 0;
static int32_t g_delta_y = 0;

// Pot "shared data from main task to pot task.
static uint8_t g_pot_x = 0;
static uint8_t g_pot_y = 0;

// Autofire
static uint8_t g_autofire_a_enabled = 0;
static uint8_t g_autofire_b_enabled = 0;

// --- Code

static c64_instance_t* get_c64_instance(uni_hid_device_t* d);
static void set_gamepad_seat(uni_hid_device_t* d, uni_gamepad_seat_t seat);
static void process_joystick(uni_joystick_t* joy, uni_gamepad_seat_t seat);
static void process_mouse(uni_hid_device_t* d, int32_t delta_x, int32_t delta_y,
                          uint16_t buttons);

// Interrupt handlers
static void handle_event_mouse();
static void handle_event_pot();
static void handle_event_button();

// Mouse related
static void joy_update_port(uni_joystick_t* joy, const gpio_num_t* gpios);
static void mouse_send_move(int pin_a, int pin_b, uint32_t delay);
static void mouse_move_x(int dir, uint32_t delay);
static void mouse_move_y(int dir, uint32_t delay);

static void delay_us(uint32_t delay);

// GPIO Interrupt handlers
static void IRAM_ATTR gpio_isr_handler_button(void* arg);
#if UNI_ENABLE_POT
static void IRAM_ATTR gpio_isr_handler_pot(void* arg);
#endif  // UNI_ENABLE_POT

static void event_loop(void* arg);
static void auto_fire_loop(void* arg);

#define MAX(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b;      \
  })

#define MIN(a, b)           \
  ({                        \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b;      \
  })

// Platform Events
static void c64_on_init(int argc, const char** argv) {
  UNUSED(argc);
  UNUSED(argv);
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  // Port A.
  io_conf.pin_bit_mask =
      ((1ULL << GPIO_JOY_A_UP) | (1ULL << GPIO_JOY_A_DOWN) |
       (1ULL << GPIO_JOY_A_LEFT) | (1ULL << GPIO_JOY_A_RIGHT) |
       (1ULL << GPIO_JOY_A_FIRE) | (1ULL << GPIO_JOY_A_POT_X) |
       (1ULL << GPIO_JOY_A_POT_Y));
  // Port B.
  io_conf.pin_bit_mask |=
      ((1ULL << GPIO_JOY_B_UP) | (1ULL << GPIO_JOY_B_DOWN) |
       (1ULL << GPIO_JOY_B_LEFT) | (1ULL << GPIO_JOY_B_RIGHT) |
       (1ULL << GPIO_JOY_B_FIRE));

  // Leds
  io_conf.pin_bit_mask |= (1ULL << GPIO_LED_J1);
  io_conf.pin_bit_mask |= (1ULL << GPIO_LED_J2);

  // Pot feeder
  // io_conf.pin_bit_mask |= (1ULL << GPIO_NUM_13);

  ESP_ERROR_CHECK(gpio_config(&io_conf));

  // Set low all GPIOs... just in case.
  const int MAX_GPIOS = sizeof(JOY_A_PORTS) / sizeof(JOY_A_PORTS[0]);
  for (int i = 0; i < MAX_GPIOS; i++) {
    ESP_ERROR_CHECK(gpio_set_level(JOY_A_PORTS[i], 0));
    ESP_ERROR_CHECK(gpio_set_level(JOY_B_PORTS[i], 0));
  }

  // Turn On LED
  gpio_set_level(GPIO_LED_J1, 1);
  gpio_set_level(GPIO_LED_J2, 1);

  // Pull-up for button
  io_conf.intr_type = GPIO_INTR_ANYEDGE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  io_conf.pin_bit_mask = (1ULL << GPIO_PUSH_BUTTON);
  ESP_ERROR_CHECK(gpio_config(&io_conf));
  ESP_ERROR_CHECK(gpio_install_isr_service(0));
  ESP_ERROR_CHECK(gpio_isr_handler_add(
      GPIO_PUSH_BUTTON, gpio_isr_handler_button, (void*)GPIO_PUSH_BUTTON));

// C64 POT related
#if UNI_ENABLE_POT
  io_conf.intr_type = GPIO_INTR_POSEDGE;  // GPIO_INTR_NEGEDGE
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = 1ULL << GPIO_NUM_12;
  io_conf.pull_down_en = false;
  io_conf.pull_up_en = true;
  ESP_ERROR_CHECK(gpio_config(&io_conf));
  ESP_ERROR_CHECK(gpio_install_isr_service(0));
  ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_12, gpio_isr_handler_pot,
                                       (void*)GPIO_NUM_12));
#endif  // UNI_ENABLE_POT

  // Split "events" from "auto_fire", since auto-fire is an on-going event.
  g_event_group = xEventGroupCreate();
  xTaskCreate(event_loop, "event_loop", 2048, NULL, 10, NULL);

  g_auto_fire_group = xEventGroupCreate();
  xTaskCreate(auto_fire_loop, "auto_fire_loop", 2048, NULL, 10, NULL);
  // xTaskCreatePinnedToCore(event_loop, "event_loop", 2048, NULL,
  // portPRIVILEGE_BIT, NULL, 1);
}

static void c64_on_init_complete(void) {
  // Turn Off LEDs
  gpio_set_level(GPIO_LED_J1, 0);
  gpio_set_level(GPIO_LED_J2, 0);
}

static void c64_on_device_connected(uni_hid_device_t* d) {
  if (d == NULL) {
    loge("ERROR: c64_on_device_connected: Invalid NULL device\n");
  }
}

static void c64_on_device_disconnected(uni_hid_device_t* d) {
  if (d == NULL) {
    loge("ERROR: c64_on_device_disconnected: Invalid NULL device\n");
    return;
  }
  c64_instance_t* ins = get_c64_instance(d);

  if (ins->gamepad_seat != GAMEPAD_SEAT_NONE) {
    set_gamepad_seat(d, GAMEPAD_SEAT_NONE);
    ins->emu_mode = EMULATION_MODE_SINGLE_JOY;
  }
}

static int c64_on_device_ready(uni_hid_device_t* d) {
  if (d == NULL) {
    loge("ERROR: c64_on_device_ready: Invalid NULL device\n");
    return -1;
  }
  c64_instance_t* ins = get_c64_instance(d);

  // Some safety checks. These conditions should not happen
  if ((ins->gamepad_seat != GAMEPAD_SEAT_NONE) ||
      (!uni_hid_device_has_controller_type(d))) {
    loge("uni_hid_device_set_ready: pre-condition not met\n");
    return -1;
  }

  uint32_t used_joystick_ports = 0;
  for (int i = 0; i < UNI_HID_DEVICE_MAX_DEVICES; i++) {
    uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(i);
    used_joystick_ports |= get_c64_instance(tmp_d)->gamepad_seat;
  }

#if UNIJOYSTICLE_SINGLE_PORT
  int wanted_seat = GAMEPAD_SEAT_A;
  ins->emu_mode = EMULATION_MODE_SINGLE_JOY;
#else   // UNIJOYSTICLE_SINGLE_PORT  == 0
  // Try with Port B, assume it is a joystick
  int wanted_seat = GAMEPAD_SEAT_B;
  ins->emu_mode = EMULATION_MODE_SINGLE_JOY;
  // d->emu_mode = EMULATION_MODE_COMBO_JOY_JOY;

  // ... unless it is a mouse which should try with PORT A. Amiga/Atari ST use
  // mice in PORT A. Undefined on the C64, but most apps use it in PORT A as
  // well.
  uint32_t mouse_cod = MASK_COD_MAJOR_PERIPHERAL | MASK_COD_MINOR_POINT_DEVICE;
  if ((d->cod & mouse_cod) == mouse_cod) {
    wanted_seat = GAMEPAD_SEAT_A;
    ins->emu_mode = EMULATION_MODE_SINGLE_MOUSE;
  }

  // If wanted port is already assigned, try with the next one
  if (used_joystick_ports & wanted_seat) {
    logi("Port already assigned, trying another one\n");
    wanted_seat = (~wanted_seat) & GAMEPAD_SEAT_AB_MASK;
  }
#endif  // UNIJOYSTICLE_SINGLE_PORT  == 0

  set_gamepad_seat(d, wanted_seat);

  if (d->report_parser.setup) d->report_parser.setup(d);

  return 0;
}

static void c64_on_device_gamepad_event(uni_hid_device_t* d, int event) {
  if (d == NULL) {
    loge("ERROR: c64_on_device_gamepad_event: Invalid NULL device\n");
    return;
  }

  c64_instance_t* ins = get_c64_instance(d);

  if (ins->gamepad_seat == GAMEPAD_SEAT_NONE) {
    logi(
        "c64: cannot swap port since device has joystick_port = "
        "GAMEPAD_SEAT_NONE\n");
    return;
  }

  // This could happen if device is any Combo emu mode.
  if (ins->gamepad_seat == (GAMEPAD_SEAT_A | GAMEPAD_SEAT_B)) {
    logi(
        "c64: cannot swap port since has more than one port associated with. "
        "Leave emu mode and try again.\n");
    return;
  }

  // Swap joysticks if only one device is attached.
  int num_devices = 0;
  for (int j = 0; j < UNI_HID_DEVICE_MAX_DEVICES; j++) {
    uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(j);
    if ((bd_addr_cmp(tmp_d->address, zero_addr) != 0) &&
        (get_c64_instance(tmp_d)->gamepad_seat > 0)) {
      num_devices++;
      if (num_devices > 1) {
        logi(
            "c64: cannot swap joystick ports when more than one device is "
            "attached\n");
        uni_hid_device_dump_all();
        return;
      }
    }
  }

  // swap joystick A with B
  uni_gamepad_seat_t seat =
      (ins->gamepad_seat == GAMEPAD_SEAT_A) ? GAMEPAD_SEAT_B : GAMEPAD_SEAT_A;
  set_gamepad_seat(d, seat);

  // Clear joystick after switch to avoid having a line "On".
  uni_joystick_t joy;
  memset(&joy, 0, sizeof(joy));
  process_joystick(&joy, GAMEPAD_SEAT_A);
  process_joystick(&joy, GAMEPAD_SEAT_B);
}

static void c64_on_gamepad_data(uni_hid_device_t* d, uni_gamepad_t* gp) {
  if (d == NULL) {
    loge("ERROR: c64_on_device_gamepad_data: Invalid NULL device\n");
    return;
  }

  c64_instance_t* ins = get_c64_instance(d);

  // FIXME: Add support for EMULATION_MODE_COMBO_JOY_MOUSE
  uni_joystick_t joy, joy_ext;
  memset(&joy, 0, sizeof(joy));
  memset(&joy_ext, 0, sizeof(joy_ext));

  switch (ins->emu_mode) {
    case EMULATION_MODE_SINGLE_JOY:
      uni_joy_to_single_joy_from_gamepad(gp, &joy);
      process_joystick(&joy, ins->gamepad_seat);
      break;
    case EMULATION_MODE_SINGLE_MOUSE:
      uni_joy_to_single_mouse_from_gamepad(gp, &joy);
      process_mouse(d, gp->axis_x, gp->axis_y, gp->buttons);
      break;
    case EMULATION_MODE_COMBO_JOY_JOY:
      uni_joy_to_combo_joy_joy_from_gamepad(gp, &joy, &joy_ext);
      process_joystick(&joy, GAMEPAD_SEAT_B);
      process_joystick(&joy_ext, GAMEPAD_SEAT_A);
      break;
    case EMULATION_MODE_COMBO_JOY_MOUSE:
      uni_joy_to_combo_joy_mouse_from_gamepad(gp, &joy, &joy_ext);
      process_joystick(&joy, GAMEPAD_SEAT_B);
      process_joystick(&joy_ext, GAMEPAD_SEAT_A);
      break;
    default:
      loge("c64: Unsupported emulation mode: %d\n", ins->emu_mode);
      break;
  }
}

static uint8_t c64_is_button_pressed(void) {
  // Hi-released, Low-pressed
  return !gpio_get_level(GPIO_PUSH_BUTTON);
}

//
// Helpers
//

static c64_instance_t* get_c64_instance(uni_hid_device_t* d) {
  return (c64_instance_t*)&d->platform_data[0];
}

static void process_mouse(uni_hid_device_t* d, int32_t delta_x, int32_t delta_y,
                          uint16_t buttons) {
  UNUSED(d);
  static uint16_t prev_buttons = 0;
  logd("c64: mouse: x=%d, y=%d, buttons=0x%4x\n", delta_x, delta_y, buttons);

  // Mouse is implemented using a quadrature encoding
  // FIXME: Passing values to mouse task using global variables. This is, of
  // course, error-prone to races and what not, but seeems to be good enough for
  // our purpose.
  if (delta_x || delta_y) {
    g_delta_x = delta_x;
    g_delta_y = delta_y;
    xEventGroupSetBits(g_event_group, EVENT_BIT_MOUSE);
  }
  if (buttons != prev_buttons) {
    prev_buttons = buttons;
    gpio_set_level(GPIO_MOUSE_BUTTON_L, (buttons & BUTTON_A));
    gpio_set_level(GPIO_MOUSE_BUTTON_R, (buttons & BUTTON_B));
    gpio_set_level(GPIO_MOUSE_BUTTON_M, (buttons & BUTTON_X));
  }
}

static void process_joystick(uni_joystick_t* joy, uni_gamepad_seat_t seat) {
  if (seat == GAMEPAD_SEAT_A) {
    joy_update_port(joy, JOY_A_PORTS);
    g_autofire_a_enabled = joy->auto_fire;
  } else if (seat == GAMEPAD_SEAT_B) {
    joy_update_port(joy, JOY_B_PORTS);
    g_autofire_b_enabled = joy->auto_fire;
  } else {
    loge("c64: process_joystick: invalid gamepad seat: %d\n", seat);
  }

  if (g_autofire_a_enabled || g_autofire_b_enabled) {
    xEventGroupSetBits(g_auto_fire_group, EVENT_BIT_AUTOFIRE);
  }
}

static void set_gamepad_seat(uni_hid_device_t* d, uni_gamepad_seat_t seat) {
  c64_instance_t* ins = get_c64_instance(d);
  ins->gamepad_seat = seat;

  logi("C64: device %s has new gamepad seat: %d\n", bd_addr_to_str(d->address),
       seat);

  // Fetch all enabled ports
  uni_gamepad_seat_t all_seats = GAMEPAD_SEAT_NONE;
  for (int i = 0; i < UNI_HID_DEVICE_MAX_DEVICES; i++) {
    uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(i);
    if (tmp_d == NULL) continue;
    if (bd_addr_cmp(tmp_d->address, zero_addr) != 0) {
      all_seats |= get_c64_instance(tmp_d)->gamepad_seat;
    }
  }

  bool status_a = ((all_seats & GAMEPAD_SEAT_A) != 0);
  bool status_b = ((all_seats & GAMEPAD_SEAT_B) != 0);
  gpio_set_level(GPIO_LED_J1, status_a);
  gpio_set_level(GPIO_LED_J2, status_b);

  if (d->report_parser.update_led != NULL) {
    d->report_parser.update_led(d, all_seats);
  }
}

static void joy_update_port(uni_joystick_t* joy, const gpio_num_t* gpios) {
  logd("up=%d, down=%d, left=%d, right=%d, fire=%d, potx=%d, poty=%d\n",
       joy->up, joy->down, joy->left, joy->right, joy->fire, joy->pot_x,
       joy->pot_y);

  g_pot_x = joy->pot_x;
  g_pot_y = joy->pot_y;

  gpio_set_level(gpios[0], !!joy->up);
  gpio_set_level(gpios[1], !!joy->down);
  gpio_set_level(gpios[2], !!joy->left);
  gpio_set_level(gpios[3], !!joy->right);

  // only update fire if auto-fire is off. otherwise it will conflict.
  if (!joy->auto_fire) {
    gpio_set_level(gpios[4], !!joy->fire);
  }

#if UNIJOYSTICLE_SINGLE_PORT == 1
  // Diginal buttons B and C for Amiga/Atari ST (pot X and pot Y on C64) are
  // enabled only on "unijoysticle single port" mode.
  gpio_set_level(gpios[5], !!joy->pot_x);
  gpio_set_level(gpios[6], !!joy->pot_y);
#endif  // UNIJOYSTICLE_SINGLE_PORT == 0
}

static void event_loop(void* arg) {
  // timeout of 10s
  const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;
  while (1) {
    EventBits_t uxBits = xEventGroupWaitBits(
        g_event_group, (EVENT_BIT_MOUSE | EVENT_BIT_BUTTON | EVENT_BIT_POT),
        pdTRUE, pdFALSE, xTicksToWait);

    // timeout ?
    if (uxBits == 0) continue;

    if (uxBits & EVENT_BIT_MOUSE) handle_event_mouse();

    if (uxBits & EVENT_BIT_BUTTON) handle_event_button();

    if (uxBits & EVENT_BIT_POT) handle_event_pot();
  }
}

static void auto_fire_loop(void* arg) {
  // timeout of 10s
  const TickType_t xTicksToWait = 10000 / portTICK_PERIOD_MS;
  const TickType_t delayTicks = AUTOFIRE_FREQ_MS / portTICK_PERIOD_MS;
  while (1) {
    EventBits_t uxBits = xEventGroupWaitBits(
        g_auto_fire_group, EVENT_BIT_AUTOFIRE, pdTRUE, pdFALSE, xTicksToWait);

    // timeout ?
    if (uxBits == 0) continue;

    while (g_autofire_a_enabled || g_autofire_b_enabled) {
      if (g_autofire_a_enabled) gpio_set_level(JOY_A_PORTS[4], 1);
      if (g_autofire_b_enabled) gpio_set_level(JOY_B_PORTS[4], 1);

      vTaskDelay(delayTicks);

      if (g_autofire_a_enabled) gpio_set_level(JOY_A_PORTS[4], 0);
      if (g_autofire_b_enabled) gpio_set_level(JOY_B_PORTS[4], 0);

      vTaskDelay(delayTicks);
    }
  }
}

// Mouse handler
void handle_event_mouse() {
  // Copy global variables to local, in case they changed.
  int delta_x = g_delta_x;
  int delta_y = g_delta_y;

  // Should not happen, but better safe than sorry
  if (delta_x == 0 && delta_y == 0) return;

  int dir_x = 0;
  int dir_y = 0;
  float a = atan2f(delta_y, delta_x) + M_PI;
  float d = a * (180.0 / M_PI);
  logd("x=%d, y=%d, r=%f, d=%f\n", delta_x, delta_y, a, d);

  if (d < 60 || d >= 300) {
    // Moving left? (a<60, a>=300)
    dir_x = -1;
  } else if (d > 120 && d <= 240) {
    // Moving right? (120 < a <= 240)
    dir_x = 1;
  }

  if (d > 30 && d <= 150) {
    // Moving down? (30 < a <= 150)
    dir_y = -1;
  } else if (d > 210 && d <= 330) {
    // Moving up?
    dir_y = 1;
  }

  int delay = MOUSE_DELAY_BETWEEN_EVENT_US - (abs(delta_x) + abs(delta_y)) * 3;

  if (dir_y != 0) {
    mouse_move_y(dir_y, delay);
  }
  if (dir_x != 0) {
    mouse_move_x(dir_x, delay);
  }
}

static void mouse_send_move(int pin_a, int pin_b, uint32_t delay) {
  gpio_set_level(pin_a, 1);
  delay_us(delay);
  gpio_set_level(pin_b, 1);
  delay_us(delay);

  gpio_set_level(pin_a, 0);
  delay_us(delay);
  gpio_set_level(pin_b, 0);
  delay_us(delay);

  vTaskDelay(0);
}

static void mouse_move_x(int dir, uint32_t delay) {
  // up, down, left, right, fire
  if (dir < 0)
    mouse_send_move(JOY_A_PORTS[0], JOY_A_PORTS[1], delay);
  else
    mouse_send_move(JOY_A_PORTS[1], JOY_A_PORTS[0], delay);
}

static void mouse_move_y(int dir, uint32_t delay) {
  // up, down, left, right, fire
  if (dir < 0)
    mouse_send_move(JOY_A_PORTS[2], JOY_A_PORTS[3], delay);
  else
    mouse_send_move(JOY_A_PORTS[3], JOY_A_PORTS[2], delay);
}

// Delay in microseconds. Anything bigger than 1000 microseconds (1 millisecond)
// should be scheduled using vTaskDelay(), which will allow context-switch and
// allow other tasks to run.
static void delay_us(uint32_t delay) {
  if (delay > 1000)
    vTaskDelay(delay / 1000);
  else
    ets_delay_us(delay);
}

static void handle_event_pot() {
  // gpio_set_level(GPIO_NUM_13, 1);
  // ets_delay_us(50 /*223*/);
  // gpio_set_level(GPIO_NUM_13, 0);

  // gpio_set_level(GPIO_NUM_13, 0);
  // ets_delay_us(200 /*235*/);
  // ets_delay_us(g_pot_y);
  // gpio_set_level(GPIO_NUM_13, 1);
}

static void IRAM_ATTR gpio_isr_handler_button(void* arg) {
  // button released ?
  if (gpio_get_level(GPIO_PUSH_BUTTON)) {
    g_last_time_pressed_us = esp_timer_get_time();
    return;
  }

  // button pressed
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xEventGroupSetBitsFromISR(g_event_group, EVENT_BIT_BUTTON,
                            &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken == pdTRUE) portYIELD_FROM_ISR();
}

#if UNI_ENABLE_POT
static void IRAM_ATTR gpio_isr_handler_pot(void* arg) {
  uint32_t gpio_num = (uint32_t)arg;
  (void)gpio_num;

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xEventGroupSetBitsFromISR(g_event_group, EVENT_BIT_POT,
                            &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken == pdTRUE) portYIELD_FROM_ISR();
}
#endif  // UNI_ENABLE_POT

static void handle_event_button() {
  // FIXME: Debouncer might fail when releasing the button. Implement something
  // like this one:
  // https://hackaday.com/2015/12/10/embed-with-elliot-debounce-your-noisy-buttons-part-ii/
  const int64_t button_threshold_time_us = 300 * 1000;  // 300ms
  static int enabled = 0;

  // Regardless of the state, ignore the event if not enough time passed.
  int64_t now = esp_timer_get_time();
  if ((now - g_last_time_pressed_us) < button_threshold_time_us) return;

  g_last_time_pressed_us = now;

  // "up" button is released. Ignore event.
  if (gpio_get_level(GPIO_PUSH_BUTTON)) {
    return;
  }

  // "down", button pressed.
  logi("handle_event_button: %d -> %d\n", enabled, !enabled);
  enabled = !enabled;

  // Change emulation mode
  int num_devices = 0;
  uni_hid_device_t* d = NULL;
  for (int j = 0; j < UNI_HID_DEVICE_MAX_DEVICES; j++) {
    uni_hid_device_t* tmp_d = uni_hid_device_get_instance_for_idx(j);
    if (bd_addr_cmp(tmp_d->address, zero_addr) != 0 &&
        tmp_d->hid_control_cid != 0 && tmp_d->hid_interrupt_cid != 0) {
      num_devices++;
      d = tmp_d;
    }
  }

  if (d == NULL) {
    loge("c64: Cannot find valid HID devicen\n");
  }

  if (num_devices != 1) {
    loge("c64: cannot change mode. Expected num_devices=1, actual=%d\n",
         num_devices);
    return;
  }

  c64_instance_t* ins = get_c64_instance(d);

  if (ins->emu_mode == EMULATION_MODE_SINGLE_JOY) {
    ins->emu_mode = EMULATION_MODE_COMBO_JOY_JOY;
    ins->prev_gamepad_seat = ins->gamepad_seat;
    set_gamepad_seat(d, GAMEPAD_SEAT_A | GAMEPAD_SEAT_B);
    logi("c64: Emulation mode = Combo Joy Joy\n");
  } else if (ins->emu_mode == EMULATION_MODE_COMBO_JOY_JOY) {
    ins->emu_mode = EMULATION_MODE_SINGLE_JOY;
    set_gamepad_seat(d, ins->prev_gamepad_seat);
    // Turn on only the valid one
    logi("c64: Emulation mode = Single Joy\n");
  } else {
    loge("c64: Cannot switch emu mode. Current mode: %d\n", ins->emu_mode);
  }
}

struct uni_platform* uni_platform_c64_create(void) {
  static struct uni_platform plat;

  plat.name = "Unijoysticle for the C64";
  plat.on_init = c64_on_init;
  plat.on_init_complete = c64_on_init_complete;
  plat.on_device_connected = c64_on_device_connected;
  plat.on_device_disconnected = c64_on_device_disconnected;
  plat.on_device_ready = c64_on_device_ready;
  plat.on_device_gamepad_event = c64_on_device_gamepad_event;
  plat.on_gamepad_data = c64_on_gamepad_data;
  plat.is_button_pressed = c64_is_button_pressed;

  return &plat;
}
