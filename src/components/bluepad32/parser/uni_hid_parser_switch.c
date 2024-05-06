// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

// Technical info taken from:
// https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering
// https://github.com/DanielOgorchock/linux/blob/ogorchock/drivers/hid/hid-nintendo.c

#include "parser/uni_hid_parser_switch.h"

#include <assert.h>

#define ENABLE_SPI_FLASH_DUMP 0
#define ENABLE_IMU_REPORT 1

#if ENABLE_SPI_FLASH_DUMP
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // ENABLE_SPI_FLASH_DUMP

#include "bt/uni_bt_conn.h"
#include "controller/uni_controller.h"
#include "hid_usage.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

// Support for Nintendo Switch Pro gamepad and JoyCons.

static const uint16_t NINTENDO_VID = 0x057e;
static const uint16_t SWITCH_JOYCON_L_PID = 0x2006;
static const uint16_t SWITCH_JOYCON_R_PID = 0x2007;
static const uint16_t SWITCH_PRO_CONTROLLER_PID = 0x2009;
static const uint16_t SWITCH_ONLINE_SNES_CONTROLLER_PID = 0x2017;
// static const uint16_t SWITCH_ONLINE_N64_CONTROLLER_PID = 0x2019;
// static const uint16_t SWITCH_ONLINE_SEGA_CONTROLLER_PID = 0x201e;

#define SWITCH_FACTORY_STICK_CAL_DATA_SIZE 9
static const uint16_t SWITCH_FACTORY_STICK_CAL_DATA_ADDR_LEFT = 0x603d;
static const uint16_t SWITCH_FACTORY_STICK_CAL_DATA_ADDR_RIGHT = 0x6046;
#define SWITCH_USER_STICK_CAL_DATA_SIZE 2
static const uint16_t SWITCH_USER_STICK_CAL_DATA_ADDR = 0x8010;

// Constants taken from Linux kernel / Nintendo Rev.Eng doc
static const int16_t DEFAULT_ACCEL_OFFSET = 0;
static const int16_t DEFAULT_ACCEL_SCALE = 16384;
static const int16_t DEFAULT_GYRO_OFFSET = 0;
static const int16_t DEFAULT_GYRO_SCALE = 13371;
#define SWITCH_IMU_PREC_RANGE_SCALE 1000

#define SWITCH_FACTORY_IMU_CAL_DATA_SIZE 24
static const uint16_t SWITCH_FACTORY_IMU_CAL_DATA_ADDR = 0x6020;

#define SWITCH_DUMP_ROM_DATA_SIZE 24  // Max size is 24
#define SWITCH_SETUP_TIMEOUT_MS 600
#if ENABLE_SPI_FLASH_DUMP
static const uint32_t SWITCH_DUMP_ROM_DATA_ADDR_START = 0x20000;
static const uint32_t SWITCH_DUMP_ROM_DATA_ADDR_END = 0x30000;
#endif  // ENABLE_SPI_FLASH_DUMP

enum switch_state {
    STATE_UNINIT,
    STATE_SETUP,
    STATE_REQ_DEV_INFO,                    // What controller
    STATE_READ_FACTORY_STICK_CALIBRATION,  // Factory stick calibration info
    STATE_READ_USER_STICK_CALIBRATION,     // User calibration info
    STATE_READ_FACTORY_IMU_CALIBRATION,    // Factory IMU calibration info
    STATE_SET_FULL_REPORT,                 // Request report 0x30
    STATE_ENABLE_IMU,                      // Enable/Disable gyro/accel
    STATE_DUMP_FLASH,                      // Dump SPI Flash memory
    STATE_UPDATE_LED,                      // Update LEDs
    STATE_READY,                           // Gamepad setup ready!
};

enum switch_flags {
    SWITCH_MODE_NONE,    // Mode not set yet
    SWITCH_MODE_NORMAL,  // Gamepad using regular buttons
    SWITCH_MODE_IMU,     // Gamepad using gyro+accel
};

// Taken from Linux kernel: hid-nintendo.c
enum switch_proto_reqs {
    /* Input Reports */
    SWITCH_INPUT_SUBCMD_REPLY = 0x21,
    SWITCH_INPUT_IMU_DATA = 0x30,
    SWITCH_INPUT_MCU_DATA = 0x31,
    SWITCH_INPUT_BUTTON_EVENT = 0x3F,
};

// Received in SUBCMD_REQ_DEV_INFO
enum switch_controller_types {
    SWITCH_CONTROLLER_TYPE_JCL = 0x01,   // Joy-con left
    SWITCH_CONTROLLER_TYPE_JCR = 0x02,   // Joy-con right
    SWITCH_CONTROLLER_TYPE_PRO = 0x03,   // Pro Controller
    SWITCH_CONTROLLER_TYPE_SNES = 0x0b,  // SNES Controller
};

enum {
    OUTPUT_RUMBLE_AND_SUBCMD = 0x01,
    OUTPUT_RUMBLE_ONLY = 0x10,
};

enum switch_subcmd {
    SUBCMD_REQ_DEV_INFO = 0x02,
    SUBCMD_SET_REPORT_MODE = 0x03,
    SUBCMD_SPI_FLASH_READ = 0x10,
    SUBCMD_SET_PLAYER_LEDS = 0x30,
    SUBCMD_ENABLE_IMU = 0x40,
};

typedef enum {
    SWITCH_STATE_RUMBLE_DISABLED,
    SWITCH_STATE_RUMBLE_DELAYED,
    SWITCH_STATE_RUMBLE_IN_PROGRESS,
} switch_state_rumble_t;

// Calibration values for a stick.
typedef struct switch_cal_stick_s {
    int32_t min;
    int32_t center;
    int32_t max;
} switch_cal_stick_t;

// Calibration values for a IMU.
typedef struct switch_cal_imu_s {
    int16_t offset[3];
    int16_t scale[3];
} switch_cal_imu_t;

