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

// Adafruit Matrix Portal has two ICs:
// - ESP32
// - ATSAMD51J19
// By default the ESP32 is used only for WiFi, and it is connected to the SAMD51
// using SPI.
// In our case, we replace the existing ESP32 firmware with Bluepad32, and we
// use the same SPI channel so that the SAMD51 can request: "give me the gamepad
// data"

// Adafruit Matrix Portal driver
#include "uni_platform_matrix_portal.h"

#include <driver/gpio.h>
#include <driver/spi_slave.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <math.h>

#include "uni_config.h"
#include "uni_debug.h"
#include "uni_hid_device.h"

// SPI et al pins
#define GPIO_MOSI GPIO_NUM_14
#define GPIO_MISO GPIO_NUM_23
#define GPIO_SCLK GPIO_NUM_18
#define GPIO_CS GPIO_NUM_5
#define GPIO_READY GPIO_NUM_33
#define DMA_CHANNEL 1

static SemaphoreHandle_t _ready_semaphore = NULL;

static int spi_transfer(uint8_t out[], uint8_t in[], size_t len) {
  spi_slave_transaction_t slvTrans;
  spi_slave_transaction_t* slvRetTrans;

  memset(&slvTrans, 0x00, sizeof(slvTrans));

  slvTrans.length = len * 8;
  slvTrans.trans_len = 0;
  slvTrans.tx_buffer = out;
  slvTrans.rx_buffer = in;

  spi_slave_queue_trans(VSPI_HOST, &slvTrans, portMAX_DELAY);
  xSemaphoreTake(_ready_semaphore, portMAX_DELAY);
  gpio_set_level(GPIO_READY, 0);
  spi_slave_get_trans_result(VSPI_HOST, &slvRetTrans, portMAX_DELAY);
  gpio_set_level(GPIO_READY, 1);

  return (slvTrans.trans_len / 8);
}

static int process_request(const uint8_t command_buf[], int command_len,
                           uint8_t out[]) {
  return 0;
}

#define SPI_BUFFER_LEN SPI_MAX_DMA_LEN
static void spi_main_loop(void* arg) {
  WORD_ALIGNED_ATTR uint8_t response_buf[SPI_BUFFER_LEN + 1];
  WORD_ALIGNED_ATTR uint8_t command_buf[SPI_BUFFER_LEN + 1];

  while (1) {
    memset(command_buf, 0, SPI_BUFFER_LEN);
    int command_len = spi_transfer(NULL, command_buf, SPI_BUFFER_LEN);

    // process request
    memset(response_buf, 0, SPI_BUFFER_LEN);
    int response_len = process_request(command_buf, command_len, response_buf);

    spi_transfer(response_buf, NULL, response_len);
  }
}

// Called after a transaction is queued and ready for pickup by master.
static void spi_post_setup_cb(spi_slave_transaction_t* trans) {
  xSemaphoreGiveFromISR(_ready_semaphore, NULL);

  // Create SPI main loop thread
  // Bluetooth code runs in Core 0.
  // In order to not interfere with it, this one should run in the other Core.
  xTaskCreatePinnedToCore(spi_main_loop, "spi_main_loop", 8192, NULL, 1, NULL, 1);
}

static void IRAM_ATTR isr_handler_on_chip_select(void* arg) {
  gpio_set_level(GPIO_READY, 1);
}

static void matrix_portal_init(int argc, const char** argv) {
  UNUSED(argc);
  UNUSED(argv);

  // SPI Initialization taken from Nina-fw, the ESP32 firmware used in Adafruit
  // Matrix Portal:
  // https://github.com/adafruit/nina-fw/blob/master/arduino/libraries/SPIS/src/SPIS.cpp

  // The Arduino-like API was converted to ESP32 API calls.

  // Arduino: pinMode(_readyPin, OUTPUT);
  gpio_set_direction(GPIO_READY, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode(GPIO_READY, GPIO_FLOATING);
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[GPIO_READY], PIN_FUNC_GPIO);

  // Arduino: digitalWrite(_readyPin, HIGH);
  gpio_set_level(GPIO_READY, 1);

  _ready_semaphore = xSemaphoreCreateCounting(1, 0);

  // Arduino: attachInterrupt(_csPin, onChipSelect, FALLING);
  gpio_set_intr_type(GPIO_CS, GPIO_INTR_NEGEDGE);
  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
  gpio_isr_handler_add(GPIO_CS, isr_handler_on_chip_select, (void*)GPIO_CS);
  gpio_intr_enable(GPIO_CS);

  // Configuration for the SPI bus
  spi_bus_config_t buscfg = {.mosi_io_num = GPIO_MOSI,
                             .miso_io_num = GPIO_MISO,
                             .sclk_io_num = GPIO_SCLK};

  // Configuration for the SPI slave interface
  spi_slave_interface_config_t slvcfg = {.mode = 0,
                                         .spics_io_num = GPIO_CS,
                                         .queue_size = 1,
                                         .flags = 0,
                                         .post_setup_cb = spi_post_setup_cb,
                                         .post_trans_cb = NULL};

  gpio_set_pull_mode(GPIO_MOSI, GPIO_FLOATING);
  gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLDOWN_ONLY);
  gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

  spi_slave_initialize(VSPI_HOST, &buscfg, &slvcfg, DMA_CHANNEL);
}

// Events
static void matrix_portal_on_init_complete(void) {}

static void matrix_portal_on_port_assign_changed(uni_joystick_port_t port) {}

static void matrix_portal_on_mouse_data(int32_t delta_x, int32_t delta_y,
                                        uint16_t buttons) {}

static void matrix_portal_on_joy_a_data(uni_joystick_t* joy) {}

static void matrix_portal_on_joy_b_data(uni_joystick_t* joy) {}

static uint8_t matrix_portal_is_button_pressed(void) { return 0; }

struct uni_platform* uni_platform_matrix_portal_create(void) {
  static struct uni_platform plat;

  plat.name = "Adafruit Matrix Portal";
  plat.init = matrix_portal_init;
  plat.on_init_complete = matrix_portal_on_init_complete;
  plat.on_port_assign_changed = matrix_portal_on_port_assign_changed;
  plat.on_joy_a_data = matrix_portal_on_joy_a_data;
  plat.on_joy_b_data = matrix_portal_on_joy_b_data;
  plat.on_mouse_data = matrix_portal_on_mouse_data;
  plat.is_button_pressed = matrix_portal_is_button_pressed;

  return &plat;
}
