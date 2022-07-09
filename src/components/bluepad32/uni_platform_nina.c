/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2020 Ricardo Quesada

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

// Arduino NINA is supposed to work with
// - ESP32 as a co-procesor
// - And the main processor (usually an ARM or AVR)
// The ESP32 is a SPI-slave witch should handle a pre-defined set of requests.
// Instead of implementing all of these pre-defined requests, we add our own
// gamepad-related requests.

// Logic based on Adafruit NINA-fw code: https://github.com/adafruit/nina-fw
//
// This firmware should work on all Arduino boards with an NINA-W10x
// - Arduino Nano RP2040 Connect
// - Arduino Nano 33 IoT
// - Arduino MKR WiFi 1010

#include "uni_platform_nina.h"

#include <driver/gpio.h>
#include <driver/spi_slave.h>
#include <driver/uart.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <math.h>

#include "sdkconfig.h"
#include "uni_bluetooth.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_debug.h"
#include "uni_esp32.h"
#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_platform.h"
#include "uni_version.h"

// SPI et al pins
// NINA / AirLift don't use the pre-designated IO_MUX pins for VSPI.
// Instead it uses the GPIO matrix, that might not be suitable for fast SPI
// communications.
// #define GPIO_MOSI GPIO_NUM_23
// #define GPIO_MISO GPIO_NUM_19

// The only difference between NINA and AirLift, seems to be the MOSI pin.
#ifdef CONFIG_BLUEPAD32_PLATFORM_AIRLIFT
#define GPIO_MOSI GPIO_NUM_14
#elif defined(CONFIG_BLUEPAD32_PLATFORM_NINA)
#define GPIO_MOSI GPIO_NUM_12
#else
// FIXME: This file should not be compiled when NINA/AirLift is not used.
#define GPIO_MOSI GPIO_NUM_0
#endif
#define GPIO_MISO GPIO_NUM_23
#define GPIO_SCLK GPIO_NUM_18
#define GPIO_CS GPIO_NUM_5
#define GPIO_READY GPIO_NUM_33
#define DMA_CHANNEL 1

enum {
    NINA_GAMEPAD_INVALID = -1,
};

// Instead of using the uni_gamepad, we create one.
// This is because this "struct" is sent via the wire and the format, padding,
// etc. must not change.
typedef struct __attribute__((packed)) {
    // Used to tell "master" who is the owner of this data. 4 gamepads can be
    // connected, this value indicates which gamepad it is. Goes from 0 to 3.
    uint8_t idx;

    // Usage Page: 0x01 (Generic Desktop Controls)
    uint8_t dpad;
    int32_t axis_x;
    int32_t axis_y;
    int32_t axis_rx;
    int32_t axis_ry;

    // Usage Page: 0x02 (Sim controls)
    int32_t brake;
    int32_t throttle;

    // Usage Page: 0x09 (Button)
    uint16_t buttons;

    // Misc buttons (from 0x0c (Consumer) and others)
    uint8_t misc_buttons;

    // Not related to the state:
    uint8_t type;  // model: PS4, PS3, Xbox, etc..?
                   // Should be moved to nina_gamepad_properties_t
} nina_gamepad_t;

enum {
    PROPERTY_FLAG_RUMBLE = BIT(0),
    PROPERTY_FLAG_PLAYER_LEDS = BIT(1),
    PROPERTY_FLAG_PLAYER_LIGHTBAR = BIT(2),

    PROPERTY_FLAG_GAMEPAD = BIT(13),
    PROPERTY_FLAG_MOUSE = BIT(14),
    PROPERTY_FLAG_KEYBOARD = BIT(15),
};

// This is sent via the wire. Adding new properties at the end Ok.
// If so, update Protocol version.
typedef struct __attribute__((packed)) {
    uint8_t idx;          // Device index
    uint8_t btaddr[6];    // BT Addr
    uint8_t type;         // model: copy from nina_gamepad_t
    uint8_t subtype;      // subtype. E.g: Wii Remote 2nd version
    uint16_t vendor_id;   // VID
    uint16_t product_id;  // PID
    uint16_t flags;       // Features like Rumble, LEDs, etc.
} nina_gamepad_properties_t;