// switch_instance_t represents data used by the Switch driver instance.
typedef struct switch_instance_s {
    // Although technically, we can use one timer for delay and duration, easier to debug/maintain if we have two.
    btstack_timer_source_t rumble_timer_duration;
    btstack_timer_source_t rumble_timer_delayed_start;
    switch_state_rumble_t rumble_state;

    btstack_timer_source_t setup_timer;

    // Used by delayed start
    uint16_t rumble_weak_magnitude;
    uint16_t rumble_strong_magnitude;
    uint16_t rumble_duration_ms;

    enum switch_state state;
    enum switch_flags mode;
    uint8_t firmware_version_hi;
    uint8_t firmware_version_lo;
    enum switch_controller_types controller_type;
    uni_gamepad_seat_t gamepad_seat;

    // Calibration info
    switch_cal_stick_t cal_x;
    switch_cal_stick_t cal_y;
    switch_cal_stick_t cal_rx;
    switch_cal_stick_t cal_ry;

    switch_cal_imu_t cal_accel;
    switch_cal_imu_t cal_gyro;

    int32_t imu_cal_accel_divisor[3];
    int32_t imu_cal_gyro_divisor[3];

    // Debug only
    int debug_fd;         // File descriptor where dump is saved
    uint32_t debug_addr;  // Current dump address
} switch_instance_t;
_Static_assert(sizeof(switch_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "Switch instance too big");

struct switch_subcmd_request {
    // Report related
    uint8_t transaction_type;  // type of transaction
    uint8_t report_id;         // must be 0x01 for subcommand, 0x10 for rumble only

    // Data related
    uint8_t packet_num;  // increment by 1 for each packet sent. It loops in 0x0 -
                         // 0xF range.
    uint8_t rumble_left[4];
    uint8_t rumble_right[4];
    uint8_t subcmd_id;  // Not used by rumble, request
    uint8_t data[0];    // length depends on the subcommand
} __attribute__((packed));

struct switch_report_3f_s {
    uint8_t buttons_main;
    uint8_t buttons_aux;
    uint8_t hat;
    uint8_t x_lsb;
    uint8_t x_msb;
    uint8_t y_lsb;
    uint8_t y_msb;
    uint8_t rx_lsb;
    uint8_t rx_msb;
    uint8_t ry_lsb;
    uint8_t ry_msb;
} __attribute__((packed));

struct switch_imu_data_s {
    int16_t accel[3];  // x, y, z
    int16_t gyro[3];   // x, y, z
} __attribute__((packed));

struct switch_buttons_s {
    uint8_t buttons_right;
    uint8_t buttons_misc;
    uint8_t buttons_left;
    uint8_t stick_left[3];
    uint8_t stick_right[3];
    uint8_t vibrator_report;
} __attribute__((packed));

struct switch_report_30_s {
    struct switch_buttons_s buttons;
    struct switch_imu_data_s imu[3];  // contains 3 samples differentiated by 5ms (?) each
} __attribute__((packed));

struct switch_report_21_s {
    uint8_t report_id;
    uint8_t timer;
    uint8_t bat_con;
    struct switch_buttons_s status;
    uint8_t ack;
    uint8_t subcmd_id;
    uint8_t data[0];
} __attribute__((packed));

/* frequency/amplitude tables for rumble */
struct switch_rumble_freq_data {
    uint16_t high;
    uint8_t low;
    uint16_t freq; /* Hz*/
};

struct switch_rumble_amp_data {
    uint8_t high;
    uint16_t low;
    uint16_t amp;
};

/*
 * These tables are from
 * https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/rumble_data_table.md
 */
static const struct switch_rumble_freq_data rumble_freqs[] = {
    /* high, low, freq */
    {0x0000, 0x01, 41},   {0x0000, 0x02, 42},   {0x0000, 0x03, 43},   {0x0000, 0x04, 44},   {0x0000, 0x05, 45},
    {0x0000, 0x06, 46},   {0x0000, 0x07, 47},   {0x0000, 0x08, 48},   {0x0000, 0x09, 49},   {0x0000, 0x0A, 50},
    {0x0000, 0x0B, 51},   {0x0000, 0x0C, 52},   {0x0000, 0x0D, 53},   {0x0000, 0x0E, 54},   {0x0000, 0x0F, 55},
    {0x0000, 0x10, 57},   {0x0000, 0x11, 58},   {0x0000, 0x12, 59},   {0x0000, 0x13, 60},   {0x0000, 0x14, 62},
    {0x0000, 0x15, 63},   {0x0000, 0x16, 64},   {0x0000, 0x17, 66},   {0x0000, 0x18, 67},   {0x0000, 0x19, 69},
    {0x0000, 0x1A, 70},   {0x0000, 0x1B, 72},   {0x0000, 0x1C, 73},   {0x0000, 0x1D, 75},   {0x0000, 0x1e, 77},
    {0x0000, 0x1f, 78},   {0x0000, 0x20, 80},   {0x0400, 0x21, 82},   {0x0800, 0x22, 84},   {0x0c00, 0x23, 85},
    {0x1000, 0x24, 87},   {0x1400, 0x25, 89},   {0x1800, 0x26, 91},   {0x1c00, 0x27, 93},   {0x2000, 0x28, 95},
    {0x2400, 0x29, 97},   {0x2800, 0x2a, 99},   {0x2c00, 0x2b, 102},  {0x3000, 0x2c, 104},  {0x3400, 0x2d, 106},
    {0x3800, 0x2e, 108},  {0x3c00, 0x2f, 111},  {0x4000, 0x30, 113},  {0x4400, 0x31, 116},  {0x4800, 0x32, 118},
    {0x4c00, 0x33, 121},  {0x5000, 0x34, 123},  {0x5400, 0x35, 126},  {0x5800, 0x36, 129},  {0x5c00, 0x37, 132},
    {0x6000, 0x38, 135},  {0x6400, 0x39, 137},  {0x6800, 0x3a, 141},  {0x6c00, 0x3b, 144},  {0x7000, 0x3c, 147},
    {0x7400, 0x3d, 150},  {0x7800, 0x3e, 153},  {0x7c00, 0x3f, 157},  {0x8000, 0x40, 160},  {0x8400, 0x41, 164},
    {0x8800, 0x42, 167},  {0x8c00, 0x43, 171},  {0x9000, 0x44, 174},  {0x9400, 0x45, 178},  {0x9800, 0x46, 182},
    {0x9c00, 0x47, 186},  {0xa000, 0x48, 190},  {0xa400, 0x49, 194},  {0xa800, 0x4a, 199},  {0xac00, 0x4b, 203},
    {0xb000, 0x4c, 207},  {0xb400, 0x4d, 212},  {0xb800, 0x4e, 217},  {0xbc00, 0x4f, 221},  {0xc000, 0x50, 226},
    {0xc400, 0x51, 231},  {0xc800, 0x52, 236},  {0xcc00, 0x53, 241},  {0xd000, 0x54, 247},  {0xd400, 0x55, 252},
    {0xd800, 0x56, 258},  {0xdc00, 0x57, 263},  {0xe000, 0x58, 269},  {0xe400, 0x59, 275},  {0xe800, 0x5a, 281},
    {0xec00, 0x5b, 287},  {0xf000, 0x5c, 293},  {0xf400, 0x5d, 300},  {0xf800, 0x5e, 306},  {0xfc00, 0x5f, 313},
    {0x0001, 0x60, 320},  {0x0401, 0x61, 327},  {0x0801, 0x62, 334},  {0x0c01, 0x63, 341},  {0x1001, 0x64, 349},
    {0x1401, 0x65, 357},  {0x1801, 0x66, 364},  {0x1c01, 0x67, 372},  {0x2001, 0x68, 381},  {0x2401, 0x69, 389},
    {0x2801, 0x6a, 397},  {0x2c01, 0x6b, 406},  {0x3001, 0x6c, 415},  {0x3401, 0x6d, 424},  {0x3801, 0x6e, 433},
    {0x3c01, 0x6f, 443},  {0x4001, 0x70, 453},  {0x4401, 0x71, 462},  {0x4801, 0x72, 473},  {0x4c01, 0x73, 483},
    {0x5001, 0x74, 494},  {0x5401, 0x75, 504},  {0x5801, 0x76, 515},  {0x5c01, 0x77, 527},  {0x6001, 0x78, 538},
    {0x6401, 0x79, 550},  {0x6801, 0x7a, 562},  {0x6c01, 0x7b, 574},  {0x7001, 0x7c, 587},  {0x7401, 0x7d, 600},
    {0x7801, 0x7e, 613},  {0x7c01, 0x7f, 626},  {0x8001, 0x00, 640},  {0x8401, 0x00, 654},  {0x8801, 0x00, 668},
    {0x8c01, 0x00, 683},  {0x9001, 0x00, 698},  {0x9401, 0x00, 713},  {0x9801, 0x00, 729},  {0x9c01, 0x00, 745},
    {0xa001, 0x00, 761},  {0xa401, 0x00, 778},  {0xa801, 0x00, 795},  {0xac01, 0x00, 812},  {0xb001, 0x00, 830},
    {0xb401, 0x00, 848},  {0xb801, 0x00, 867},  {0xbc01, 0x00, 886},  {0xc001, 0x00, 905},  {0xc401, 0x00, 925},
    {0xc801, 0x00, 945},  {0xcc01, 0x00, 966},  {0xd001, 0x00, 987},  {0xd401, 0x00, 1009}, {0xd801, 0x00, 1031},
    {0xdc01, 0x00, 1053}, {0xe001, 0x00, 1076}, {0xe401, 0x00, 1100}, {0xe801, 0x00, 1124}, {0xec01, 0x00, 1149},
    {0xf001, 0x00, 1174}, {0xf401, 0x00, 1199}, {0xf801, 0x00, 1226}, {0xfc01, 0x00, 1253}};
#define TOTAL_RUMBLE_FREQS (sizeof(rumble_freqs) / sizeof(rumble_freqs[0]))

static const struct switch_rumble_amp_data rumble_amps[] = {
    /* high, low, amp */
    {0x00, 0x0040, 0},   {0x02, 0x8040, 10},  {0x04, 0x0041, 12},  {0x06, 0x8041, 14},  {0x08, 0x0042, 17},
    {0x0a, 0x8042, 20},  {0x0c, 0x0043, 24},  {0x0e, 0x8043, 28},  {0x10, 0x0044, 33},  {0x12, 0x8044, 40},
    {0x14, 0x0045, 47},  {0x16, 0x8045, 56},  {0x18, 0x0046, 67},  {0x1a, 0x8046, 80},  {0x1c, 0x0047, 95},
    {0x1e, 0x8047, 112}, {0x20, 0x0048, 117}, {0x22, 0x8048, 123}, {0x24, 0x0049, 128}, {0x26, 0x8049, 134},
    {0x28, 0x004a, 140}, {0x2a, 0x804a, 146}, {0x2c, 0x004b, 152}, {0x2e, 0x804b, 159}, {0x30, 0x004c, 166},
    {0x32, 0x804c, 173}, {0x34, 0x004d, 181}, {0x36, 0x804d, 189}, {0x38, 0x004e, 198}, {0x3a, 0x804e, 206},
    {0x3c, 0x004f, 215}, {0x3e, 0x804f, 225}, {0x40, 0x0050, 230}, {0x42, 0x8050, 235}, {0x44, 0x0051, 240},
    {0x46, 0x8051, 245}, {0x48, 0x0052, 251}, {0x4a, 0x8052, 256}, {0x4c, 0x0053, 262}, {0x4e, 0x8053, 268},
    {0x50, 0x0054, 273}, {0x52, 0x8054, 279}, {0x54, 0x0055, 286}, {0x56, 0x8055, 292}, {0x58, 0x0056, 298},
    {0x5a, 0x8056, 305}, {0x5c, 0x0057, 311}, {0x5e, 0x8057, 318}, {0x60, 0x0058, 325}, {0x62, 0x8058, 332},
    {0x64, 0x0059, 340}, {0x66, 0x8059, 347}, {0x68, 0x005a, 355}, {0x6a, 0x805a, 362}, {0x6c, 0x005b, 370},
    {0x6e, 0x805b, 378}, {0x70, 0x005c, 387}, {0x72, 0x805c, 395}, {0x74, 0x005d, 404}, {0x76, 0x805d, 413},
    {0x78, 0x005e, 422}, {0x7a, 0x805e, 431}, {0x7c, 0x005f, 440}, {0x7e, 0x805f, 450}, {0x80, 0x0060, 460},
    {0x82, 0x8060, 470}, {0x84, 0x0061, 480}, {0x86, 0x8061, 491}, {0x88, 0x0062, 501}, {0x8a, 0x8062, 512},
    {0x8c, 0x0063, 524}, {0x8e, 0x8063, 535}, {0x90, 0x0064, 547}, {0x92, 0x8064, 559}, {0x94, 0x0065, 571},
    {0x96, 0x8065, 584}, {0x98, 0x0066, 596}, {0x9a, 0x8066, 609}, {0x9c, 0x0067, 623}, {0x9e, 0x8067, 636},
    {0xa0, 0x0068, 650}, {0xa2, 0x8068, 665}, {0xa4, 0x0069, 679}, {0xa6, 0x8069, 694}, {0xa8, 0x006a, 709},
    {0xaa, 0x806a, 725}, {0xac, 0x006b, 741}, {0xae, 0x806b, 757}, {0xb0, 0x006c, 773}, {0xb2, 0x806c, 790},
    {0xb4, 0x006d, 808}, {0xb6, 0x806d, 825}, {0xb8, 0x006e, 843}, {0xba, 0x806e, 862}, {0xbc, 0x006f, 881},
    {0xbe, 0x806f, 900}, {0xc0, 0x0070, 920}, {0xc2, 0x8070, 940}, {0xc4, 0x0071, 960}, {0xc6, 0x8071, 981},
    {0xc8, 0x0072, 1003}};
#define TOTAL_RUMBLE_AMPS (sizeof(rumble_amps) / sizeof(rumble_amps[0]))

static void parse_report_30(struct uni_hid_device_s* d, const uint8_t* report, int len);
static void parse_report_30_joycon_left(uni_hid_device_t* d, const struct switch_report_30_s* r);
static void parse_report_30_joycon_right(uni_hid_device_t* d, const struct switch_report_30_s* r);
static void parse_report_30_pro_controller(uni_hid_device_t* d, const struct switch_report_30_s* r);
static void parse_report_3f(struct uni_hid_device_s* d, const uint8_t* report, int len);
static void process_input_subcmd_reply(struct uni_hid_device_s* d, const uint8_t* report, int len);
static switch_instance_t* get_switch_instance(uni_hid_device_t* d);
static void send_subcmd(uni_hid_device_t* d, struct switch_subcmd_request* r, int len);
static void process_fsm(struct uni_hid_device_s* d);
static void fsm_dump_rom(struct uni_hid_device_s* d);
static void fsm_request_device_info(struct uni_hid_device_s* d);
static void fsm_read_factory_stick_calibration(struct uni_hid_device_s* d);
static void fsm_read_user_stick_calibration(struct uni_hid_device_s* d);
static void fsm_read_factory_imu_calibration(struct uni_hid_device_s* d);
static void fsm_set_full_report(struct uni_hid_device_s* d);
static void fsm_enable_imu(struct uni_hid_device_s* d);
static void fsm_update_led(struct uni_hid_device_s* d);
static void fsm_ready(struct uni_hid_device_s* d);
static void process_reply_read_spi_dump(struct uni_hid_device_s* d, const uint8_t* data, int len);
static void process_reply_read_spi_factory_stick_calibration(struct uni_hid_device_s* d, const uint8_t* data, int len);
static void process_reply_read_spi_user_stick_calibration(struct uni_hid_device_s* d, const uint8_t* data, int len);
static void process_reply_read_spi_factory_imu_calibration(struct uni_hid_device_s* d, const uint8_t* data, int len);
static void process_reply_req_dev_info(struct uni_hid_device_s* d, const struct switch_report_21_s* r, int len);
static void process_reply_set_report_mode(struct uni_hid_device_s* d, const struct switch_report_21_s* r, int len);
static void process_reply_spi_flash_read(struct uni_hid_device_s* d, const struct switch_report_21_s* r, int len);
static void process_reply_set_player_leds(struct uni_hid_device_s* d, const struct switch_report_21_s* r, int len);
static void process_reply_enable_imu(struct uni_hid_device_s* d, const struct switch_report_21_s* r, int len);
static int32_t calibrate_axis(int32_t v, switch_cal_stick_t cal);
static void set_led(uni_hid_device_t* d, uint8_t leds);
static void on_switch_set_rumble_on(btstack_timer_source_t* ts);
static void on_switch_set_rumble_off(btstack_timer_source_t* ts);
static void switch_stop_rumble_now(uni_hid_device_t* d);
static void switch_play_dual_rumble_now(uni_hid_device_t* d,
                                        uint16_t duration_ms,
                                        uint8_t weak_magnitude,
                                        uint8_t strong_magnitude);
static void switch_setup_timeout_callback(btstack_timer_source_t* ts);
static void parse_stick_calibration(switch_cal_stick_t* x, switch_cal_stick_t* y, const uint8_t* data, bool is_left);

void uni_hid_parser_switch_setup(struct uni_hid_device_s* d) {
    switch_instance_t* ins = get_switch_instance(d);

    memset(ins, 0, sizeof(*ins));

    ins->state = STATE_SETUP;
    ins->mode = SWITCH_MODE_NONE;
    // In case the controller doesn't answer to SUBCMD_REQ_DEV_INFO command, set a default one.
    ins->controller_type = SWITCH_CONTROLLER_TYPE_PRO;

    // Setup default min,center,max calibration values for sticks.
    ins->cal_x.min = ins->cal_y.min = ins->cal_rx.min = ins->cal_ry.min = 512;
    ins->cal_x.center = ins->cal_y.center = ins->cal_rx.center = ins->cal_ry.center = 2048;
    ins->cal_x.max = ins->cal_y.max = ins->cal_rx.max = ins->cal_ry.max = 3583;

    // Default values for IMU in case factory values are not present
    for (int i = 0; i < 3; i++) {
        ins->cal_accel.offset[i] = DEFAULT_ACCEL_OFFSET;
        ins->cal_accel.scale[i] = DEFAULT_ACCEL_SCALE;
        ins->cal_gyro.offset[i] = DEFAULT_GYRO_OFFSET;
        ins->cal_gyro.scale[i] = DEFAULT_GYRO_SCALE;

        // Divisors that must be updated after calibration data is updated.
        ins->imu_cal_accel_divisor[i] = ins->cal_accel.scale[i] - ins->cal_accel.offset[i];
        ins->imu_cal_gyro_divisor[i] = ins->cal_gyro.scale[i] - ins->cal_gyro.offset[i];
    }

    // Dump SPI flash
#if ENABLE_SPI_FLASH_DUMP
    ins->debug_addr = SWITCH_DUMP_ROM_DATA_ADDR_START;
    ins->debug_fd = open("/tmp/spi_flash.bin", O_CREAT | O_RDWR);
    if (ins->debug_fd < 0) {
        loge("Switch: failed to create dump file");
    }
#endif  // ENABLE_SPI_FLASH_DUMP

    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));
    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;

    process_fsm(d);
}

