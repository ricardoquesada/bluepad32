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

// Adafruit AirLift is supposed to work with
// - ESP32 as a co-procesor
// - And the main processor (usually an ARM or AVR)
// The ESP32 is a SPI-slave witch should handle a pre-defined set of requests.
// Instead of implementing all of these pre-defined requests, we add our own
// gamepad-related requests.

// This firmware should work on all Adafruit "AirLift" family like:
// - AirLift
// - Matrix Portal
// - PyPortal

#include "uni_platform_airlift.h"

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
#include "uni_joystick.h"

// SPI et al pins
#define GPIO_MOSI GPIO_NUM_14
#define GPIO_MISO GPIO_NUM_23
#define GPIO_SCLK GPIO_NUM_18
#define GPIO_CS GPIO_NUM_5
#define GPIO_READY GPIO_NUM_33
#define DMA_CHANNEL 1

const char FIRMWARE_VERSION[] = "Bluepad32 for Adafruit AirLift v0.1";

static SemaphoreHandle_t _ready_semaphore = NULL;
static uni_joystick_t _gamepad0;

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

static int request_get_fw_version(const uint8_t command[], uint8_t response[]) {
  response[2] = 1;                         // number of parameters
  response[3] = sizeof(FIRMWARE_VERSION);  // parameter 1 length

  memcpy(&response[4], FIRMWARE_VERSION, sizeof(FIRMWARE_VERSION));

  return 5 + sizeof(FIRMWARE_VERSION);
}

static int request_connected_gamepads(const uint8_t command[],
                                      uint8_t response[]) {
  response[2] = 1;  // number of parameters
  response[3] = 1;  // parameter 1 length
  response[4] = 1;  // FIXME: hardcoded one connected gamepad

  return 6;
}

static int request_gamepad_data(const uint8_t command[], uint8_t response[]) {
  response[2] = 1;                  // number of parameters
  response[3] = sizeof(_gamepad0);  // parameter 1 lenght
  memcpy(&response[4], &_gamepad0, sizeof(_gamepad0));

  return 5 + sizeof(_gamepad0);
}

typedef int (*command_handler_t)(const uint8_t command[],
                                 uint8_t response[] /* out */);
const command_handler_t command_handlers[] = {
    // 0x00 -> 0x0f
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,

    // 0x10 -> 0x1f
    NULL,  // setNet
    NULL,  // setPassPhrase,
    NULL,  // setKey,
    NULL,
    NULL,  // setIPconfig,
    NULL,  // setDNSconfig,
    NULL,  // setHostname,
    NULL,  // setPowerMode,
    NULL,  // setApNet,
    NULL,  // setApPassPhrase,
    NULL,  // setDebug,
    NULL,  // getTemperature,
    NULL, NULL, NULL, NULL,

    // 0x20 -> 0x2f
    NULL,  // getConnStatus,
    NULL,  // getIPaddr,
    NULL,  // getMACaddr,
    NULL,  // getCurrSSID,
    NULL,  // getCurrBSSID,
    NULL,  // getCurrRSSI,
    NULL,  // getCurrEnct,
    NULL,  // scanNetworks,
    NULL,  // startServerTcp,
    NULL,  // getStateTcp,
    NULL,  // dataSentTcp,
    NULL,  // availDataTcp,
    NULL,  // getDataTcp,
    NULL,  // startClientTcp,
    NULL,  // stopClientTcp,
    NULL,  // getClientStateTcp,

    // 0x30 -> 0x3f
    NULL,  // disconnect,
    NULL,
    NULL,                    // getIdxRSSI,
    NULL,                    // getIdxEnct,
    NULL,                    // reqHostByName,
    NULL,                    // getHostByName,
    NULL,                    // startScanNetworks,
    request_get_fw_version,  // getFwVersion,
    NULL,
    NULL,  // sendUDPdata,
    NULL,  // getRemoteData,
    NULL,  // getTime,
    NULL,  // getIdxBSSID,
    NULL,  // getIdxChannel,
    NULL,  // ping,
    NULL,  // getSocket,

    // 0x40 -> 0x4f
    NULL,  // setClientCert,
    NULL,  // setCertKey,
    NULL, NULL,
    NULL,  // sendDataTcp,
    NULL,  // getDataBufTcp,
    NULL,  // insertDataBuf,
    NULL, NULL, NULL,
    NULL,  // wpa2EntSetIdentity,
    NULL,  // wpa2EntSetUsername,
    NULL,  // wpa2EntSetPassword,
    NULL,  // wpa2EntSetCACert,
    NULL,  // wpa2EntSetCertKey,
    NULL,  // wpa2EntEnable,

    // 0x50 -> 0x5f
    NULL,  // setPinMode,
    NULL,  // setDigitalWrite,
    NULL,  // setAnalogWrite,
    NULL,  // setDigitalRead,
    NULL,  // setAnalogRead,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

    // 0x60 -> 0x6f: Bluepad32 own extensions
    request_connected_gamepads,  // request_connected_gamepads
    request_gamepad_data,        // request_gamepad_data
    NULL,                        // request_set_gamepad_rumble
    NULL,                        // request_set_gamepad_led_color
};
#define COMMAND_HANDLERS_MAX \
  (sizeof(command_handlers) / sizeof(command_handlers[0]))