//
// Globals
//
#ifdef CONFIG_BLUEPAD32_PLATFORM_AIRLIFT
static const char FIRMWARE_VERSION[] = "Bluepad32 for AirLift v" UNI_VERSION;
#elif defined(CONFIG_BLUEPAD32_PLATFORM_NINA)
static const char FIRMWARE_VERSION[] = "Bluepad32 for NINA v" UNI_VERSION;
#else
// FIXME: This file should not be compiled when NINA/AirLift is not used.
static const char FIRMWARE_VERSION[] = "";
#endif

// NINA device "instance"
typedef struct nina_instance_s {
    // Gamepad index, from 0 to CONFIG_BLUEPAD32_MAX_DEVICES
    // NINA_GAMEPAD_INVALID means gamepad was not assigned yet.
    int8_t gamepad_idx;
} nina_instance_t;
_Static_assert(sizeof(nina_instance_t) < HID_DEVICE_MAX_PLATFORM_DATA, "NINA intance too big");

static SemaphoreHandle_t _ready_semaphore = NULL;
static QueueHandle_t _pending_queue = NULL;
static SemaphoreHandle_t _gamepad_mutex = NULL;
static nina_gamepad_t _gamepads[CONFIG_BLUEPAD32_MAX_DEVICES];
static nina_gamepad_properties_t _gamepads_properties[CONFIG_BLUEPAD32_MAX_DEVICES];
static volatile uni_gamepad_seat_t _gamepad_seats;

static nina_instance_t* get_nina_instance(uni_hid_device_t* d);
static uint8_t predicate_nina_index(uni_hid_device_t* d, void* data);

//
//
// Shared by CPU0 / CPU1
//
// BTStack / Bluepad32 are not thread safe.
// This code is the bridge between CPU1 and CPU0.
// CPU1 (SPI-slave) queues possible commands here,
// CPU0 will read from them and execute the commands.
//
//
enum {
    PENDING_REQUEST_CMD_NONE = 0,
    PENDING_REQUEST_CMD_LIGHTBAR_COLOR = 1,
    PENDING_REQUEST_CMD_PLAYER_LEDS = 2,
    PENDING_REQUEST_CMD_RUMBLE = 3,
};

typedef struct {
    uint8_t device_idx;
    uint8_t cmd;
    uint8_t args[8];
} pending_request_t;

#define MAX_PENDING_REQUESTS 16

//
//
// CPU1 - CPU1 - CPU1
//
// BTStack / Bluepad32 are not thread safe.
// Be extra careful when calling code that runs on the other CPU
//
//

//
// SPI / NINA-fw related
//

static int spi_transfer(uint8_t out[], uint8_t in[], size_t len) {
    spi_slave_transaction_t* slv_ret_trans;

    spi_slave_transaction_t slv_trans = {.length = len * 8, .trans_len = 0, .tx_buffer = out, .rx_buffer = in};

    esp_err_t ret = spi_slave_queue_trans(VSPI_HOST, &slv_trans, portMAX_DELAY);
    if (ret != ESP_OK)
        return -1;

    xSemaphoreTake(_ready_semaphore, portMAX_DELAY);
    gpio_set_level(GPIO_READY, 0);

    ret = spi_slave_get_trans_result(VSPI_HOST, &slv_ret_trans, portMAX_DELAY);
    if (ret != ESP_OK)
        return -1;

    assert(slv_ret_trans == &slv_trans);

    gpio_set_level(GPIO_READY, 1);

    return (slv_trans.trans_len / 8);
}

// Possible answers when the request doesn't need an answer, like in "set_xxx".
enum {
    RESPONSE_ERROR = 0,
    RESPONSE_OK = 1,
};

// Command 0x00
static int request_protocol_version(const uint8_t command[], uint8_t response[]) {
#define PROTOCOL_VERSION_HI 0x01
#define PROTOCOL_VERSION_LO 0x02

    response[2] = 1;  // Number of parameters
    response[3] = 2;  // Param len
    response[4] = PROTOCOL_VERSION_HI;
    response[5] = PROTOCOL_VERSION_LO;

    return 6;
}