void uni_hid_parser_switch_init_report(uni_hid_device_t* d) {
    ARG_UNUSED(d);
    // Nothing
}

void uni_hid_parser_switch_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    if (len < 12) {
        loge("Nintendo Switch: Invalid packet len; got %d, want >= 12\n", len);
        return;
    }
    switch (report[0]) {
        case SWITCH_INPUT_SUBCMD_REPLY:
            // Don't memset gamepad report
            process_input_subcmd_reply(d, report, len);
            break;
        case SWITCH_INPUT_IMU_DATA:
            parse_report_30(d, report, len);
            break;
        case SWITCH_INPUT_BUTTON_EVENT:
            parse_report_3f(d, report, len);
            break;
        default:
            loge("Nintendo Switch: unsupported report id: 0x%02x\n", report[0]);
            printf_hexdump(report, len);
    }
}

static void process_fsm(struct uni_hid_device_s* d) {
    switch_instance_t* ins = get_switch_instance(d);
    logd("Switch: fsm next state = %d\n", ins->state + 1);

    switch (ins->state) {
        case STATE_SETUP:
            logd("STATE_SETUP\n");
            btstack_run_loop_set_timer_context(&ins->setup_timer, d);
            btstack_run_loop_set_timer_handler(&ins->setup_timer, &switch_setup_timeout_callback);
            btstack_run_loop_set_timer(&ins->setup_timer, SWITCH_SETUP_TIMEOUT_MS);
            btstack_run_loop_add_timer(&ins->setup_timer);

            fsm_request_device_info(d);
            break;
        case STATE_REQ_DEV_INFO:
            logd("STATE_REQ_DEV_INFO\n");
            fsm_read_factory_stick_calibration(d);
            break;
        case STATE_READ_FACTORY_STICK_CALIBRATION:
            logd("STATE_READ_FACTORY_STICK_CALIBRATION\n");
            fsm_read_user_stick_calibration(d);
            break;
        case STATE_READ_USER_STICK_CALIBRATION:
            logd("STATE_READ_USER_STICK_CALIBRATION\n");
            fsm_read_factory_imu_calibration(d);
            break;
        case STATE_READ_FACTORY_IMU_CALIBRATION:
            logd("STATE_READ_FACTORY_IMU_CALIBRATION\n");
            fsm_set_full_report(d);
            break;
        case STATE_SET_FULL_REPORT:
            logd("STATE_SET_FULL_REPORT\n");
            fsm_enable_imu(d);
            break;
        case STATE_ENABLE_IMU:
            logd("STATE_ENABLE_IMU\n");
            fsm_dump_rom(d);
            break;
        case STATE_DUMP_FLASH:
            logd("STATE_DUMP_FLASH\n");
            fsm_update_led(d);
            break;
        case STATE_UPDATE_LED:
            logd("STATE_UPDATE_LED\n");
            fsm_ready(d);
            break;
        case STATE_READY:
            logd("STATE_READY\n");
            btstack_run_loop_remove_timer(&ins->setup_timer);
            break;
        default:
            loge("Switch: unexpected state: 0x%02x\n", ins->mode);
    }
}

