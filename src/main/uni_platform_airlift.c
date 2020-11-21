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

// Logic based on Adafruit Nina-fw code: https://github.com/adafruit/nina-fw
//
// This firmware should work on all Adafruit "AirLift" family like:
// - AirLift
// - Matrix Portal
// - PyPortal

#include "uni_platform_airlift.h"

#include <driver/gpio.h>
#include <driver/spi_slave.h>
#include <driver/uart.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <math.h>

#include "uni_config.h"
#include "uni_debug.h"
#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_main_esp32.h"
#include "uni_platform.h"

// SPI et al pins
// AirLift doesn't use the pre-designated IO_MUX pins for VSPI.
// Instead it uses the GPIO matrix, that might not be suitable for fast SPI
// communications.
//#define GPIO_MOSI GPIO_NUM_23
//#define GPIO_MISO GPIO_NUM_19
#define GPIO_MOSI GPIO_NUM_14
#define GPIO_MISO GPIO_NUM_23
#define GPIO_SCLK GPIO_NUM_18
#define GPIO_CS GPIO_NUM_5
#define GPIO_READY GPIO_NUM_33
#define DMA_CHANNEL 1

// Gamepad related
// Arbitrary max number of gamepads that can be connected at the same time
#define AIRLIFT_MAX_GAMEPADS 4

//
// Globals
//
static const char FIRMWARE_VERSION[] = "Bluepad32 for Airlift v1.0";
static SemaphoreHandle_t _ready_semaphore = NULL;
static SemaphoreHandle_t _gamepad_mutex = NULL;
static uni_gamepad_t _gamepads[AIRLIFT_MAX_GAMEPADS];
static uni_gamepad_seat_t _gamepad_seats;

// Airlift device "instance"
typedef struct airlift_instance_s {
  // Gamepad index, from 0 to AIRLIFT_MAX_GAMEPADS
  // -1 means gamepad was not assigned yet.
  int8_t gamepad_idx;
} airlift_instance_t;

static airlift_instance_t* get_airlift_instance(uni_hid_device_t* d);

//
// SPI / Nina-fw related
//

static int spi_transfer(uint8_t out[], uint8_t in[], size_t len) {
  spi_slave_transaction_t* slv_ret_trans;

  spi_slave_transaction_t slv_trans = {
      .length = len * 8, .trans_len = 0, .tx_buffer = out, .rx_buffer = in};

  esp_err_t ret = spi_slave_queue_trans(VSPI_HOST, &slv_trans, portMAX_DELAY);
  if (ret != ESP_OK) return -1;

  xSemaphoreTake(_ready_semaphore, portMAX_DELAY);
  gpio_set_level(GPIO_READY, 0);

  ret = spi_slave_get_trans_result(VSPI_HOST, &slv_ret_trans, portMAX_DELAY);
  if (ret != ESP_OK) return -1;

  assert(slv_ret_trans == &slv_trans);

  gpio_set_level(GPIO_READY, 1);

  return (slv_trans.trans_len / 8);
}

// Command 0x1a
static int request_set_debug(const uint8_t command[], uint8_t response[]) {
  uni_esp32_enable_uart_output(command[4]);
  response[2] = 1;  // number of parameters
  response[3] = 1;  // parameter 1 length
  response[4] = command[4];

  return 5;
}

// Command 0x37
static int request_get_fw_version(const uint8_t command[], uint8_t response[]) {
  response[2] = 1;                         // number of parameters
  response[3] = sizeof(FIRMWARE_VERSION);  // parameter 1 length

  memcpy(&response[4], FIRMWARE_VERSION, sizeof(FIRMWARE_VERSION));

  return 4 + sizeof(FIRMWARE_VERSION);
}