// Command 0x01
static int request_gamepads_data(const uint8_t command[], uint8_t response[]) {
    // Returned struct:
    // --- generic to all requests
    // byte 2: number of parameters (contains the number of gamepads)
    //      3: param len (sizeof(_gamepads[0])
    //      4: gamepad N data

    xSemaphoreTake(_gamepad_mutex, portMAX_DELAY);

    int total_gamepads = 0;
    int offset = 3;
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        if (_gamepad_seats & (1 << i)) {
            total_gamepads++;
            // Update param len
            response[offset] = sizeof(_gamepads[0]);
            // Update param (data)
            memcpy(&response[offset + 1], &_gamepads[i], sizeof(_gamepads[0]));
            offset += sizeof(_gamepads[0]) + 1;
        }
    }

    response[2] = total_gamepads;  // total params

    xSemaphoreGive(_gamepad_mutex);

    // "offset" has the total length
    return offset;
}

// Command 0x02
static int request_set_gamepad_player_leds(const uint8_t command[], uint8_t response[]) {
    // command[2]: total params
    // command[3]: param len
    int idx = command[4];
    // command[5]: param len
    // command[6]: leds

    pending_request_t request = (pending_request_t){
        .device_idx = idx,
        .cmd = PENDING_REQUEST_CMD_PLAYER_LEDS,
        .args[0] = command[6],
    };
    xQueueSendToBack(_pending_queue, &request, (TickType_t)0);

    // TODO: We really don't know whether this request will succeed
    response[2] = 1;  // Number of parameters
    response[3] = 1;  // Lenghts of each parameter
    response[4] = RESPONSE_OK;

    return 5;
}

// Command 0x03
static int request_set_gamepad_color_led(const uint8_t command[], uint8_t response[]) {
    // command[2]: total params
    // command[3]: param len
    int idx = command[4];
    // command[5]: param len
    // command[6-8]: RGB

    pending_request_t request = (pending_request_t){
        .device_idx = idx,
        .cmd = PENDING_REQUEST_CMD_LIGHTBAR_COLOR,
        .args[0] = command[6],
        .args[1] = command[7],
        .args[2] = command[8],
    };
    xQueueSendToBack(_pending_queue, &request, (TickType_t)0);

    // TODO: We really don't know whether this request will succeed
    response[2] = 1;  // Number of parameters
    response[3] = 1;  // Lenghts of each parameter
    response[4] = RESPONSE_OK;

    return 5;
}

// Command 0x04
static int request_set_gamepad_rumble(const uint8_t command[], uint8_t response[]) {
    // command[2]: total params
    // command[3]: param len
    int idx = command[4];
    // command[5]: param len
    // command[6,7]: force, duration

    pending_request_t request = (pending_request_t){
        .device_idx = idx,
        .cmd = PENDING_REQUEST_CMD_RUMBLE,
        .args[0] = command[6],
        .args[1] = command[7],
    };
    xQueueSendToBack(_pending_queue, &request, (TickType_t)0);

    // TODO: We really don't know whether this request will succeed
    response[2] = 1;  // Number of parameters
    response[3] = 1;  // Param len
    response[4] = RESPONSE_OK;

    return 5;
}

// Command 0x05
static int request_forget_bluetooth_keys(const uint8_t command[], uint8_t response[]) {
    response[2] = 1;  // Number of parameters
    response[3] = 1;  // Param len
    response[4] = RESPONSE_OK;

    uni_bluetooth_del_keys_safe();
    return 5;
}

// Command 0x06
static int request_get_gamepad_properties(const uint8_t command[], uint8_t response[]) {
    // command[2]: total params
    // command[3]: param len
    int idx = command[4];

    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES) {
        // To be consistent with the "OK" case, we return 2 parameters on "Error".
        response[2] = 2;  // Number of parameters
        response[3] = 1;  // Param len
        response[4] = RESPONSE_ERROR;
        response[5] = 1;  // Param len
        response[6] = 0;  // Ignore
        return 7;
    };

    response[2] = 2;                                // Number of parameters
    response[3] = 1;                                // Param len
    response[4] = RESPONSE_OK;                      // Ok
    response[5] = sizeof(_gamepads_properties[0]);  // Param len

    xSemaphoreTake(_gamepad_mutex, portMAX_DELAY);
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        if (_gamepads_properties[i].idx == idx) {
            memcpy(&response[6], &_gamepads_properties[i], sizeof(_gamepads_properties[0]));
            break;
        }
    }
    xSemaphoreGive(_gamepad_mutex);

    return 6 + sizeof(nina_gamepad_properties_t);
}