static void process_reply_read_spi_dump(struct uni_hid_device_s* d, const uint8_t* data, int len) {
#if ENABLE_SPI_FLASH_DUMP
    ARG_UNUSED(len);
    switch_instance_t* ins = get_switch_instance(d);
    uint32_t addr = data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24;
    int chunk_size = data[4];

    if (chunk_size != SWITCH_DUMP_ROM_DATA_SIZE) {
        loge(
            "Switch: could not dump chunk at 0x%04x. Invalid size, got %d, want "
            "%d\n",
            addr, chunk_size, SWITCH_DUMP_ROM_DATA_SIZE);
        return;
    }

    logi("Switch: dumping %d bytes at address: 0x%04x\n", chunk_size, addr);
    write(ins->debug_fd, &data[5], chunk_size);
#else
    ARG_UNUSED(d);
    ARG_UNUSED(data);
    ARG_UNUSED(len);
#endif  // ENABLE_SPI_FLASH_DUMP
}

static void process_reply_read_spi_factory_stick_calibration(struct uni_hid_device_s* d, const uint8_t* data, int len) {
    switch_instance_t* ins = get_switch_instance(d);
    bool is_left;

    if (ins->controller_type == SWITCH_CONTROLLER_TYPE_PRO) {
        // If data is longer than expected, we treat it as Ok.
        // Clones might report longer length.
        // See: https://github.com/ricardoquesada/bluepad32/issues/94
        if (len < SWITCH_FACTORY_STICK_CAL_DATA_SIZE * 2) {
            loge("Switch: invalid spi factory stick calibration len; got %d, wanted >= %d\n", len,
                 SWITCH_FACTORY_STICK_CAL_DATA_SIZE * 2);
            printf_hexdump(data, len);
            return;
        }

        parse_stick_calibration(&ins->cal_x, &ins->cal_y, data, true);
        parse_stick_calibration(&ins->cal_rx, &ins->cal_y, &data[9], false);
    } else {
        if (len < SWITCH_FACTORY_STICK_CAL_DATA_SIZE) {
            // If data is longer than expected, we treat it as Ok.
            // Clones might report longer length.
            // See: https://github.com/ricardoquesada/bluepad32/issues/94
            loge("Switch: invalid spi factory stick calibration len; got %d, wanted >= %d\n", len,
                 SWITCH_FACTORY_STICK_CAL_DATA_SIZE);
            printf_hexdump(data, len);
            return;
        }
        is_left = ins->controller_type == SWITCH_CONTROLLER_TYPE_JCL;
        parse_stick_calibration(&ins->cal_x, &ins->cal_y, data, is_left);
    }

    if (ins->controller_type == SWITCH_CONTROLLER_TYPE_PRO || ins->controller_type == SWITCH_CONTROLLER_TYPE_JCL)
        logi("Switch: Left stick calibration: x=%d,%d,%d, y=%d,%d,%d\n",  //
             ins->cal_x.min, ins->cal_x.center,
             ins->cal_x.max,  // x
             ins->cal_y.min, ins->cal_y.center,
             ins->cal_y.max  // y
        );
    if (ins->controller_type == SWITCH_CONTROLLER_TYPE_PRO || ins->controller_type == SWITCH_CONTROLLER_TYPE_JCR)
        logi("Switch: Right stick calibration: x=%d,%d,%d, y=%d,%d,%d\n",  //
             ins->cal_rx.min, ins->cal_rx.center,
             ins->cal_rx.max,  // rx
             ins->cal_ry.min, ins->cal_ry.center,
             ins->cal_ry.max  // ry
        );
}

static void process_reply_read_spi_user_stick_calibration(struct uni_hid_device_s* d, const uint8_t* data, int len) {
    ARG_UNUSED(d);
    ARG_UNUSED(data);
    ARG_UNUSED(len);

    // FIXME: Implement me

    logd("process_reply_read_spi_user_stick_calibration\n");
    // printf_hexdump(data, len);
}

static void process_reply_read_spi_factory_imu_calibration(struct uni_hid_device_s* d, const uint8_t* data, int len) {
    switch_instance_t* ins = get_switch_instance(d);

    if (len != SWITCH_FACTORY_IMU_CAL_DATA_SIZE) {
        loge("Switch: invalid spi factory imu calibration len; got %d, wanted %d\n", len,
             SWITCH_FACTORY_IMU_CAL_DATA_SIZE);
        return;
    }

    for (int i = 0; i < 3; i++) {
        int j = i * 2;
        // Treat them as non-aligned, might crash on RP2040.
        // See: https://github.com/ricardoquesada/bluepad32/issues/86
        ins->cal_accel.offset[i] = data[j + 0] | (data[j + 1] << 8);
        ins->cal_accel.scale[i] = data[j + 6] | data[j + 7] << 8;
        ins->cal_gyro.offset[i] = data[j + 12] | data[j + 13] << 8;
        ins->cal_gyro.scale[i] = data[j + 18] | data[j + 19] << 8;
    }

    for (int i = 0; i < 3; i++) {
        // Divisors that must be updated after calibration data is updated.
        // FIXME: move to its own function
        ins->imu_cal_accel_divisor[i] = ins->cal_accel.scale[i] - ins->cal_accel.offset[i];
        ins->imu_cal_gyro_divisor[i] = ins->cal_gyro.scale[i] - ins->cal_gyro.offset[i];
    }

    logi(
        "Switch: IMU calibration info: accel.offset=%d,%d,%d, accel.scale=%d,%d,%d, gyro.offset=%d,%d,%d, gyro."
        "scale=%d,%d,%d\n",
        ins->cal_accel.offset[0], ins->cal_accel.offset[1], ins->cal_accel.offset[2],  // accel offset
        ins->cal_accel.scale[0], ins->cal_accel.scale[1], ins->cal_accel.scale[2],     // accel scale
        ins->cal_gyro.offset[0], ins->cal_gyro.offset[1], ins->cal_gyro.offset[2],     // gyro offset
        ins->cal_gyro.scale[0], ins->cal_gyro.scale[1], ins->cal_gyro.scale[2]);       // gyro scale
}

// Reply to SUBCMD_REQ_DEV_INFO
static void process_reply_req_dev_info(struct uni_hid_device_s* d, const struct switch_report_21_s* r, int len) {
    ARG_UNUSED(len);
    switch_instance_t* ins = get_switch_instance(d);
    if (ins->state > STATE_SETUP && ins->mode == SWITCH_MODE_NONE) {
        bool enable_imu;
#if ENABLE_IMU_REPORT
        enable_imu = true;
#else
        // Button "A" must be pressed in orther to enable IMU.
        enable_imu = (r->status.buttons_right & 0x08);
#endif
        if (enable_imu) {
            logi("Switch: IMU report enabled\n");
            ins->mode = SWITCH_MODE_IMU;
        } else {
            logi("Switch: IMU report disabled\n");
            ins->mode = SWITCH_MODE_NORMAL;
        }
    }
    ins->firmware_version_hi = r->data[0];
    ins->firmware_version_lo = r->data[1];
    ins->controller_type = r->data[2];
    logi("Switch: Firmware version: %d.%d. Controller type=%d\n", r->data[0], r->data[1], r->data[2]);
}

// Reply to SUBCMD_SET_REPORT_MODE
static void process_reply_set_report_mode(struct uni_hid_device_s* d, const struct switch_report_21_s* r, int len) {
    ARG_UNUSED(d);
    ARG_UNUSED(r);
    ARG_UNUSED(len);
}

// Reply to SUBCMD_SPI_FLASH_READ
static void process_reply_spi_flash_read(struct uni_hid_device_s* d, const struct switch_report_21_s* r, int len) {
    // +5 because it includes the address and size of the payload
    if (len < sizeof(*r) + 5) {
        loge("Switch: Invalid SPI flash read length, expected >= %d, got: %d\n", sizeof(*r) + 5, len);
        return;
    }
    int mem_len = r->data[4];
    uint32_t addr = r->data[0] | r->data[1] << 8 | r->data[2] << 16 | r->data[3] << 24;

    logd("Switch: Reading from %#x, mem len=%d, struct size=%d, report size=%d\n", addr, mem_len, sizeof(*r), len);

    switch_instance_t* ins = get_switch_instance(d);

    switch (ins->state) {
        case STATE_READ_FACTORY_STICK_CALIBRATION:
            process_reply_read_spi_factory_stick_calibration(d, &r->data[5], mem_len);
            break;
        case STATE_READ_USER_STICK_CALIBRATION:
            process_reply_read_spi_user_stick_calibration(d, &r->data[5], mem_len);
            break;
        case STATE_READ_FACTORY_IMU_CALIBRATION:
            process_reply_read_spi_factory_imu_calibration(d, &r->data[5], mem_len);
            break;
        case STATE_DUMP_FLASH:
            process_reply_read_spi_dump(d, r->data, mem_len);
            break;
        default:
            loge("Switch: unexpected state: %d, spi_read size reply %d at 0x%04x\n", ins->state, mem_len, addr);
            printf_hexdump((const uint8_t*)r, len);
    }
}