// Command 0x60
static int request_gamepad_data(const uint8_t command[], uint8_t response[]) {
  // returned struct:
  // --- generic to all requests
  // byte 2: number of parameters (always 1 for 0x60)
  //      3: lenght of each parameter (always total size of payload for 0x60)
  // --- 0x60 specific
  //      4: number of gamepad reports. 0 == no reports
  // --- gamepad I
  //      5: gamepad seat (0, 1, 2 or 3)
  //      6-....N: gamepad data (see uni_gamepad_t)
  // --- gamepad I+1
  //      ...
  //
  // Instead of returning multiple parameters, one per gamepad, one parameter
  // is returned, of variable witdth.
  response[2] = 1;                        // number of parameters

  xSemaphoreTake(_gamepad_mutex, portMAX_DELAY);

  int total = 0;
  int offset = 5;
  for (int i=0;i < AIRLIFT_MAX_GAMEPADS; i++) {
    if (_gamepad_seats & (1 << i)) {
      total++;
      response[offset] = i;
      memcpy(&response[offset+1], &_gamepads[i], sizeof(_gamepads[0]));
      offset += 1 + sizeof(_gamepads[0]);
    }
  }

  response[3] = 1 + total * (1 + sizeof(_gamepads[0])); // parameter 1 length
  response[4] = total; // Number of gamepad reports

  xSemaphoreGive(_gamepad_mutex);

  // offset is the total lenght
  return offset;
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
    NULL,               // setIPconfig,
    NULL,               // setDNSconfig,
    NULL,               // setHostname,
    NULL,               // setPowerMode,
    NULL,               // setApNet,
    NULL,               // setApPassPhrase,
    request_set_debug,  // setDebug (0x1a)
    NULL,               // getTemperature,
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
    request_get_fw_version,  // getFwVersion (0x37)
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
    request_gamepad_data,  // request_gamepad_data
    NULL,                  // request_set_gamepad_rumble
    NULL,                  // request_set_gamepad_led_color
};
#define COMMAND_HANDLERS_MAX \
  (sizeof(command_handlers) / sizeof(command_handlers[0]))

// Nina-fw commands. Taken from:
// https://github.com/adafruit/Adafruit_CircuitPython_ESP32SPI/blob/master/adafruit_esp32spi/adafruit_esp32spi.py
enum {
  CMD_START = 0xe0,
  CMD_END = 0xee,
  CMD_ERR = 0xef,
  CMD_REPLY_FLAG = 1 << 7,
};

static int process_request(const uint8_t command[], int command_len,
                           uint8_t response[] /* out */) {
  int response_len = 0;

  if (command_len >= 2 && command[0] == 0xe0 &&
      command[1] < COMMAND_HANDLERS_MAX) {
    command_handler_t command_handler = command_handlers[command[1]];

    if (command_handler) {
      response_len = command_handler(command, response);
    }
  }

  if (response_len <= 0) {
    loge("Error in request:\n");
    printf_hexdump(command, command_len);
    // Response for invalid requests
    response[0] = CMD_ERR;
    response[1] = 0x00;
    response[2] = CMD_END;

    response_len = 3;
  } else {
    response[0] = CMD_START;
    response[1] = (CMD_REPLY_FLAG | command[1]);

    // Add extra byte to indicate end of command
    response[response_len] = CMD_END;
    response_len++;
  }

  return response_len;
}

// Called after a transaction is queued and ready for pickup by master.
static void IRAM_ATTR spi_post_setup_cb(spi_slave_transaction_t* trans) {
  UNUSED(trans);
  xSemaphoreGiveFromISR(_ready_semaphore, NULL);
}

static void IRAM_ATTR isr_handler_on_chip_select(void* arg) {
  gpio_set_level(GPIO_READY, 1);
}

static void spi_main_loop(void* arg) {
  _gamepad_mutex = xSemaphoreCreateMutex();
  assert(_gamepad_mutex != NULL);

  // Small delay to let CPU0 finish initialization. This is to prevent collision
  // in the log(). No harm is done if there is collision, only that it is more
  // difficult to read the logs from the console.
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  // From SPIS.cpp SPISClass::being()

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
                             .sclk_io_num = GPIO_SCLK,
                             .quadwp_io_num = -1,
                             .quadhd_io_num = -1};

  // Configuration for the SPI slave interface
  spi_slave_interface_config_t slvcfg = {.mode = 0,
                                         .spics_io_num = GPIO_CS,
                                         .queue_size = 1,
                                         .flags = 0,
                                         .post_setup_cb = spi_post_setup_cb,
                                         .post_trans_cb = NULL};

  // Enable pull-ups on SPI lines so we don't detect rogue pulses when no master
  // is connected. gpio_set_pull_mode(GPIO_MOSI, GPIO_PULLUP_ONLY);
  // gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLUP_ONLY);
  // gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

  // Honor Nina-fw way of setting up these GPIOs.
  gpio_set_pull_mode(GPIO_MOSI, GPIO_FLOATING);
  gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLDOWN_ONLY);
  gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

  esp_err_t ret =
      spi_slave_initialize(VSPI_HOST, &buscfg, &slvcfg, DMA_CHANNEL);
  assert(ret == ESP_OK);

  //#define SPI_BUFFER_LEN SPI_MAX_DMA_LEN

  // Must be modulo 4 and word aligned.