// Command 0x07
static int request_enable_bluetooth_connections(const uint8_t command[], uint8_t response[]) {
    bool enabled = command[4];
    uni_bluetooth_enable_new_connections_safe(enabled);

    response[2] = 1;  // total params
    response[3] = 1;  // param len
    response[4] = RESPONSE_OK;

    return 5;
}

// Command 0x1a
static int request_set_debug(const uint8_t command[], uint8_t response[]) {
    uni_esp32_enable_uart_output(command[4]);
    response[2] = 1;           // total params
    response[3] = 1;           // param len
    response[4] = command[4];  // return the value requested

    return 5;
}

// Command 0x20
// This is to make the default "CheckFirmwareVersion" sketch happy.
// Taken from wl_definitions.h
// See:
// https://github.com/arduino-libraries/WiFiNINA/blob/master/src/utility/wl_definitions.h
enum { WL_IDLE_STATUS = 0 };
static int request_get_conn_status(const uint8_t command[], uint8_t response[]) {
    response[2] = 1;  // total params
    response[3] = 1;  // param len
    response[4] = WL_IDLE_STATUS;

    return 5;
}

// Command 0x37
static int request_get_fw_version(const uint8_t command[], uint8_t response[]) {
    response[2] = 1;                         // Number of parameters
    response[3] = sizeof(FIRMWARE_VERSION);  // Parameter 1 length

    memcpy(&response[4], FIRMWARE_VERSION, sizeof(FIRMWARE_VERSION));

    return 4 + sizeof(FIRMWARE_VERSION);
}

// Command 0x50
static int request_set_pin_mode(const uint8_t command[], uint8_t response[]) {
    enum {
        INPUT = 0,
        OUTPUT = 1,
        INPUT_PULLUP = 2,
    };

    // command[2]: total params, should be 2
    // command[3]: param len, should be 1
    uint8_t pin = command[4];
    // command[5]: param 2 len: should be 1
    uint8_t mode = command[6];

    // Taken from Arduino pinMode()
    switch (mode) {
        case INPUT:
            gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
            gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING);
            break;

        case OUTPUT:
            gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
            gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING);
            break;

        case INPUT_PULLUP:
            gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
            gpio_set_pull_mode((gpio_num_t)pin, GPIO_PULLUP_ONLY);
            break;
    }
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);

    response[2] = 1;  // number of parameters
    response[3] = 1;  // parameter 1 length
    response[4] = RESPONSE_OK;
    return 5;
}

// Command 0x51
static int request_digital_write(const uint8_t command[], uint8_t response[]) {
    // command[2]: total params, should be 2
    // command[3]: param len, should be 1
    uint8_t pin = command[4];
    // command[5]: param len, should be 1
    uint8_t value = command[6];

    gpio_set_level((gpio_num_t)pin, value);

    response[2] = 1;  // total parameters
    response[3] = 1;  // param len
    response[4] = RESPONSE_OK;
    return 5;
}

// Command 0x53
static int request_digital_read(const uint8_t command[], uint8_t response[]) {
    // command[2]: total params, should be 1
    // command[3]: param len, should be 1
    uint8_t pin = command[4];

    uint8_t value = gpio_get_level(pin);

    response[2] = 1;      // number of parameters
    response[3] = 1;      // parameter 1 length
    response[4] = value;  // response
    return 5;
}