// Reply to SUBCMD_SET_PLAYER_LEDS
static void process_reply_set_player_leds(struct uni_hid_device_s* d, const struct switch_report_21_s* r, int len) {
    ARG_UNUSED(d);
    ARG_UNUSED(r);
    ARG_UNUSED(len);
}

// Reply SUBCMD_ENABLE_IMU
static void process_reply_enable_imu(struct uni_hid_device_s* d, const struct switch_report_21_s* r, int len) {
    ARG_UNUSED(d);
    ARG_UNUSED(r);
    ARG_UNUSED(len);
}

// Process 0x21 input report: SWITCH_INPUT_SUBCMD_REPLY
static void process_input_subcmd_reply(struct uni_hid_device_s* d, const uint8_t* report, int len) {
    // Report has this format:
    // 21 D9 80 08 10 00 18 A8 78 F2 C7 70 0C 80 30 00 00 00 00 00 00 00 00 00
    // 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    // 00
    const struct switch_report_21_s* r = (const struct switch_report_21_s*)report;
    if ((r->ack & 0b10000000) == 0) {
        loge("Switch: Error, subcommand id=0x%02x was not successful.\n", r->subcmd_id);
    }
    switch (r->subcmd_id) {
        case SUBCMD_REQ_DEV_INFO:
            process_reply_req_dev_info(d, r, len);
            break;
        case SUBCMD_SET_REPORT_MODE:
            process_reply_set_report_mode(d, r, len);
            break;
        case SUBCMD_SPI_FLASH_READ:
            process_reply_spi_flash_read(d, r, len);
            break;
        case SUBCMD_SET_PLAYER_LEDS:
            process_reply_set_player_leds(d, r, len);
            break;
        case SUBCMD_ENABLE_IMU:
            process_reply_enable_imu(d, r, len);
            break;
        default:
            loge("Switch: Error, unexpected subcmd_id=0x%02x in report 0x21\n", r->subcmd_id);
            break;
    }

    // Update battery
    int battery = r->bat_con >> 5;
    switch (battery) {
        case 0:
            d->controller.battery = UNI_CONTROLLER_BATTERY_EMPTY;
            break;
        case 1:
            d->controller.battery = 64;
            break;
        case 2:
            d->controller.battery = 128;
            break;
        case 3:
            d->controller.battery = 192;
            break;
        case 4:
            d->controller.battery = UNI_CONTROLLER_BATTERY_FULL;
            break;
        default:
            loge("Switch: invalid battery value: %d\n", battery);
    }
    process_fsm(d);
}

static void parse_stick_calibration(switch_cal_stick_t* x, switch_cal_stick_t* y, const uint8_t* data, bool is_left) {
    int32_t cal_x_max;
    int32_t cal_y_max;
    int32_t cal_x_min;
    int32_t cal_y_min;

    // Guaranteed that data has at lest 9 elements
    // left and right sticks have different parsing order

    if (is_left) {
        // max
        cal_x_max = data[0] | ((data[1] & 0x0f) << 8);  // bits: 0-11
        cal_y_max = (data[1] >> 4) | (data[2] << 4);    // bits: 12-23
        // center
        x->center = data[3] | ((data[4] & 0x0f) << 8);  // bits: 24-35
        y->center = (data[4] >> 4) | (data[5] << 4);    // bits: 36-47
        // min
        cal_x_min = data[6] | ((data[7] & 0x0f) << 8);  // bits: 48-59
        cal_y_min = (data[7] >> 4) | (data[8] << 4);    // bits: 60-71
    } else {
        // Right stick
        // center
        x->center = data[0] | ((data[1] & 0x0f) << 8);  // bits: 0-11
        y->center = (data[1] >> 4) | (data[2] << 4);    // bits: 12-23
        // min
        cal_x_min = data[3] | ((data[4] & 0x0f) << 8);  // bits: 24-35
        cal_y_min = (data[4] >> 4) | (data[5] << 4);    // bits: 36-47
        // max
        cal_x_max = data[6] | ((data[7] & 0x0f) << 8);  // bits: 48-59
        cal_y_max = (data[7] >> 4) | (data[8] << 4);    // bits: 60-71
    }

    x->min = x->center - cal_x_min;
    x->max = x->center + cal_x_max;
    y->min = y->center - cal_y_min;
    y->max = y->center + cal_y_max;
}

static void parse_imu(uni_hid_device_t* d, const struct switch_imu_data_s* r) {
    switch_instance_t* ins = get_switch_instance(d);
    uni_controller_t* ctl = &d->controller;

    int accel[3];
    int gyro[3];

    for (int i = 0; i < 3; i++) {
        if (ins->imu_cal_accel_divisor[i] == 0)
            accel[i] = r->accel[i];
        else
            accel[i] = (r->accel[i] * ins->cal_accel.scale[i]) / ins->imu_cal_accel_divisor[i];
        gyro[i] = mult_frac((SWITCH_IMU_PREC_RANGE_SCALE * (r->gyro[i] - ins->cal_gyro.offset[i])),
                            ins->cal_gyro.scale[i], ins->imu_cal_gyro_divisor[i]);
    }

    // Right joycon has Y and Z axes negated.
    if (ins->controller_type == SWITCH_CONTROLLER_TYPE_JCR) {
        accel[1] = -accel[1];
        accel[2] = -accel[2];
        gyro[1] = -gyro[1];
        gyro[2] = -gyro[2];
    }

    for (int i = 0; i < 3; i++) {
        ctl->gamepad.accel[i] = accel[i];
        ctl->gamepad.gyro[i] = gyro[i];
    }
}

// Process 0x30 input report: SWITCH_INPUT_IMU_DATA
static void parse_report_30(struct uni_hid_device_s* d, const uint8_t* report, int len) {
    // Expecting something like:
    // (a1) 30 44 60 00 00 00 FD 87 7B 0E B8 70 00 6C FD FC FF 78 10 35 00 C1 FF
    // 9D FF 72 FD 01 00 72 10 35 00 C1 FF 9B FF 75 FD FF FF 6C 10 34 00 C2 FF
    // 9A FF

    ARG_UNUSED(len);

    switch_instance_t* ins = get_switch_instance(d);
    uni_controller_t* ctl = &d->controller;
    memset(&ctl->gamepad, 0, sizeof(ctl->gamepad));

    const struct switch_report_30_s* r = (const struct switch_report_30_s*)&report[3];

    switch (ins->controller_type) {
        case SWITCH_CONTROLLER_TYPE_JCL:
            parse_report_30_joycon_left(d, r);
            break;
        case SWITCH_CONTROLLER_TYPE_JCR:
            parse_report_30_joycon_right(d, r);
            break;
        case SWITCH_CONTROLLER_TYPE_PRO:
        case SWITCH_CONTROLLER_TYPE_SNES:
            parse_report_30_pro_controller(d, r);
            break;
        default:
            loge("Switch: Invalid controller_type: 0x%04x\n", ins->controller_type);
            break;
    }

    // IMU is valid for all 3 types of controllers.

    // 3 gyro/accel frames are reported.
    // Different approaches: take the latest one, or average them.
    // We just take the latest one. If it is not accurate enough, we can average them.
    if (ins->mode == SWITCH_MODE_IMU)
        parse_imu(d, &r->imu[2]);
}