#define SPI_BUFFER_LEN 256
  WORD_ALIGNED_ATTR uint8_t response_buf[SPI_BUFFER_LEN];
  WORD_ALIGNED_ATTR uint8_t command_buf[SPI_BUFFER_LEN];

  while (1) {
    memset(command_buf, 0, SPI_BUFFER_LEN);
    int command_len = spi_transfer(NULL, command_buf, SPI_BUFFER_LEN);
    if (command_len == 0) continue;

    // process request
    memset(response_buf, 0, SPI_BUFFER_LEN);
    int response_len = process_request(command_buf, command_len, response_buf);

    spi_transfer(response_buf, NULL, response_len);
  }
}

//
// Platform Overrides
//
static void airlift_init(int argc, const char** argv) {
  UNUSED(argc);
  UNUSED(argv);
  logi("********** airlift_init()\n");
}

static void airlift_on_init_complete(void) {
  logi("************  airlift_on_init_complete()\n");
  // Create SPI main loop thread
  // Bluetooth code runs in Core 0.
  // In order to not interfere with it, this one should run in the other Core.
  xTaskCreatePinnedToCore(spi_main_loop, "spi_main_loop", 8192, NULL, 1, NULL,
                          1);
}

static void airlift_on_device_connected(uni_hid_device_t* d) {
  airlift_instance_t* ins = get_airlift_instance(d);
  memset(ins, 0, sizeof(*ins));
  ins->gamepad_idx = -1;
}

static void airlift_on_device_disconnected(uni_hid_device_t* d) {
  airlift_instance_t* ins = get_airlift_instance(d);
  // Process it only if the gamepad has been assigned before.
  if (ins->gamepad_idx != -1) {
    ins->gamepad_idx = -1;
    _gamepad_seats &= ~(1 << ins->gamepad_idx);
  }
}

static int airlift_on_device_ready(uni_hid_device_t* d) {
  if (_gamepad_seats ==
      (GAMEPAD_SEAT_A | GAMEPAD_SEAT_B | GAMEPAD_SEAT_C | GAMEPAD_SEAT_D)) {
    // No more available seats, reject connection;
    return -1;
  }

  airlift_instance_t* ins = get_airlift_instance(d);
  if (ins->gamepad_idx != -1) {
    loge("airlift: unexpected value for on_device_ready; got: %d, want: -1\n",
         ins->gamepad_idx);
  }

  // Find first available gamepad
  for (int i = 0; i < AIRLIFT_MAX_GAMEPADS; i++) {
    if ((_gamepad_seats & (1 << i)) == 0) {
      ins->gamepad_idx = i;
      _gamepad_seats |= (1 << i);
      break;
    }
  }

  if (d->report_parser.update_led != NULL) {
    airlift_instance_t* ins = get_airlift_instance(d);
    d->report_parser.update_led(d, (1 << ins->gamepad_idx));
  }
  return 0;
}

static void airlift_on_gamepad_data(uni_hid_device_t* d, uni_gamepad_t* gp) {
  airlift_instance_t* ins = get_airlift_instance(d);
  if (ins->gamepad_idx < 0 || ins->gamepad_idx >= AIRLIFT_MAX_GAMEPADS) {
    loge("Airlift: unexpected gamepad idx, got: %d, want: [0-%d]\n",
         ins->gamepad_idx, AIRLIFT_MAX_GAMEPADS);
    return;
  }

  xSemaphoreTake(_gamepad_mutex, portMAX_DELAY);
  _gamepads[ins->gamepad_idx] = *gp;
  xSemaphoreGive(_gamepad_mutex);
}

static void airlift_on_device_oob_event(uni_hid_device_t* d,
                                        uni_platform_oob_event_t event) {
  logi("airlift_on_device_oob_event(), event=%d\n", event);
  if (event != UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON) return;

  // TODO: Do something ?
}

static int32_t airlift_get_property(uni_platform_property_t key) {
  logi("airlift_get_property(), key=%d\n", key);
  return 0;
}

//
// Helpers
//
static airlift_instance_t* get_airlift_instance(uni_hid_device_t* d) {
  return (airlift_instance_t*)&d->platform_data[0];
}

//
// Entry Point
//
struct uni_platform* uni_platform_airlift_create(void) {
  static struct uni_platform plat;

  plat.name = "Adafruit AirLift";
  plat.init = airlift_init;
  plat.on_init_complete = airlift_on_init_complete;
  plat.on_device_connected = airlift_on_device_connected;
  plat.on_device_disconnected = airlift_on_device_disconnected;
  plat.on_device_ready = airlift_on_device_ready;
  plat.on_device_oob_event = airlift_on_device_oob_event;
  plat.on_gamepad_data = airlift_on_gamepad_data;
  plat.get_property = airlift_get_property;

  return &plat;
}