typedef int (*command_handler_t)(const uint8_t command[], uint8_t response[] /* out */);
const command_handler_t command_handlers[] = {
    // 0x00 -> 0x0f: Bluepad32 own extensions
    // These 16 entries are NULL in NINA. Perhaps they are reserved for future
    // use? Seems to be safe to use them for Bluepad32 commands.
    request_protocol_version,
    request_gamepads_data,                 // data
    request_set_gamepad_player_leds,       // the 4 LEDs that is available in many
                                           // gamepads.
    request_set_gamepad_color_led,         // available on DS4, DualSense
    request_set_gamepad_rumble,            // available on DS4, Xbox, Switch, etc.
    request_forget_bluetooth_keys,         // forget stored Bluetooth keys
    request_get_gamepad_properties,        // get gamepad properties like BTAddr, VID/PID, etc.
    request_enable_bluetooth_connections,  // Enable/Disable bluetooth connection
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

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
    NULL,
    NULL,
    NULL,
    NULL,

    // 0x20 -> 0x2f
    request_get_conn_status,  // getConnStatus (0x20)
    NULL,                     // getIPaddr,
    NULL,                     // getMACaddr,
    NULL,                     // getCurrSSID,
    NULL,                     // getCurrBSSID,
    NULL,                     // getCurrRSSI,
    NULL,                     // getCurrEnct,
    NULL,                     // scanNetworks,
    NULL,                     // startServerTcp,
    NULL,                     // getStateTcp,
    NULL,                     // dataSentTcp,
    NULL,                     // availDataTcp,
    NULL,                     // getDataTcp,
    NULL,                     // startClientTcp,
    NULL,                     // stopClientTcp,
    NULL,                     // getClientStateTcp,

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
    NULL,
    NULL,
    NULL,  // sendDataTcp,
    NULL,  // getDataBufTcp,
    NULL,  // insertDataBuf,
    NULL,
    NULL,
    NULL,
    NULL,  // wpa2EntSetIdentity,
    NULL,  // wpa2EntSetUsername,
    NULL,  // wpa2EntSetPassword,
    NULL,  // wpa2EntSetCACert,
    NULL,  // wpa2EntSetCertKey,
    NULL,  // wpa2EntEnable,

    // 0x50 -> 0x5f
    request_set_pin_mode,   // setPinMode,
    request_digital_write,  // setDigitalWrite,
    NULL,                   // setAnalogWrite,
    request_digital_read,   // setDigitalRead,
    NULL,                   // setAnalogRead,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

    // 0x60 -> 0x6f
    NULL,  // writeFile,
    NULL,  // readFile,
    NULL,  // deleteFile,
    NULL,  // existsFile,
    NULL,  // downloadFile,
    NULL,  // applyOTA,
    NULL,  // renameFile,
    NULL,  // downloadOTA,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};
#define COMMAND_HANDLERS_MAX (sizeof(command_handlers) / sizeof(command_handlers[0]))

static int process_request(const uint8_t command[], int command_len, uint8_t response[] /* out */) {
    // NINA-fw commands. Taken from:
    // https://github.com/arduino-libraries/WiFiNINA/blob/master/src/utility/wifi_spi.h
    // https://github.com/adafruit/Adafruit_CircuitPython_ESP32SPI/blob/master/adafruit_esp32spi/adafruit_esp32spi.py
    enum {
        CMD_START = 0xe0,
        CMD_END = 0xee,
        CMD_ERR = 0xef,
        CMD_REPLY_FLAG = 1 << 7,
    };

    int response_len = 0;
    /* Cmd Struct Message, from:
    https://github.com/arduino-libraries/WiFiNINA/blob/master/src/utility/spi_drv.cpp
     ________________________________________________________________________
    | START CMD | C/R  | CMD  | N.PARAM | PARAM LEN | PARAM  | .. | END CMD |
    |___________|______|______|_________|___________|________|____|_________|
    |   8 bit   | 1bit | 7bit |  8bit   |   8bit    | nbytes | .. |   8bit  |
    |___________|______|______|_________|___________|________|____|_________|
    */

    if (command_len >= 2 && command[0] == CMD_START && command[1] < COMMAND_HANDLERS_MAX) {
        command_handler_t command_handler = command_handlers[command[1]];

        if (command_handler) {
            // To make the code "compatible", we pass "command" to all the request
            // handlers. On an ideal world, we should pass &command[2] instead.
            response_len = command_handler(command, response);
        }
    }

    if (response_len <= 0) {
        loge("NINA: Error in request:\n");
        printf_hexdump(command, command_len);
        // Response for invalid requests
        response[0] = CMD_ERR;
        response[1] = 0x00;
        response[2] = CMD_END;

        response_len = 3;
    } else {
        response[0] = CMD_START;
        response[1] = (command[1] | CMD_REPLY_FLAG);

        // Add extra byte to indicate end of command
        response[response_len] = CMD_END;
        response_len++;
    }

    return response_len;
}

// Called after a transaction is queued and ready for pickup by master.
static void IRAM_ATTR spi_post_setup_cb(spi_slave_transaction_t* trans) {
    ARG_UNUSED(trans);
    xSemaphoreGiveFromISR(_ready_semaphore, NULL);
}

static void IRAM_ATTR isr_handler_on_chip_select(void* arg) {
    gpio_set_level(GPIO_READY, 1);
}

static void spi_main_loop(void* arg) {
    // Small delay to let CPU0 finish initialization. This is to prevent
    // collision in the log(). No harm is done if there is collision, only
    // that it is more difficult to read the logs from the console.
    vTaskDelay(50 / portTICK_PERIOD_MS);

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

    // Enable pull-ups on SPI lines so we don't detect rogue pulses when no
    // master is connected. gpio_set_pull_mode(GPIO_MOSI, GPIO_PULLUP_ONLY);
    // gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLUP_ONLY);
    // gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

    // Honor NINA-fw way of setting up these GPIOs.
    gpio_set_pull_mode(GPIO_MOSI, GPIO_FLOATING);
    gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLDOWN_ONLY);
    gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

    esp_err_t ret = spi_slave_initialize(VSPI_HOST, &buscfg, &slvcfg, DMA_CHANNEL);
    assert(ret == ESP_OK);

    // Must be modulo 4 and word aligned.
    // A higher value up to SPI_MAX_DMA_LEN can be defined if needed.
#define SPI_BUFFER_LEN 256
    WORD_ALIGNED_ATTR uint8_t response_buf[SPI_BUFFER_LEN];
    WORD_ALIGNED_ATTR uint8_t command_buf[SPI_BUFFER_LEN];

    while (1) {
        memset(command_buf, 0, SPI_BUFFER_LEN);
        int command_len = spi_transfer(NULL, command_buf, SPI_BUFFER_LEN);
        if (command_len == 0)
            continue;

        // process request
        memset(response_buf, 0, SPI_BUFFER_LEN);
        int response_len = process_request(command_buf, command_len, response_buf);

        spi_transfer(response_buf, NULL, response_len);
    }
}