// Shared both by Switch Pro Controller and Switch SNES.
static void parse_report_30_pro_controller(uni_hid_device_t* d, const struct switch_report_30_s* r) {
    switch_instance_t* ins = get_switch_instance(d);
    uni_controller_t* ctl = &d->controller;
    // Buttons "right"
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b00000001) ? BUTTON_X : 0;           // Y
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b00000010) ? BUTTON_Y : 0;           // X
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b00000100) ? BUTTON_A : 0;           // B
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b00001000) ? BUTTON_B : 0;           // A
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b01000000) ? BUTTON_SHOULDER_R : 0;  // R
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b10000000) ? BUTTON_TRIGGER_R : 0;   // ZR

    // Buttons "left"
    ctl->gamepad.dpad |= (r->buttons.buttons_left & 0b00000001) ? DPAD_DOWN : 0;
    ctl->gamepad.dpad |= (r->buttons.buttons_left & 0b00000010) ? DPAD_UP : 0;
    ctl->gamepad.dpad |= (r->buttons.buttons_left & 0b00000100) ? DPAD_RIGHT : 0;
    ctl->gamepad.dpad |= (r->buttons.buttons_left & 0b00001000) ? DPAD_LEFT : 0;
    ctl->gamepad.buttons |= (r->buttons.buttons_left & 0b01000000) ? BUTTON_SHOULDER_L : 0;  // L
    ctl->gamepad.buttons |= (r->buttons.buttons_left & 0b10000000) ? BUTTON_TRIGGER_L : 0;   // ZL

    // Misc
    ctl->gamepad.misc_buttons |= (r->buttons.buttons_misc & 0b00000001) ? MISC_BUTTON_SELECT : 0;   // -
    ctl->gamepad.misc_buttons |= (r->buttons.buttons_misc & 0b00000010) ? MISC_BUTTON_START : 0;    // +
    ctl->gamepad.misc_buttons |= (r->buttons.buttons_misc & 0b00010000) ? MISC_BUTTON_SYSTEM : 0;   // Home
    ctl->gamepad.misc_buttons |= (r->buttons.buttons_misc & 0b00100000) ? MISC_BUTTON_CAPTURE : 0;  // Capture

    // Sticks, not present on SNES model.
    if (ins->controller_type == SWITCH_CONTROLLER_TYPE_PRO) {
        // Thumbs
        ctl->gamepad.buttons |= (r->buttons.buttons_misc & 0b00000100) ? BUTTON_THUMB_R : 0;  // Thumb R
        ctl->gamepad.buttons |= (r->buttons.buttons_misc & 0b00001000) ? BUTTON_THUMB_L : 0;  // Thumb L

        // Stick left
        int32_t lx = r->buttons.stick_left[0] | ((r->buttons.stick_left[1] & 0x0f) << 8);
        ctl->gamepad.axis_x = calibrate_axis(lx, ins->cal_x);
        int32_t ly = (r->buttons.stick_left[1] >> 4) | (r->buttons.stick_left[2] << 4);
        ctl->gamepad.axis_y = -calibrate_axis(ly, ins->cal_y);

        // Stick right
        int32_t rx = r->buttons.stick_right[0] | ((r->buttons.stick_right[1] & 0x0f) << 8);
        ctl->gamepad.axis_rx = calibrate_axis(rx, ins->cal_rx);
        int32_t ry = (r->buttons.stick_right[1] >> 4) | (r->buttons.stick_right[2] << 4);
        ctl->gamepad.axis_ry = -calibrate_axis(ry, ins->cal_ry);
        logd("uncalibrated values: x=%d,y=%d,rx=%d,ry=%d\n", lx, ly, rx, ry);
    }
}

static void parse_report_30_joycon_left(uni_hid_device_t* d, const struct switch_report_30_s* r) {
    // JoyCons are treated as standalone controllers. So the buttons/axis are "rotated".
    uni_controller_t* ctl = &d->controller;
    switch_instance_t* ins = get_switch_instance(d);

    // Axis (left only stick)
    int32_t lx = r->buttons.stick_left[0] | ((r->buttons.stick_left[1] & 0x0f) << 8);
    ctl->gamepad.axis_y = -calibrate_axis(lx, ins->cal_x);
    int32_t ly = (r->buttons.stick_left[1] >> 4) | (r->buttons.stick_left[2] << 4);
    ctl->gamepad.axis_x = -calibrate_axis(ly, ins->cal_y);

    // Buttons
    ctl->gamepad.buttons |= (r->buttons.buttons_left & 0b00000001) ? BUTTON_B : 0;
    ctl->gamepad.buttons |= (r->buttons.buttons_left & 0b00000010) ? BUTTON_X : 0;
    ctl->gamepad.buttons |= (r->buttons.buttons_left & 0b00000100) ? BUTTON_Y : 0;
    ctl->gamepad.buttons |= (r->buttons.buttons_left & 0b00001000) ? BUTTON_A : 0;
    ctl->gamepad.buttons |= (r->buttons.buttons_left & 0b00010000) ? BUTTON_SHOULDER_R : 0;  // SR
    ctl->gamepad.buttons |= (r->buttons.buttons_left & 0b00100000) ? BUTTON_SHOULDER_L : 0;  // SL
    ctl->gamepad.buttons |= (r->buttons.buttons_left & 0b01000000) ? BUTTON_TRIGGER_L : 0;   // L
    ctl->gamepad.buttons |= (r->buttons.buttons_left & 0b10000000) ? BUTTON_TRIGGER_R : 0;   // ZL
    ctl->gamepad.buttons |= (r->buttons.buttons_misc & 0b00001000) ? BUTTON_THUMB_L : 0;

    // Misc buttons
    // Since the JoyCon is in horizontal mode, map "-" / "Capture" as if they where "-" and "+"
    ctl->gamepad.misc_buttons |= (r->buttons.buttons_misc & 0b00000001) ? MISC_BUTTON_SELECT : 0;  // -
    ctl->gamepad.misc_buttons |= (r->buttons.buttons_misc & 0b00100000) ? MISC_BUTTON_START : 0;   // Capture
}

static void parse_report_30_joycon_right(uni_hid_device_t* d, const struct switch_report_30_s* r) {
    // JoyCons are treated as standalone controllers. So the buttons/axis are "rotated".
    uni_controller_t* ctl = &d->controller;
    switch_instance_t* ins = get_switch_instance(d);

    // Axis (right only stick)
    int32_t rx = r->buttons.stick_right[0] | ((r->buttons.stick_right[1] & 0x0f) << 8);
    ctl->gamepad.axis_y = calibrate_axis(rx, ins->cal_rx);
    int32_t ry = (r->buttons.stick_right[1] >> 4) | (r->buttons.stick_right[2] << 4);
    ctl->gamepad.axis_x = calibrate_axis(ry, ins->cal_ry);

    // Buttons
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b00000001) ? BUTTON_Y : 0;
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b00000010) ? BUTTON_B : 0;
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b00000100) ? BUTTON_X : 0;
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b00001000) ? BUTTON_A : 0;
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b00010000) ? BUTTON_SHOULDER_R : 0;  // SR
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b00100000) ? BUTTON_SHOULDER_L : 0;  // SL
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b01000000) ? BUTTON_TRIGGER_L : 0;   // R
    ctl->gamepad.buttons |= (r->buttons.buttons_right & 0b10000000) ? BUTTON_TRIGGER_R : 0;   // ZR
    ctl->gamepad.buttons |= (r->buttons.buttons_misc & 0b00000100) ? BUTTON_THUMB_L : 0;

    // Misc buttons
    // Since the JoyCon is in horizontal mode, map "Home" / "+" as if they where "-" and "+"
    ctl->gamepad.misc_buttons |= (r->buttons.buttons_misc & 0b00010000) ? MISC_BUTTON_SELECT : 0;  // Home
    ctl->gamepad.misc_buttons |= (r->buttons.buttons_misc & 0b00000010) ? MISC_BUTTON_START : 0;   // +
}

// Process 0x3f input report: SWITCH_INPUT_BUTTON_EVENT
// Some clones report the buttons inverted. Always base the mappings on the original
// devices, not clones.
static void parse_report_3f(struct uni_hid_device_s* d, const uint8_t* report, int len) {
    // Expecting something like:
    // (a1) 3F 00 00 08 D0 81 0F 88 F0 81 6F 8E
    ARG_UNUSED(len);
    uni_controller_t* ctl = &d->controller;
    memset(&ctl->gamepad, 0, sizeof(ctl->gamepad));

    const struct switch_report_3f_s* r = (const struct switch_report_3f_s*)&report[1];

    // Button main
    ctl->gamepad.buttons |= (r->buttons_main & 0b00000001) ? BUTTON_A : 0;           // B
    ctl->gamepad.buttons |= (r->buttons_main & 0b00000010) ? BUTTON_B : 0;           // A
    ctl->gamepad.buttons |= (r->buttons_main & 0b00000100) ? BUTTON_X : 0;           // Y
    ctl->gamepad.buttons |= (r->buttons_main & 0b00001000) ? BUTTON_Y : 0;           // X
    ctl->gamepad.buttons |= (r->buttons_main & 0b00010000) ? BUTTON_SHOULDER_L : 0;  // L
    ctl->gamepad.buttons |= (r->buttons_main & 0b00100000) ? BUTTON_SHOULDER_R : 0;  // R
    ctl->gamepad.buttons |= (r->buttons_main & 0b01000000) ? BUTTON_TRIGGER_L : 0;   // ZL
    ctl->gamepad.buttons |= (r->buttons_main & 0b10000000) ? BUTTON_TRIGGER_R : 0;   // ZR

    // Button aux
    ctl->gamepad.misc_buttons |= (r->buttons_aux & 0b00000001) ? MISC_BUTTON_SELECT : 0;   // -
    ctl->gamepad.misc_buttons |= (r->buttons_aux & 0b00000010) ? MISC_BUTTON_START : 0;    // +
    ctl->gamepad.buttons |= (r->buttons_aux & 0b00000100) ? BUTTON_THUMB_L : 0;            // Thumb L
    ctl->gamepad.buttons |= (r->buttons_aux & 0b00001000) ? BUTTON_THUMB_R : 0;            // Thumb R
    ctl->gamepad.misc_buttons |= (r->buttons_aux & 0b00010000) ? MISC_BUTTON_SYSTEM : 0;   // Home
    ctl->gamepad.misc_buttons |= (r->buttons_aux & 0b00100000) ? MISC_BUTTON_CAPTURE : 0;  // Capture

    // Dpad
    ctl->gamepad.dpad = uni_hid_parser_hat_to_dpad(r->hat);

    // Axis
    ctl->gamepad.axis_x = ((r->x_msb << 8) | r->x_lsb) * AXIS_NORMALIZE_RANGE / 65536 - AXIS_NORMALIZE_RANGE / 2;
    ctl->gamepad.axis_y = ((r->y_msb << 8) | r->y_lsb) * AXIS_NORMALIZE_RANGE / 65536 - AXIS_NORMALIZE_RANGE / 2;
    ctl->gamepad.axis_rx = ((r->rx_msb << 8) | r->rx_lsb) * AXIS_NORMALIZE_RANGE / 65536 - AXIS_NORMALIZE_RANGE / 2;
    ctl->gamepad.axis_ry = ((r->ry_msb << 8) | r->ry_lsb) * AXIS_NORMALIZE_RANGE / 65536 - AXIS_NORMALIZE_RANGE / 2;
}