static int process_request(const uint8_t command[], int command_len,
                           uint8_t response[] /* out */) {
  if (command_len < 2) {
    loge("Airlift: invalid process_request lenght: got %d, want >= 2",
         command_len);
    return -1;
  }
  int response_len = 0;

  if (command[0] == 0xe0 && command[1] < COMMAND_HANDLERS_MAX) {
    command_handler_t command_handler = command_handlers[command[1]];

    if (command_handler) {
      response_len = command_handler(command, response);
    }
  }

  if (response_len <= 0) {
    response[0] = 0xef;
    response[1] = 0x00;
    response[2] = 0xee;

    response_len = 3;
  } else {
    response[0] = 0xe0;
    response[1] = (0x80 | command[1]);
    response[response_len - 1] = 0xee;
  }

  return response_len;
}

#define SPI_BUFFER_LEN SPI_MAX_DMA_LEN
static void spi_main_loop(void* arg) {
  WORD_ALIGNED_ATTR uint8_t response_buf[SPI_BUFFER_LEN + 1];
  WORD_ALIGNED_ATTR uint8_t command_buf[SPI_BUFFER_LEN + 1];

  while (1) {
    logi("************  while main loop\n");
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
  UNUSED(trans);
  logi("************  spi_post_setup_cb\n");
  xSemaphoreGiveFromISR(_ready_semaphore, NULL);

  // Create SPI main loop thread
  // Bluetooth code runs in Core 0.
  // In order to not interfere with it, this one should run in the other Core.
  xTaskCreatePinnedToCore(spi_main_loop, "spi_main_loop", 8192, NULL, 1, NULL,
                          1);
}

static void IRAM_ATTR isr_handler_on_chip_select(void* arg) {
  gpio_set_level(GPIO_READY, 1);
}

// Events
static void airlift_on_init(int argc, const char** argv) {
  UNUSED(argc);
  UNUSED(argv);
  logi("********** airlift_on_init()\n");
}

static void airlift_on_init_complete(void) {
  logi("************  airlift_on_init_complete()\n");
  // SPI Initialization taken from Nina-fw; the firmware used in Adafruit
  // AirLift products:
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

  esp_err_t ret =
      spi_slave_initialize(VSPI_HOST, &buscfg, &slvcfg, DMA_CHANNEL);
  logi("spi_slave_initialize = %d\n", ret);
  assert(ret == ESP_OK);

  // Manually put the ESP32 in upload mode so that the ESP32 UART is connected
  // with the main MCU UART.
  // digitalWrite(ESP32_GPIO0, LOW);
  //  gpio_set_level(GPIO_NUM_0, 0);

  // digitalWrite(ESP32_RESETN, LOW);
  //  gpio_set_level(GPIO_NUM_19, 0);
  // delay(100);
  //  vTaskDelay(100 / portTICK_PERIOD_MS);

  // digitalWrite(ESP32_RESETN, HIGH);
  //  gpio_set_level(GPIO_NUM_19, 1);
}

static void airlift_on_device_connected(uni_hid_device_t* d) {}

static void airlift_on_device_disconnected(uni_hid_device_t* d) {}

static int airlift_on_device_ready(uni_hid_device_t* d) { return 0; }

static void airlift_on_device_oob_event(uni_hid_device_t* d,
                                        uni_platform_oob_event_t event) {}

static void airlift_on_gamepad_data(uni_hid_device_t* d, uni_gamepad_t* gp) {}

static int32_t airlift_get_property(uni_platform_property_t key) { return 0; }

struct uni_platform* uni_platform_airlift_create(void) {
  static struct uni_platform plat;

  plat.name = "Adafruit AirLift";
  plat.on_init = airlift_on_init;
  plat.on_init_complete = airlift_on_init_complete;
  plat.on_device_connected = airlift_on_device_connected;
  plat.on_device_disconnected = airlift_on_device_disconnected;
  plat.on_device_ready = airlift_on_device_ready;
  plat.on_device_oob_event = airlift_on_device_oob_event;
  plat.on_gamepad_data = airlift_on_gamepad_data;
  plat.get_property = airlift_get_property;

  return &plat;
}