//
//
// CPU0 - CPU0 - CPU0
//
// BTStack / Bluepad32 are not thread safe.
// Be extra careful when calling code that runs on the other CPU
//

static void process_pending_requests(void) {
    pending_request_t request;

    while (xQueueReceive(_pending_queue, &request, (TickType_t)0) == pdTRUE) {
        int idx = request.device_idx;
        uni_hid_device_t* d = uni_hid_device_get_instance_with_predicate(predicate_nina_index, (void*)idx);
        if (d == NULL) {
            loge("NINA: device cannot be found while processing pending request\n");
            return;
        }
        switch (request.cmd) {
            case PENDING_REQUEST_CMD_LIGHTBAR_COLOR:
                if (d->report_parser.set_lightbar_color != NULL)
                    d->report_parser.set_lightbar_color(d, request.args[0], request.args[1], request.args[2]);
                break;
            case PENDING_REQUEST_CMD_PLAYER_LEDS:
                if (d->report_parser.set_player_leds != NULL)
                    d->report_parser.set_player_leds(d, request.args[0]);
                break;

            case PENDING_REQUEST_CMD_RUMBLE:
                if (d->report_parser.set_rumble != NULL)
                    d->report_parser.set_rumble(d, request.args[0], request.args[1]);
                break;

            default:
                loge("NINA: Invalid pending command: %d\n", request.cmd);
        }
    }
}

//
// Platform Overrides
//
static void nina_init(int argc, const char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    // First things first:
    // Set READY pin as not-ready (HIGH) so that SPI-master doesn't start the
    // transaction while we are still booting

    // Arduino: pinMode(_readyPin, OUTPUT);
    gpio_set_direction(GPIO_READY, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(GPIO_READY, GPIO_FLOATING);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[GPIO_READY], PIN_FUNC_GPIO);

    // Arduino: digitalWrite(_readyPin, HIGH);
    gpio_set_level(GPIO_READY, 1);
}