static void fsm_dump_rom(struct uni_hid_device_s* d) {
#if ENABLE_SPI_FLASH_DUMP
    switch_instance_t* ins = get_switch_instance(d);
    uint32_t addr = ins->debug_addr;

    if (addr >= SWITCH_DUMP_ROM_DATA_ADDR_END || ins->debug_fd < 0) {
        ins->state = STATE_DUMP_FLASH;
        close(ins->debug_fd);
        process_fsm(d);
        return;
    }

    uint8_t out[sizeof(struct switch_subcmd_request) + 5] = {0};
    struct switch_subcmd_request* req = (struct switch_subcmd_request*)&out[0];
    req->report_id = 0x01;  // 0x01 for sub commands
    req->subcmd_id = SUBCMD_SPI_FLASH_READ;
    // Address to read from: stick calibration
    req->data[0] = addr & 0xff;
    req->data[1] = (addr >> 8) & 0xff;
    req->data[2] = (addr >> 16) & 0xff;
    req->data[3] = (addr >> 24) & 0xff;
    req->data[4] = SWITCH_DUMP_ROM_DATA_SIZE;
    send_subcmd(d, req, sizeof(out));

    ins->debug_addr += SWITCH_DUMP_ROM_DATA_SIZE;
#else
    switch_instance_t* ins = get_switch_instance(d);
    ins->state = STATE_DUMP_FLASH;
    btstack_run_loop_remove_timer(&ins->setup_timer);
    process_fsm(d);
#endif  // ENABLE_SPI_FLASH_DUMP
}

static void fsm_request_device_info(struct uni_hid_device_s* d) {
    switch_instance_t* ins = get_switch_instance(d);
    ins->state = STATE_REQ_DEV_INFO;

    struct switch_subcmd_request req = {
        .report_id = 0x01,  // 0x01 for sub commands
        .subcmd_id = SUBCMD_REQ_DEV_INFO,
    };
    send_subcmd(d, &req, sizeof(req));
}

static void fsm_read_factory_stick_calibration(struct uni_hid_device_s* d) {
    switch_instance_t* ins = get_switch_instance(d);
    ins->state = STATE_READ_FACTORY_STICK_CALIBRATION;

    // Either my math was bad, or requesting more bytes for the left controller returns invalid calibration.
    // So for Pro we request both left and right cal data.
    // But for the JoyCons just the cal data that they need.
    uint32_t spi_addr = (ins->controller_type == SWITCH_CONTROLLER_TYPE_JCR) ? SWITCH_FACTORY_STICK_CAL_DATA_ADDR_RIGHT
                                                                             : SWITCH_FACTORY_STICK_CAL_DATA_ADDR_LEFT;
    uint8_t bytes_to_read = SWITCH_FACTORY_STICK_CAL_DATA_SIZE;
    // Double, since it requests both left and right
    if (ins->controller_type == SWITCH_CONTROLLER_TYPE_PRO)
        bytes_to_read *= 2;

    uint8_t out[sizeof(struct switch_subcmd_request) + 5] = {0};
    struct switch_subcmd_request* req = (struct switch_subcmd_request*)&out[0];
    req->report_id = 0x01;  // 0x01 for sub commands
    req->subcmd_id = SUBCMD_SPI_FLASH_READ;

    // Address to read from: stick calibration
    req->data[0] = spi_addr & 0xff;
    req->data[1] = (spi_addr >> 8) & 0xff;
    req->data[2] = (spi_addr >> 16) & 0xff;
    req->data[3] = (spi_addr >> 24) & 0xff;
    req->data[4] = bytes_to_read;
    send_subcmd(d, req, sizeof(out));
}

static void fsm_read_user_stick_calibration(struct uni_hid_device_s* d) {
    switch_instance_t* ins = get_switch_instance(d);
    ins->state = STATE_READ_USER_STICK_CALIBRATION;

    uint8_t out[sizeof(struct switch_subcmd_request) + 5] = {0};
    struct switch_subcmd_request* req = (struct switch_subcmd_request*)&out[0];
    req->report_id = 0x01;  // 0x01 for sub commands
    req->subcmd_id = SUBCMD_SPI_FLASH_READ;
    // Address to read from: stick calibration
    req->data[0] = SWITCH_USER_STICK_CAL_DATA_ADDR & 0xff;
    req->data[1] = (SWITCH_USER_STICK_CAL_DATA_ADDR >> 8) & 0xff;
    req->data[2] = (SWITCH_USER_STICK_CAL_DATA_ADDR >> 16) & 0xff;
    req->data[3] = (SWITCH_USER_STICK_CAL_DATA_ADDR >> 24) & 0xff;
    req->data[4] = SWITCH_USER_STICK_CAL_DATA_SIZE;
    send_subcmd(d, req, sizeof(out));
}

static void fsm_read_factory_imu_calibration(struct uni_hid_device_s* d) {
    switch_instance_t* ins = get_switch_instance(d);
    ins->state = STATE_READ_FACTORY_IMU_CALIBRATION;

    uint8_t out[sizeof(struct switch_subcmd_request) + 5] = {0};
    struct switch_subcmd_request* req = (struct switch_subcmd_request*)&out[0];
    req->report_id = 0x01;  // 0x01 for sub commands
    req->subcmd_id = SUBCMD_SPI_FLASH_READ;
    // Address to read from: stick calibration
    req->data[0] = SWITCH_FACTORY_IMU_CAL_DATA_ADDR & 0xff;
    req->data[1] = (SWITCH_FACTORY_IMU_CAL_DATA_ADDR >> 8) & 0xff;
    req->data[2] = (SWITCH_FACTORY_IMU_CAL_DATA_ADDR >> 16) & 0xff;
    req->data[3] = (SWITCH_FACTORY_IMU_CAL_DATA_ADDR >> 24) & 0xff;
    req->data[4] = SWITCH_FACTORY_IMU_CAL_DATA_SIZE;
    send_subcmd(d, req, sizeof(out));
}

static void fsm_set_full_report(struct uni_hid_device_s* d) {
    switch_instance_t* ins = get_switch_instance(d);
    ins->state = STATE_SET_FULL_REPORT;

    uint8_t out[sizeof(struct switch_subcmd_request) + 1] = {0};
    struct switch_subcmd_request* req = (struct switch_subcmd_request*)&out[0];
    req->report_id = 0x01;  // 0x01 for sub commands
    req->subcmd_id = SUBCMD_SET_REPORT_MODE;
    req->data[0] = 0x30; /* type of report: standard, full */
    send_subcmd(d, req, sizeof(out));
}

static void fsm_enable_imu(struct uni_hid_device_s* d) {
    switch_instance_t* ins = get_switch_instance(d);
    ins->state = STATE_ENABLE_IMU;

    uint8_t out[sizeof(struct switch_subcmd_request) + 1] = {0};
    struct switch_subcmd_request* req = (struct switch_subcmd_request*)&out[0];
    req->report_id = 0x01;  // 0x01 for sub commands
    req->subcmd_id = SUBCMD_ENABLE_IMU;
    req->data[0] = (ins->mode == SWITCH_MODE_IMU);
    send_subcmd(d, req, sizeof(out));
}

static void fsm_update_led(struct uni_hid_device_s* d) {
    switch_instance_t* ins = get_switch_instance(d);
    ins->state = STATE_UPDATE_LED;

    set_led(d, ins->gamepad_seat);
}

static void fsm_ready(struct uni_hid_device_s* d) {
    switch_instance_t* ins = get_switch_instance(d);

    ins->state = STATE_READY;
    logi("Switch: gamepad is ready!\n");
    uni_hid_device_set_ready_complete(d);

    // So that it can end gracefully, disabling the timer
    process_fsm(d);
}

static struct switch_rumble_freq_data find_rumble_freq(uint16_t freq) {
    unsigned int i = 0;
    if (freq > rumble_freqs[0].freq) {
        for (i = 1; i < TOTAL_RUMBLE_FREQS - 1; i++) {
            if (freq > rumble_freqs[i - 1].freq && freq <= rumble_freqs[i].freq)
                break;
        }
    }

    return rumble_freqs[i];
}

static struct switch_rumble_amp_data find_rumble_amp(uint16_t amp) {
    unsigned int i = 0;
    if (amp > rumble_amps[0].amp) {
        for (i = 1; i < TOTAL_RUMBLE_AMPS - 1; i++) {
            if (amp > rumble_amps[i - 1].amp && amp <= rumble_amps[i].amp)
                break;
        }
    }

    return rumble_amps[i];
}