static void nina_on_init_complete(void) {
    _gamepad_mutex = xSemaphoreCreateMutex();
    assert(_gamepad_mutex != NULL);

    _pending_queue = xQueueCreate(MAX_PENDING_REQUESTS, sizeof(pending_request_t));
    assert(_pending_queue != NULL);

    // Create SPI main loop thread
    // In order to not interfere with Bluetooth that runs in CPU0, SPI code
    // should run in CPU1
    xTaskCreatePinnedToCore(spi_main_loop, "spi_main_loop", 8192, NULL, 1, NULL, 1);
}

static void nina_on_device_connected(uni_hid_device_t* d) {
    nina_instance_t* ins = get_nina_instance(d);
    memset(ins, 0, sizeof(*ins));
    ins->gamepad_idx = NINA_GAMEPAD_INVALID;
}

static void nina_on_device_disconnected(uni_hid_device_t* d) {
    nina_instance_t* ins = get_nina_instance(d);
    // Only process it if the gamepad has been assigned before
    if (ins->gamepad_idx != NINA_GAMEPAD_INVALID) {
        if (ins->gamepad_idx < 0 || ins->gamepad_idx >= CONFIG_BLUEPAD32_MAX_DEVICES) {
            loge("NINA: unexpected gamepad idx, got: %d, want: [0-%d]\n", ins->gamepad_idx,
                 CONFIG_BLUEPAD32_MAX_DEVICES);
            return;
        }
        _gamepad_seats &= ~(1 << ins->gamepad_idx);
        memset(&_gamepads[ins->gamepad_idx], 0, sizeof(_gamepads[ins->gamepad_idx]));
        _gamepads[ins->gamepad_idx].idx = NINA_GAMEPAD_INVALID;
        memset(&_gamepads_properties[ins->gamepad_idx], 0, sizeof(_gamepads_properties[ins->gamepad_idx]));
        _gamepads_properties[ins->gamepad_idx].idx = NINA_GAMEPAD_INVALID;
        ins->gamepad_idx = NINA_GAMEPAD_INVALID;
    }
}

static int nina_on_device_ready(uni_hid_device_t* d) {
    if (_gamepad_seats == (GAMEPAD_SEAT_A | GAMEPAD_SEAT_B | GAMEPAD_SEAT_C | GAMEPAD_SEAT_D)) {
        // No more available seats, reject connection
        logi("NINA: More available seats\n");
        return -1;
    }

    nina_instance_t* ins = get_nina_instance(d);
    if (ins->gamepad_idx != -1) {
        loge("NINA: unexpected value for on_device_ready; got: %d, want: -1\n", ins->gamepad_idx);
        return -1;
    }

    // Find first available gamepad
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        if ((_gamepad_seats & (1 << i)) == 0) {
            ins->gamepad_idx = i;
            _gamepad_seats |= (1 << i);
            break;
        }
    }

    // This is how "client" knows which gamepad emitted the events.
    int idx = ins->gamepad_idx;
    _gamepads[idx].idx = ins->gamepad_idx;
    _gamepads[idx].type = d->controller_type;

    // FIXME: To save RAM gamepad_properties should be updated at "request time".
    // It requires to add a mutex in uni_hid_device, and that has its own issues.
    // As a quick hack, it is easier to copy them now.
    _gamepads_properties[idx].idx = ins->gamepad_idx;
    _gamepads_properties[idx].type = d->controller_type;
    _gamepads_properties[idx].subtype = d->controller_subtype;
    _gamepads_properties[idx].vendor_id = d->vendor_id;
    _gamepads_properties[idx].product_id = d->product_id;
    _gamepads_properties[idx].flags = (d->report_parser.set_player_leds ? PROPERTY_FLAG_PLAYER_LEDS : 0) |
                                      (d->report_parser.set_rumble ? PROPERTY_FLAG_RUMBLE : 0) |
                                      (d->report_parser.set_lightbar_color ? PROPERTY_FLAG_PLAYER_LIGHTBAR : 0);

    // TODO: Most probably a device cannot be a mouse a keyboard and a gamepad at the same time,
    // and 2 bits should be more than enough.
    // But for simplicity, let's use one bit for each category.
    if (uni_hid_device_is_mouse(d))
        _gamepads_properties[idx].flags |= PROPERTY_FLAG_MOUSE;

    if (uni_hid_device_is_keyboard(d))
        _gamepads_properties[idx].flags |= PROPERTY_FLAG_KEYBOARD;

    if (uni_hid_device_is_gamepad(d))
        _gamepads_properties[idx].flags |= PROPERTY_FLAG_GAMEPAD;

    memcpy(_gamepads_properties[ins->gamepad_idx].btaddr, d->conn.btaddr, sizeof(_gamepads_properties[0].btaddr));

    if (d->report_parser.set_player_leds != NULL) {
        d->report_parser.set_player_leds(d, (1 << ins->gamepad_idx));
    }
    return 0;
}

static uint8_t predicate_nina_index(uni_hid_device_t* d, void* data) {
    int wanted_idx = (int)data;
    nina_instance_t* ins = get_nina_instance(d);
    if (ins->gamepad_idx != wanted_idx)
        return 0;
    return 1;
}

static void nina_on_gamepad_data(uni_hid_device_t* d, uni_gamepad_t* gp) {
    // FIXME:
    // When SPI-slave (CPU1) receives a request that cannot be fulfilled
    // immediately (e.g: the ones that needs to run on CPU0), it is processed from
    // this callback. This works because a "gamepad_data" event is generated
    // almost immediately after the SPI-slave request was made. Although unlikely,
    // it could happen that "gamepad_data" gets called after a delay.
    process_pending_requests();

    nina_instance_t* ins = get_nina_instance(d);
    if (ins->gamepad_idx < 0 || ins->gamepad_idx >= CONFIG_BLUEPAD32_MAX_DEVICES) {
        loge("NINA: unexpected gamepad idx, got: %d, want: [0-%d]\n", ins->gamepad_idx, CONFIG_BLUEPAD32_MAX_DEVICES);
        return;
    }

    // Populate gamepad data on shared struct.
    xSemaphoreTake(_gamepad_mutex, portMAX_DELAY);
    _gamepads[ins->gamepad_idx].dpad = gp->dpad;
    _gamepads[ins->gamepad_idx].axis_x = gp->axis_x;
    _gamepads[ins->gamepad_idx].axis_y = gp->axis_y;
    _gamepads[ins->gamepad_idx].axis_rx = gp->axis_rx;
    _gamepads[ins->gamepad_idx].axis_ry = gp->axis_ry;
    _gamepads[ins->gamepad_idx].brake = gp->brake;
    _gamepads[ins->gamepad_idx].throttle = gp->throttle;
    _gamepads[ins->gamepad_idx].buttons = gp->buttons;
    _gamepads[ins->gamepad_idx].misc_buttons = gp->misc_buttons;
    xSemaphoreGive(_gamepad_mutex);
}

static void nina_on_oob_event(uni_platform_oob_event_t event, void* data) {
    ARG_UNUSED(event);
    ARG_UNUSED(data);
    // TODO: Do something ?
}

static int32_t nina_get_property(uni_platform_property_t key) {
    // FIXME: support well-known uni_platform_property_t keys
    return 0;
}

//
// Helpers
//
static nina_instance_t* get_nina_instance(uni_hid_device_t* d) {
    return (nina_instance_t*)&d->platform_data[0];
}

//
// Entry Point
//
struct uni_platform* uni_platform_nina_create(void) {
    static struct uni_platform plat = {
        .name = "Arduino NINA",
        .init = nina_init,
        .on_init_complete = nina_on_init_complete,
        .on_device_connected = nina_on_device_connected,
        .on_device_disconnected = nina_on_device_disconnected,
        .on_device_ready = nina_on_device_ready,
        .on_oob_event = nina_on_oob_event,
        .on_gamepad_data = nina_on_gamepad_data,
        .get_property = nina_get_property,
    };

    return &plat;
}

// AirLift and NINA are identical with the exception of the MOSI pin.
struct uni_platform* uni_platform_airlift_create(void) {
    static struct uni_platform plat = {
        .name = "Adafruit AirLift",
        .init = nina_init,
        .on_init_complete = nina_on_init_complete,
        .on_device_connected = nina_on_device_connected,
        .on_device_disconnected = nina_on_device_disconnected,
        .on_device_ready = nina_on_device_ready,
        .on_oob_event = nina_on_oob_event,
        .on_gamepad_data = nina_on_gamepad_data,
        .get_property = nina_get_property,
    };

    return &plat;
}