static void switch_encode_rumble(uint8_t* data, uint16_t freq_low, uint16_t freq_high, uint16_t amp) {
    struct switch_rumble_freq_data freq_data_low;
    struct switch_rumble_freq_data freq_data_high;
    struct switch_rumble_amp_data amp_data;

    freq_data_low = find_rumble_freq(freq_low);
    freq_data_high = find_rumble_freq(freq_high);
    amp_data = find_rumble_amp(amp);

    data[0] = (freq_data_high.high >> 8) & 0xFF;
    data[1] = (freq_data_high.high & 0xFF) + amp_data.high;
    data[2] = freq_data_low.low + ((amp_data.low >> 8) & 0xFF);
    data[3] = amp_data.low & 0xFF;
}

void uni_hid_parser_switch_set_player_leds(uni_hid_device_t* d, uint8_t leds) {
    switch_instance_t* ins = get_switch_instance(d);
    // Seat must be set, even if it is not ready. Initialization will use this
    // seat and set the correct LEDs values.
    ins->gamepad_seat = leds;

    if (ins->state < STATE_READY)
        return;

    set_led(d, leds);
}

void uni_hid_parser_switch_play_dual_rumble(struct uni_hid_device_s* d,
                                            uint16_t start_delay_ms,
                                            uint16_t duration_ms,
                                            uint8_t weak_magnitude,
                                            uint8_t strong_magnitude) {
    if (d == NULL) {
        loge("Switch: Invalid device\n");
        return;
    }

    switch_instance_t* ins = get_switch_instance(d);
    switch (ins->rumble_state) {
        case SWITCH_STATE_RUMBLE_DELAYED:
            btstack_run_loop_remove_timer(&ins->rumble_timer_delayed_start);
            break;
        case SWITCH_STATE_RUMBLE_IN_PROGRESS:
            btstack_run_loop_remove_timer(&ins->rumble_timer_duration);
            break;
        default:
            // Do nothing
            break;
    }

    if (start_delay_ms == 0) {
        switch_play_dual_rumble_now(d, duration_ms, weak_magnitude, strong_magnitude);
    } else {
        // Set timer to have a delayed start
        ins->rumble_timer_delayed_start.process = &on_switch_set_rumble_on;
        ins->rumble_timer_delayed_start.context = d;
        ins->rumble_state = SWITCH_STATE_RUMBLE_DELAYED;
        ins->rumble_duration_ms = duration_ms;
        ins->rumble_strong_magnitude = strong_magnitude;
        ins->rumble_weak_magnitude = weak_magnitude;

        btstack_run_loop_set_timer(&ins->rumble_timer_delayed_start, start_delay_ms);
        btstack_run_loop_add_timer(&ins->rumble_timer_delayed_start);
    }
}

bool uni_hid_parser_switch_does_name_match(struct uni_hid_device_s* d, const char* name) {
    struct device_s {
        const char* name;
        int vid;
        int pid;
    };

    struct device_s devices[] = {
        // Clones might not respond to SDP queries. Support them by name.
        {"Pro Controller", NINTENDO_VID, SWITCH_PRO_CONTROLLER_PID},
        {"Joy-Con (L)", NINTENDO_VID, SWITCH_JOYCON_L_PID},
        {"Joy-Con (R)", NINTENDO_VID, SWITCH_JOYCON_R_PID},
        {"SNES Controller", NINTENDO_VID, SWITCH_ONLINE_SNES_CONTROLLER_PID},
#if 0
        // TODO: Untested. What are the real names for N64 and SEGA controllers.
        {"N64 Controller", NINTENDO_VID, SWITCH_ONLINE_N64_CONTROLLER_PID},
        {"SEGA Controller", NINTENDO_VID, SWITCH_ONLINE_SEGA_CONTROLLER_PID},
#endif
    };

    for (long unsigned i = 0; i < ARRAY_SIZE(devices); i++) {
        if (strcmp(devices[i].name, name) == 0) {
            // Fake VID/PID
            uni_hid_device_set_vendor_id(d, devices[i].vid);
            uni_hid_device_set_product_id(d, devices[i].pid);
            return true;
        }
    }
    return false;
}

//
// Helpers
//
static switch_instance_t* get_switch_instance(uni_hid_device_t* d) {
    return (switch_instance_t*)&d->parser_data[0];
}

static void set_led(uni_hid_device_t* d, uint8_t leds) {
    switch_instance_t* ins = get_switch_instance(d);
    ins->gamepad_seat = leds;

    // 1 == SET_LEDS subcmd len
    uint8_t report[sizeof(struct switch_subcmd_request) + 1] = {0};

    struct switch_subcmd_request* req = (struct switch_subcmd_request*)&report[0];
    req->report_id = OUTPUT_RUMBLE_AND_SUBCMD;  // 0x01 for sub commands
    req->subcmd_id = SUBCMD_SET_PLAYER_LEDS;
    // LSB: turn on LEDs, MSB: flash LEDs
    // Official Switch doesn't honor the flash bit.
    // 8BitDo in Switch mode: LEDs are not working
    // White-label Switch clone: works Ok with flash LEDs
    req->data[0] = leds & 0x0f;
    send_subcmd(d, req, sizeof(report));
}

static void send_subcmd(uni_hid_device_t* d, struct switch_subcmd_request* r, int len) {
    static uint8_t packet_num = 0;
    r->packet_num = packet_num++;
    if (packet_num > 0x0f)
        packet_num = 0;
    r->transaction_type = (HID_MESSAGE_TYPE_DATA << 4) | HID_REPORT_TYPE_OUTPUT;
    uni_hid_device_send_intr_report(d, (const uint8_t*)r, len);
}

static int32_t calibrate_axis(int32_t v, switch_cal_stick_t cal) {
    int32_t ret;
    if (v > cal.center) {
        ret = (v - cal.center) * AXIS_NORMALIZE_RANGE / 2;
        ret /= (cal.max - cal.center);
    } else {
        ret = (cal.center - v) * -AXIS_NORMALIZE_RANGE / 2;
        ret /= (cal.center - cal.min);
    }
    // Clamp it
    ret = ret > (AXIS_NORMALIZE_RANGE / 2) ? (AXIS_NORMALIZE_RANGE / 2) : ret;
    ret = ret < -AXIS_NORMALIZE_RANGE / 2 ? -AXIS_NORMALIZE_RANGE / 2 : ret;
    return ret;
}

static void switch_stop_rumble_now(uni_hid_device_t* d) {
    switch_instance_t* ins = get_switch_instance(d);

    // No need to protect it with a mutex since it runs in the same main thread
    assert(ins->rumble_state == SWITCH_STATE_RUMBLE_IN_PROGRESS);
    ins->rumble_state = SWITCH_STATE_RUMBLE_DISABLED;

    struct switch_subcmd_request req = {0};

    req.report_id = OUTPUT_RUMBLE_ONLY;
    uint8_t rumble_default[4] = {0x00, 0x01, 0x40, 0x40};
    memcpy(req.rumble_left, rumble_default, sizeof(req.rumble_left));
    memcpy(req.rumble_right, rumble_default, sizeof(req.rumble_left));

    // Rumble request don't include the last byte of "switch_subcmd_request": subcmd_id
    send_subcmd(d, (struct switch_subcmd_request*)&req, sizeof(req) - 1);
}

static void switch_play_dual_rumble_now(uni_hid_device_t* d,
                                        uint16_t duration_ms,
                                        uint8_t weak_magnitude,
                                        uint8_t strong_magnitude) {
    switch_instance_t* ins = get_switch_instance(d);

    if (duration_ms == 0) {
        if (ins->rumble_state != SWITCH_STATE_RUMBLE_DISABLED)
            switch_stop_rumble_now(d);
        return;
    }

    struct switch_subcmd_request req = {
        .report_id = OUTPUT_RUMBLE_ONLY,
    };
    switch_encode_rumble(req.rumble_left, weak_magnitude << 2, weak_magnitude, 500);
    switch_encode_rumble(req.rumble_right, strong_magnitude << 2, strong_magnitude, 500);

    // Rumble request don't include the last byte of "switch_subcmd_request": subcmd_id
    send_subcmd(d, &req, sizeof(req) - 1);

    // Set timer to turn off rumble
    ins->rumble_timer_duration.process = &on_switch_set_rumble_off;
    ins->rumble_timer_duration.context = d;
    ins->rumble_state = SWITCH_STATE_RUMBLE_IN_PROGRESS;
    btstack_run_loop_set_timer(&ins->rumble_timer_duration, duration_ms);
    btstack_run_loop_add_timer(&ins->rumble_timer_duration);
}

static void on_switch_set_rumble_on(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    switch_instance_t* ins = get_switch_instance(d);

    switch_play_dual_rumble_now(d, ins->rumble_duration_ms, ins->rumble_weak_magnitude, ins->rumble_strong_magnitude);
}

static void on_switch_set_rumble_off(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = btstack_run_loop_get_timer_context(ts);
    switch_stop_rumble_now(d);
}

void switch_setup_timeout_callback(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = btstack_run_loop_get_timer_context(ts);
    switch_instance_t* ins = get_switch_instance(d);
    logi("Switch: setup timer timeout, failed state: 0x%02x\n", ins->state);
    process_fsm(d);
}

void uni_hid_parser_switch_device_dump(uni_hid_device_t* d) {
    switch_instance_t* ins = get_switch_instance(d);
    logi("\tSwitch: FW version %d.%d\n", ins->firmware_version_hi, ins->firmware_version_lo);
}
