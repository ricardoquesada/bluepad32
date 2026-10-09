// SPDX-License-Identifier: Apache-2.0
// MegaCadeDev
// https://github.com/MegaCadeDev/OGX-Mini-2026

/**
 * @file uni_hid_parser_switch2.c
 * @brief Nintendo Switch 2 BLE GATT driver & input report parser
 *        (Pro Controller 2 `057e:2069`, Joy-Con 2 Left `057e:2067`, Joy-Con 2 Right `057e:2066`).
 *
 * ### Architectural Overview
 * Nintendo Switch 2 controllers do not implement the standard BLE HID Over GATT Profile
 * (HOGP, UUID `0x1812`). Instead, they advertise a custom Manufacturer Specific Data (`0xFF`)
 * payload and expose a proprietary 128-bit BLE GATT service containing four primary
 * characteristics:
 * - **Input Report (`sw2_input_report_uuid128`, default handle `0x000a`):** Streams 63-byte
 *   controller state notifications at ~133 Hz.
 * - **Command Write (`sw2_cmd_write_uuid128`, default handle `0x0014`):** Accepts 8-byte
 *   framed commands (`{cmd, 0x91, 0x01, subcmd, 0x00, len, 0x00, 0x00} + payload`) via
 *   GATT Write Without Response.
 * - **Vibration / Keepalive (`sw2_vibration_*_uuid128`, default handle `0x0016`):** Accepts
 *   33-byte (Pro Controller 2) or 17-byte (Joy-Con 2 L/R) HD Rumble 2 packets (`0x50 + seq`)
 *   via GATT Write Without Response. Must be written every ~5 ms (`SW2_KEEPALIVE_MS`) once
 *   ready or the controller drops the BLE link with an HCI connection timeout (`0x08`).
 * - **Command Response (`sw2_cmd_response_uuid128`, default handle `0x001a`):** Emits command
 *   acknowledgments and SPI flash read responses via GATT notifications.
 *
 * ### GATT Setup & Pairing State Machine
 * ```text
 * SW2_STATE_IDLE
 *   └─> SW2_STATE_DISCOVER_SERVICES (discover primary services, up to SW2_MAX_GATT_SERVICES)
 *         └─> SW2_STATE_DISCOVER_CHARS (bind 0x2b29/0x0004 gate + input/cmd_w/vib/cmd_rsp handles)
 *               └─> SW2_STATE_WRITE_BOOTSTRAP_GATE (500 ms stabilization -> write {0x01, 0x00} to 0x0004)
 *                     └─> SW2_STATE_ENABLE_CMD_NOTIFY (write {0x01, 0x00} to cmd_rsp CCCD 0x001b)
 *                           ├─> [AUTH/ENC/NOT_FOUND] Request Just Works SMP (AuthReq=0) -> retry CCCD
 *                           └─> SW2_STATE_INIT_SEQUENCE (all 13 steps 0..12 of sw2_init_sequence[])
 *                                 └─> SW2_STATE_READ_CALIBRATION (SPI read 0x02/0x04 @ 0x001fc042)
 *                                       ├─> [needs_pair] SW2_STATE_PAIRING
 *                                       │     (4-step 0x15 handshake: SET_MAC 0x01, LTK1 0x04, LTK2 0x02, FINISH 0x03)
 *                                       │     └─> SW2_STATE_ENABLE_INPUT_NOTIFY (write {0x01, 0x00} to input CCCD
 * 0x000b) └─> [reconnect] SW2_STATE_ENABLE_INPUT_NOTIFY └─> SW2_STATE_READY (conn params, LEDs, 5 ms keepalive,
 * ready_complete)
 * ```
 *
 * ### Non-Obvious Protocol Rationale
 * 1. **Direct CCCD Handle Writes (`value_handle + 1`) Without Descriptor Discovery:**
 *    Switch 2 firmware rejects ATT `READ_BY_TYPE_REQUEST` / `FIND_INFORMATION_REQUEST` queries
 *    for `0x2902` CCCD descriptors. We derive `cmd_response_cccd_handle = cmd_response_value_handle + 1`
 *    (`0x001b`) and `input_report_cccd_handle = input_report_value_handle + 1` (`0x000b`) and write
 *    `{0x01, 0x00}` directly via `gatt_client_write_characteristic_descriptor_using_descriptor_handle()`.
 * 2. **500 ms Post-Discovery Stabilization & 2-Byte Bootstrap Gate (`0x0004`):** After discovering
 *    all characteristics across the 3 services, Switch 2 firmware requires a ~500 ms stabilization
 *    pause followed by a 2-byte write `{0x01, 0x00}` to handle `0x0004` (`0x2b29`) before enabling
 *    command-response notifications on `0x001b`.
 * 3. **Hardware-Verified Command Ordering (`INIT_SEQUENCE` -> `READ_CALIBRATION` -> `PAIRING`):**
 *    Switch 2 controllers require the 13-step `sw2_init_sequence[]` to execute FIRST after
 *    `0x001b` notifications are enabled, followed by SPI stick calibration read (`0x02/0x04` at
 *    `0x001fc042`), and then (when `needs_pair == true`) the 4-step `0x15` pairing sequence
 *    (`SET_MAC`, `LTK1`, `LTK2`, `FINISH`) before enabling input notifications on `0x000b`.
 * 4. **Standalone Horizontal Joy-Con 2 Mode:** Each Joy-Con 2 Left (`057e:2067`) or Right
 *    (`057e:2066`) operates as an independent horizontal controller matching Bluepad32's
 *    Switch 1 Joy-Con convention (no cross-slot pair merging).
 */

#include "parser/uni_hid_parser_switch2.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <btstack.h>

#include "bt/uni_bt_defines.h"
#include "controller/uni_controller.h"
#include "controller/uni_controller_type.h"
#include "controller/uni_gamepad.h"
#include "parser/uni_hid_parser.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

#define SW2_MAX_GATT_SERVICES 3
#define SW2_SETUP_TIMEOUT_MS 15000
#define SW2_CMD_TIMEOUT_MS 2000
#define SW2_SETUP_STABILIZE_MS 500
#define SW2_KEEPALIVE_MS 5

#define SW2_DEFAULT_STICK_CENTER 2048
#define SW2_DEFAULT_STICK_SPAN 1600
#define SW2_DEFAULT_STICK_MIN (SW2_DEFAULT_STICK_CENTER - SW2_DEFAULT_STICK_SPAN)
#define SW2_DEFAULT_STICK_MAX (SW2_DEFAULT_STICK_CENTER + SW2_DEFAULT_STICK_SPAN)

#define SW2_CALIBRATION_USER_JOYSTICK_1 0x001fc042u

// Switch 2 command & subcommand opcodes
#define SW2_CMD_SPI 0x02
#define SW2_SUBCMD_SPI_READ 0x04

#define SW2_CMD_LEDS 0x09
#define SW2_SUBCMD_LEDS_SET_PLAYER 0x07

#define SW2_CMD_PAIRING 0x15
#define SW2_SUBCMD_PAIR_SET_MAC 0x01
#define SW2_SUBCMD_PAIR_LTK2 0x02
#define SW2_SUBCMD_PAIR_FINISH 0x03
#define SW2_SUBCMD_PAIR_LTK1 0x04

// Default attribute handles on Switch 2 GATT table (used as fallback if UUID discovery omits handles).
// Note: Handle 0x0012 is Output Report (Vibration only), whereas Handle 0x0016 is Output Report
// (Vibration + Command) which triggers a Command Response notification on 0x001a for every write.
#define SW2_DEFAULT_BOOTSTRAP_GATE_HANDLE 0x0004
#define SW2_DEFAULT_INPUT_REPORT_HANDLE 0x000a
#define SW2_DEFAULT_VIBRATION_HANDLE 0x0012
#define SW2_DEFAULT_CMD_WRITE_HANDLE 0x0014
#define SW2_DEFAULT_CMD_RESPONSE_HANDLE 0x001a

// IMU conversion scales to SI units:
// - Accelerometer: 4096 LSB/g (+/-8g full scale) -> m/s^2 (UNI_STANDARD_GRAVITY = 9.80665 m/s^2)
// - Gyroscope: 13371 LSB per 936 deg/s (+/-2000 dps full scale) -> rad/s (UNI_DEG_TO_RAD = pi / 180)
#define SW2_ACCEL_SCALE (UNI_STANDARD_GRAVITY / 4096.0f)
#define SW2_GYRO_SCALE ((936.0f / 13371.0f) * UNI_DEG_TO_RAD)

/**
 * @brief Switch 2 BLE GATT connection and initialization state machine states.
 */
typedef enum {
    SW2_STATE_IDLE = 0,              ///< Uninitialized or awaiting LE connection complete.
    SW2_STATE_DISCOVER_SERVICES,     ///< Discovering primary GATT services.
    SW2_STATE_DISCOVER_CHARS,        ///< Discovering characteristics across stored services.
    SW2_STATE_DISCOVER_DESCS,        ///< Optional descriptor discovery state.
    SW2_STATE_WRITE_BOOTSTRAP_GATE,  ///< Stabilizing 500 ms and writing `{0x01, 0x00}` to `0x0004`.
    SW2_STATE_ENABLE_CMD_NOTIFY,     ///< Writing `{0x01, 0x00}` to command-response CCCD (`0x001b`).
    SW2_STATE_READ_CALIBRATION,      ///< Reading user stick calibration from SPI flash (`0x001fc042`).
    SW2_STATE_PAIRING,               ///< Running 4-step `0x15` pairing handshake (when `needs_pair` is true).
    SW2_STATE_INIT_SEQUENCE,         ///< Running 13-step `sw2_init_sequence[]`.
    SW2_STATE_ENABLE_INPUT_NOTIFY,   ///< Writing `{0x01, 0x00}` to input-report CCCD (`0x000b`).
    SW2_STATE_READY,                 ///< Streaming 63-byte input notifications + 5 ms keepalive active.
    SW2_STATE_DISCONNECTED,          ///< Disconnected / torn down via `uni_hid_parser_switch2_deinit()`.
} sw2_state_t;

/**
 * @brief Per-axis 12-bit stick calibration bounds (`min <= center <= max`).
 */
typedef struct {
    int16_t center;
    int16_t min;
    int16_t max;
} sw2_cal_axis_t;

/**
 * @brief 2D thumbstick calibration parameters (X and Y axes).
 */
typedef struct {
    sw2_cal_axis_t x;
    sw2_cal_axis_t y;
} sw2_cal_stick_t;

/**
 * @brief Per-device Switch 2 state stored in `d->parser_data[]`.
 *
 * Because this struct embeds `gatt_client_notification_t` and `btstack_timer_source_t`
 * (which contain 8-byte pointers on 64-bit host targets), `d->parser_data` in
 * `uni_hid_device_t` is annotated with `__attribute__((aligned(8)))` to prevent
 * UndefinedBehaviorSanitizer misaligned-pointer traps.
 */
typedef struct {
    sw2_state_t state;
    uint8_t init_step;
    uint8_t pairing_step;
    uint8_t service_count;
    uint8_t service_idx;
    uint8_t desc_target;
    bool notify_registered;
    bool setup_timer_active;
    bool keepalive_active;
    bool calibrated;
    bool paired_from_bond;
    bool needs_pair;
    bool smp_requested;
    bool waiting_encryption;
    bool rumble_active;
    bool has_imu_ts;
    bool player_leds_dirty;
    uint8_t player_leds;
    uint8_t rumble_seq;
    uint8_t rumble_weak;
    uint8_t rumble_strong;
    int8_t temperature_c;
    uint16_t last_imu_ts;
    uint32_t rumble_start_ms;
    uint32_t rumble_end_ms;
    uint16_t input_report_value_handle;
    uint16_t input_report_end_handle;
    uint16_t input_report_cccd_handle;
    uint16_t cmd_write_handle;
    uint16_t cmd_response_value_handle;
    uint16_t cmd_response_end_handle;
    uint16_t cmd_response_cccd_handle;
    uint16_t vibration_handle;
    uint16_t bootstrap_gate_value_handle;
    gatt_client_service_t services[SW2_MAX_GATT_SERVICES];
    gatt_client_notification_t notify_listener;
    btstack_timer_source_t setup_timer;
    btstack_timer_source_t keepalive_timer;
    sw2_cal_stick_t cal_left;
    sw2_cal_stick_t cal_right;
} sw2_instance_t;
_Static_assert(sizeof(sw2_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "sw2_instance_t too large");

// GATT 128-bit Characteristic UUIDs (Little-Endian wire byte order; sw2_uuid128_matches checks both endiannesses)
static const uint8_t sw2_input_report_uuid128[16] = {0xd2, 0x7f, 0xdf, 0x09, 0x8f, 0x11, 0x8f, 0x82,
                                                     0xad, 0x49, 0xfe, 0x89, 0xbe, 0xe9, 0x7d, 0xab};

static const uint8_t sw2_cmd_write_uuid128[16] = {0x05, 0xf0, 0xe5, 0x4f, 0xa5, 0x1e, 0x44, 0xaf,
                                                  0x6c, 0x4e, 0xb7, 0x8e, 0x64, 0x9d, 0x4a, 0xc9};

static const uint8_t sw2_cmd_response_uuid128[16] = {0x6a, 0x83, 0x11, 0xb1, 0x15, 0x53, 0x0a, 0xa2,
                                                     0x36, 0x4d, 0xd8, 0xd9, 0x61, 0xa9, 0x65, 0xc7};

static const uint8_t sw2_vibration_pro_uuid128[16] = {0x05, 0x2b, 0xf7, 0x31, 0x0c, 0x63, 0x39, 0xa9,
                                                      0x7d, 0x42, 0x58, 0x92, 0x51, 0x3f, 0x48, 0xcc};

static const uint8_t sw2_vibration_joycon_l_uuid128[16] = {0x41, 0x82, 0xf1, 0x14, 0x0c, 0x24, 0xf4, 0xa8,
                                                           0x5d, 0x48, 0x71, 0xa4, 0xcb, 0x26, 0x93, 0x28};

static const uint8_t sw2_vibration_joycon_r_uuid128[16] = {0x49, 0xc1, 0x00, 0x9e, 0xb0, 0xbb, 0xa1, 0x84,
                                                           0xa7, 0x46, 0x1f, 0xcd, 0xfb, 0xb0, 0x19, 0xfa};

static bool sw2_uuid128_matches(const uint8_t actual[16], const uint8_t expected_le[16]) {
    if (memcmp(actual, expected_le, 16) == 0)
        return true;
    for (int i = 0; i < 16; i++) {
        if (actual[i] != expected_le[15 - i])
            return false;
    }
    return true;
}

// Switch 2 Pairing Long-Term Keys exchanged during SW2_STATE_PAIRING (subcommands 0x04 and 0x02)
static const uint8_t sw2_ltk1[17] = {0x00, 0xea, 0xbd, 0x47, 0x13, 0x89, 0x35, 0x42, 0xc6,
                                     0x79, 0xee, 0x07, 0xf2, 0x53, 0x2c, 0x6c, 0x31};

static const uint8_t sw2_ltk2[17] = {0x00, 0x40, 0xb0, 0x8a, 0x5f, 0xcd, 0x1f, 0x9b, 0x41,
                                     0x12, 0x5c, 0xac, 0xc6, 0x3f, 0x38, 0xa0, 0x73};

typedef struct {
    uint8_t cmd;
    uint8_t subcmd;
    uint8_t len;
    const uint8_t* payload;
} sw2_init_cmd_t;

static const uint8_t sw2_init_p03_0d[] = {0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const uint8_t sw2_init_p15_03[] = {0x00};
// Feature flags bitmask: 0x01 (Buttons) | 0x02 (Analog sticks) | 0x04 (IMU) | 0x20 (Rumble) = 0x27.
// Matches SDL_hidapi_switch2.c; avoids setting unused bit 3 (0x08) which can suppress IMU on Joy-Con 2.
static const uint8_t sw2_init_p0c_02[] = {0x27, 0x00, 0x00, 0x00};
static const uint8_t sw2_init_p0a_08[] = {0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x35,
                                          0x00, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t sw2_init_p0c_04[] = {0x27, 0x00, 0x00, 0x00};
// Select Common Input Report 0x05 (carried on GATT handle 0x000a across all Switch 2 controllers).
static const uint8_t sw2_init_p03_0a[] = {0x05, 0x00, 0x00, 0x00};
static const uint8_t sw2_init_p01_01[] = {0x00, 0x00, 0x00, 0x00};
static const uint8_t sw2_init_p09_07[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 14-step initialization command sequence sent on cmd_write_handle (0x0014).
static const sw2_init_cmd_t sw2_init_sequence[] = {
    {0x03, 0x0d, sizeof(sw2_init_p03_0d), sw2_init_p03_0d},
    {0x07, 0x01, 0, NULL},
    {0x16, 0x01, 0, NULL},
    {0x15, 0x03, sizeof(sw2_init_p15_03), sw2_init_p15_03},
    {0x0c, 0x02, sizeof(sw2_init_p0c_02), sw2_init_p0c_02},
    {0x11, 0x03, 0, NULL},
    {0x0a, 0x08, sizeof(sw2_init_p0a_08), sw2_init_p0a_08},
    {0x11, 0x01, 0, NULL},
    {0x0c, 0x04, sizeof(sw2_init_p0c_04), sw2_init_p0c_04},
    {0x03, 0x0a, sizeof(sw2_init_p03_0a), sw2_init_p03_0a},
    {0x10, 0x01, 0, NULL},
    {0x01, 0x0c, 0, NULL},
    {0x01, 0x01, sizeof(sw2_init_p01_01), sw2_init_p01_01},
    {0x09, 0x07, sizeof(sw2_init_p09_07), sw2_init_p09_07},
};

static sw2_instance_t* get_sw2_instance(struct uni_hid_device_s* d) {
    return (sw2_instance_t*)&d->parser_data[0];
}

static const sw2_instance_t* get_sw2_instance_const(const struct uni_hid_device_s* d) {
    return (const sw2_instance_t*)&d->parser_data[0];
}

static void sw2_set_default_calibration(sw2_instance_t* ins) {
    ins->cal_left.x.center = SW2_DEFAULT_STICK_CENTER;
    ins->cal_left.x.min = SW2_DEFAULT_STICK_MIN;
    ins->cal_left.x.max = SW2_DEFAULT_STICK_MAX;
    ins->cal_left.y.center = SW2_DEFAULT_STICK_CENTER;
    ins->cal_left.y.min = SW2_DEFAULT_STICK_MIN;
    ins->cal_left.y.max = SW2_DEFAULT_STICK_MAX;

    ins->cal_right.x.center = SW2_DEFAULT_STICK_CENTER;
    ins->cal_right.x.min = SW2_DEFAULT_STICK_MIN;
    ins->cal_right.x.max = SW2_DEFAULT_STICK_MAX;
    ins->cal_right.y.center = SW2_DEFAULT_STICK_CENTER;
    ins->cal_right.y.min = SW2_DEFAULT_STICK_MIN;
    ins->cal_right.y.max = SW2_DEFAULT_STICK_MAX;
    ins->calibrated = false;
}

static const char* sw2_state_to_str(sw2_state_t state) {
    switch (state) {
        case SW2_STATE_IDLE:
            return "idle";
        case SW2_STATE_DISCOVER_SERVICES:
            return "discover_services";
        case SW2_STATE_DISCOVER_CHARS:
            return "discover_chars";
        case SW2_STATE_DISCOVER_DESCS:
            return "discover_descs";
        case SW2_STATE_WRITE_BOOTSTRAP_GATE:
            return "bootstrap_gate";
        case SW2_STATE_ENABLE_CMD_NOTIFY:
            return "enable_cmd_notify";
        case SW2_STATE_READ_CALIBRATION:
            return "read_calibration";
        case SW2_STATE_PAIRING:
            return "pairing";
        case SW2_STATE_INIT_SEQUENCE:
            return "init_sequence";
        case SW2_STATE_ENABLE_INPUT_NOTIFY:
            return "enable_input_notify";
        case SW2_STATE_READY:
            return "ready";
        case SW2_STATE_DISCONNECTED:
            return "disconnected";
        default:
            return "unknown";
    }
}

bool uni_hid_parser_switch2_is_device(uint16_t vid, uint16_t pid) {
    if (vid != UNI_SW2_NINTENDO_VID)
        return false;
    return (pid == UNI_SW2_PRO_PID || pid == UNI_SW2_JOYCON_L_PID || pid == UNI_SW2_JOYCON_R_PID);
}

bool uni_hid_parser_switch2_is_switch2_device(uint16_t vid, uint16_t pid) {
    return uni_hid_parser_switch2_is_device(vid, pid);
}

bool uni_hid_parser_switch2_is_ble_device(const struct uni_hid_device_s* d) {
    if (!d)
        return false;
    return uni_hid_parser_switch2_is_device(d->vendor_id, d->product_id);
}

bool uni_hid_parser_switch2_needs_pair(const struct uni_hid_device_s* d) {
    if (!d)
        return false;
    return get_sw2_instance_const(d)->needs_pair;
}

void uni_hid_parser_switch2_set_needs_pair(struct uni_hid_device_s* d, bool needs_pair) {
    if (!d)
        return;
    get_sw2_instance(d)->needs_pair = needs_pair;
}

bool uni_hid_parser_switch2_parse_mfg_data(const uint8_t* mfg,
                                           uint8_t mfg_len,
                                           uint16_t* out_pid,
                                           uint8_t out_reconnect_mac[6],
                                           bool* out_needs_pair) {
    if (!mfg || mfg_len < 4)
        return false;

    uint16_t pid = 0;
    bool needs_pair = true;
    uint8_t reconnect_mac[6] = {0};

    // Format A: Full 18-byte Nintendo Switch 2 manufacturer data (company ID 0x0553 at [0..1],
    // Nintendo VID 0x057e at [5..6], PID at [7..8], reconnect MAC at [12..17]).
    if (mfg_len >= 18 && little_endian_read_16(mfg, 0) == UNI_SW2_MFG_COMPANY_ID &&
        little_endian_read_16(mfg, 5) == UNI_SW2_NINTENDO_VID) {
        pid = little_endian_read_16(mfg, 7);
        memcpy(reconnect_mac, &mfg[12], 6);
        bool all_zero = true;
        for (int i = 0; i < 6; i++) {
            if (reconnect_mac[i] != 0) {
                all_zero = false;
                break;
            }
        }
        needs_pair = all_zero;
    } else if (little_endian_read_16(mfg, 0) == UNI_SW2_NINTENDO_VID) {
        // Format B: Direct Nintendo VID 0x057e at [0..1] with Switch 2 PID at [2..3] or [5..6].
        uint16_t pid_at_2 = little_endian_read_16(mfg, 2);
        if (uni_hid_parser_switch2_is_device(UNI_SW2_NINTENDO_VID, pid_at_2)) {
            pid = pid_at_2;
        } else if (mfg_len >= 7) {
            pid = little_endian_read_16(mfg, 5);
        }
        needs_pair = true;
    } else {
        return false;
    }

    if (!uni_hid_parser_switch2_is_device(UNI_SW2_NINTENDO_VID, pid))
        return false;

    if (out_pid)
        *out_pid = pid;
    if (out_reconnect_mac)
        memcpy(out_reconnect_mac, reconnect_mac, 6);
    if (out_needs_pair)
        *out_needs_pair = needs_pair;
    return true;
}

bool uni_hid_parser_switch2_does_packet_match(const uint8_t* packet, uint16_t size) {
    if (!packet || size < 12)
        return false;

    uint8_t raw_ad_len = gap_event_advertising_report_get_data_length(packet);
    uint16_t max_ad_len = (uint16_t)(size - 12);
    uint8_t ad_len = (raw_ad_len <= max_ad_len) ? raw_ad_len : (uint8_t)max_ad_len;
    const uint8_t* ad_data = gap_event_advertising_report_get_data(packet);

    uint16_t offset = 0;
    while (offset < ad_len) {
        uint8_t item_len = ad_data[offset];
        if (item_len == 0)
            break;
        if ((uint16_t)(offset + 1 + item_len) > ad_len)
            break;
        uint8_t item_type = ad_data[offset + 1];
        const uint8_t* item_data = &ad_data[offset + 2];
        uint8_t item_data_len = (uint8_t)(item_len - 1);

        if (item_type == BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA) {
            if (uni_hid_parser_switch2_parse_mfg_data(item_data, item_data_len, NULL, NULL, NULL)) {
                return true;
            }
        }
        offset = (uint16_t)(offset + 1 + item_len);
    }
    return false;
}

bool uni_bt_le_switch2_handle_advertisement(const uint8_t* packet, uint16_t size) {
    // GAP_EVENT_ADVERTISING_REPORT header is 12 bytes before AD payload.
    if (!packet || size < 12)
        return false;

    uint8_t raw_ad_len = gap_event_advertising_report_get_data_length(packet);
    uint16_t max_ad_len = (uint16_t)(size - 12);
    uint8_t ad_len = (raw_ad_len <= max_ad_len) ? raw_ad_len : (uint8_t)max_ad_len;
    const uint8_t* ad_data = gap_event_advertising_report_get_data(packet);

    uint16_t pid = 0;
    uint8_t reconnect_mac[6] = {0};
    bool needs_pair = false;
    bool matched = false;

    // Safely iterate AD structures with strict bounds checks against truncated AD items.
    uint16_t offset = 0;
    while (offset < ad_len) {
        uint8_t item_len = ad_data[offset];
        if (item_len == 0)
            break;
        if ((uint16_t)(offset + 1 + item_len) > ad_len)
            break;
        uint8_t item_type = ad_data[offset + 1];
        const uint8_t* item_data = &ad_data[offset + 2];
        uint8_t item_data_len = (uint8_t)(item_len - 1);

        if (item_type == BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA) {
            if (uni_hid_parser_switch2_parse_mfg_data(item_data, item_data_len, &pid, reconnect_mac, &needs_pair)) {
                matched = true;
                break;
            }
        }
        offset = (uint16_t)(offset + 1 + item_len);
    }

    if (!matched)
        return false;

    bd_addr_t addr;
    gap_event_advertising_report_get_address(packet, addr);
    uint8_t addr_type = gap_event_advertising_report_get_address_type(packet);
    uint8_t rssi = gap_event_advertising_report_get_rssi(packet);

    // When waking up or starting a SYNC button press, a Switch 2 controller initially
    // broadcasts reconnect advertisements targeting its previously paired console MAC
    // (`needs_pair == false`) before switching to `00:00:00:00:00:00` (`needs_pair == true`).
    // Ignore reconnect advertisements directed at a different host MAC so we do not open an
    // unencrypted connection while the user is still holding the SYNC button.
    // (When `gap_local_bd_addr` is all-zero in unit tests, allow synthetic reconnect packets.)
    if (!needs_pair) {
        bd_addr_t local_addr = {0};
        bd_addr_t zero_addr = {0};
        gap_local_bd_addr(local_addr);
        if (memcmp(local_addr, zero_addr, 6) != 0 && memcmp(reconnect_mac, local_addr, 6) != 0) {
            return false;
        }
    }

    uni_hid_device_t* existing = uni_hid_device_get_instance_for_address(addr);
    if (existing) {
        if (existing->conn.connected || uni_bt_conn_get_state(&existing->conn) == UNI_BT_CONN_STATE_DEVICE_READY) {
            return true;
        }
        // If the existing slot is still awaiting a pending LE connection and the advertisement
        // mode (needs_pair) has not changed, keep waiting for that connection attempt.
        if (existing->conn.handle == UNI_BT_CONN_HANDLE_INVALID &&
            uni_hid_parser_switch2_needs_pair(existing) == needs_pair) {
            return true;
        }
        logi("Switch2: cleaning up stale non-ready device slot for %s\n", bd_addr_to_str(addr));
        uni_hid_device_disconnect(existing);
        uni_hid_device_delete(existing);
    }

    const char* model_name = (pid == UNI_SW2_PRO_PID)        ? "Nintendo Switch 2 Pro Controller"
                             : (pid == UNI_SW2_JOYCON_L_PID) ? "Nintendo Switch 2 Joy-Con (L)"
                                                             : "Nintendo Switch 2 Joy-Con (R)";
    uint16_t cod = UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD;
    if (uni_hid_device_on_device_discovered(addr, model_name, cod, rssi) != UNI_ERROR_SUCCESS)
        return true;

    uni_hid_device_t* d = uni_hid_device_create(addr);
    if (!d)
        return true;

    uni_bt_conn_set_protocol(&d->conn, UNI_BT_CONN_PROTOCOL_BLE);
    uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_DEVICE_DISCOVERED);
    d->conn.rssi = rssi;
    uni_hid_device_set_cod(d, cod);
    uni_hid_device_set_name(d, model_name);
    uni_hid_device_set_vendor_id(d, UNI_SW2_NINTENDO_VID);
    uni_hid_device_set_product_id(d, pid);
    uni_hid_device_guess_controller_type_from_pid_vid(d);
    uni_hid_parser_switch2_set_needs_pair(d, needs_pair);

    if (needs_pair) {
        gap_delete_bonding((bd_addr_type_t)addr_type, addr);
    }

    gap_stop_scan();
    gap_connect(addr, (bd_addr_type_t)addr_type);
    return true;
}

static void sw2_kick_connection_timeout(uni_hid_device_t* d) {
    if (!d || d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;
    if (uni_bt_conn_get_state(&d->conn) == UNI_BT_CONN_STATE_DEVICE_READY)
        return;
    if (d->connection_timer.process != NULL) {
        btstack_run_loop_remove_timer(&d->connection_timer);
        btstack_run_loop_set_timer(&d->connection_timer, HID_DEVICE_CONNECTION_TIMEOUT_MS);
        btstack_run_loop_add_timer(&d->connection_timer);
    }
}

static void sw2_stop_setup_timer(sw2_instance_t* ins) {
    if (ins->setup_timer_active) {
        btstack_run_loop_remove_timer(&ins->setup_timer);
        ins->setup_timer_active = false;
    }
}

static void sw2_stop_keepalive_timer(sw2_instance_t* ins) {
    if (ins->keepalive_active) {
        btstack_run_loop_remove_timer(&ins->keepalive_timer);
        ins->keepalive_active = false;
    }
}

static void sw2_setup_timeout_cb(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = (uni_hid_device_t*)btstack_run_loop_get_timer_context(ts);
    if (!d)
        return;
    sw2_instance_t* ins = get_sw2_instance(d);
    btstack_run_loop_remove_timer(ts);
    ins->setup_timer_active = false;
    if (ins->state == SW2_STATE_READY || ins->state == SW2_STATE_DISCONNECTED)
        return;

    loge("Switch2: GATT setup timed out in state '%s' (%d), disconnecting\n", sw2_state_to_str(ins->state),
         (int)ins->state);
    ins->state = SW2_STATE_DISCONNECTED;
    uni_hid_device_disconnect(d);
}

static void sw2_arm_setup_timer(uni_hid_device_t* d, uint32_t timeout_ms) {
    if (!d || d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;
    sw2_instance_t* ins = get_sw2_instance(d);
    sw2_stop_setup_timer(ins);
    btstack_run_loop_set_timer_context(&ins->setup_timer, d);
    btstack_run_loop_set_timer_handler(&ins->setup_timer, &sw2_setup_timeout_cb);
    btstack_run_loop_set_timer(&ins->setup_timer, timeout_ms);
    btstack_run_loop_add_timer(&ins->setup_timer);
    ins->setup_timer_active = true;
}

static void sw2_pack_vibration(uint8_t out[5], uint16_t freq, uint16_t amp) {
    uint64_t v0 = (uint64_t)(freq & 0x01ffu);
    uint64_t v1 = (uint64_t)(amp & 0x03ffu);
    uint64_t v2 = (uint64_t)(freq & 0x01ffu);
    uint64_t v3 = (uint64_t)(amp & 0x03ffu);
    uint64_t packed = ((v0 << 0) | (v1 << 9) | (v2 << 19) | (v3 << 28)) & 0x7fffffffffull;
    for (int i = 0; i < 5; i++) {
        out[i] = (uint8_t)((packed >> (8 * i)) & 0xffu);
    }
}

static void sw2_build_pro_vibration_block(uint8_t out[16], uint8_t seq_nibble, const uint8_t motor[5]) {
    memset(out, 0, 16);
    out[0] = (uint8_t)(0x50u + (seq_nibble & 0x0fu));
    memcpy(&out[1], motor, 5);
    memcpy(&out[6], motor, 5);
    memcpy(&out[11], motor, 5);
}

static void sw2_send_vibration_packet(uni_hid_device_t* d, uint8_t weak_magnitude, uint8_t strong_magnitude) {
    if (!d || d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;
    sw2_instance_t* ins = get_sw2_instance(d);
    uint16_t handle = ins->vibration_handle ? ins->vibration_handle : SW2_DEFAULT_VIBRATION_HANDLE;

    uint8_t motor_l[5] = {0};
    uint8_t motor_r[5] = {0};
    if (weak_magnitude > 0 || strong_magnitude > 0) {
        uint16_t amp_l = (uint16_t)(((uint32_t)strong_magnitude * 850u) / 255u);
        uint16_t amp_r = (uint16_t)(((uint32_t)weak_magnitude * 850u) / 255u);
        if (amp_l == 0 && amp_r > 0)
            amp_l = amp_r;
        if (amp_r == 0 && amp_l > 0)
            amp_r = amp_l;
        sw2_pack_vibration(motor_l, 0x0e1u, amp_l);
        sw2_pack_vibration(motor_r, 0x0e1u, amp_r);
    }

    uint8_t seq = (uint8_t)(ins->rumble_seq & 0x0fu);
    ins->rumble_seq = (uint8_t)((ins->rumble_seq + 1u) & 0x0fu);

    if (d->product_id == UNI_SW2_PRO_PID) {
        // Pro Controller 2 (0x2069) expects a 33-byte payload:
        //   payload[0]:     0x00
        //   payload[1..16]: right motor block (0x50 + seq, 3x 5-byte packed motor frames)
        //   payload[17..32]: left motor block (0x50 + seq, 3x 5-byte packed motor frames)
        uint8_t payload[33];
        memset(payload, 0, sizeof(payload));
        payload[0] = 0x00;
        sw2_build_pro_vibration_block(&payload[1], seq, motor_r);
        sw2_build_pro_vibration_block(&payload[17], seq, motor_l);
        (void)gatt_client_write_value_of_characteristic_without_response(d->conn.handle, handle, sizeof(payload),
                                                                         payload);
    } else {
        // Joy-Con 2 Left (0x2067) and Right (0x2066) expect a 17-byte single-motor payload:
        //   payload[0]:     0x00
        //   payload[1]:     0x50 + seq
        //   payload[2..16]: 3x 5-byte packed motor frames
        const uint8_t* motor = (d->product_id == UNI_SW2_JOYCON_L_PID) ? motor_l : motor_r;
        uint8_t payload[17];
        memset(payload, 0, sizeof(payload));
        payload[0] = 0x00;
        payload[1] = (uint8_t)(0x50u + seq);
        memcpy(&payload[2], motor, 5);
        memcpy(&payload[7], motor, 5);
        memcpy(&payload[12], motor, 5);
        (void)gatt_client_write_value_of_characteristic_without_response(d->conn.handle, handle, sizeof(payload),
                                                                         payload);
    }
}

static uint8_t sw2_send_command(uni_hid_device_t* d,
                                uint8_t cmd,
                                uint8_t subcmd,
                                const uint8_t* payload,
                                uint8_t payload_len);

static uint8_t sw2_send_player_leds_cmd(uni_hid_device_t* d, uint8_t leds) {
    uint8_t val[8] = {(uint8_t)(leds & 0x0fu), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    return sw2_send_command(d, SW2_CMD_LEDS, SW2_SUBCMD_LEDS_SET_PLAYER, val, sizeof(val));
}

static void sw2_keepalive_timer_cb(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = (uni_hid_device_t*)btstack_run_loop_get_timer_context(ts);
    if (!d)
        return;
    sw2_instance_t* ins = get_sw2_instance(d);
    btstack_run_loop_remove_timer(ts);
    ins->keepalive_active = false;
    if (ins->state != SW2_STATE_READY || d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;

    if (ins->player_leds_dirty) {
        uint8_t status = sw2_send_player_leds_cmd(d, ins->player_leds);
        if (status != GATT_CLIENT_BUSY) {
            ins->player_leds_dirty = false;
        }
        if (status == ERROR_CODE_SUCCESS) {
            btstack_run_loop_set_timer_context(&ins->keepalive_timer, d);
            btstack_run_loop_set_timer_handler(&ins->keepalive_timer, &sw2_keepalive_timer_cb);
            btstack_run_loop_set_timer(&ins->keepalive_timer, SW2_KEEPALIVE_MS);
            btstack_run_loop_add_timer(&ins->keepalive_timer);
            ins->keepalive_active = true;
            return;
        }
    }

    // Switch 2 controllers require a periodic ~5 ms packet on vibration_handle (0x0012) to
    // prevent an HCI connection timeout (0x08). By evaluating the active rumble window
    // [rumble_start_ms, rumble_end_ms) inside each 5 ms keepalive tick, keepalive packets
    // sustain active vibration instead of overwriting it with silence.
    uint8_t weak = 0;
    uint8_t strong = 0;
    if (ins->rumble_active) {
        uint32_t now = btstack_run_loop_get_time_ms();
        if ((int32_t)(now - ins->rumble_end_ms) >= 0) {
            ins->rumble_active = false;
            ins->rumble_weak = 0;
            ins->rumble_strong = 0;
        } else if ((int32_t)(now - ins->rumble_start_ms) >= 0) {
            weak = ins->rumble_weak;
            strong = ins->rumble_strong;
        }
    }

    sw2_send_vibration_packet(d, weak, strong);

    btstack_run_loop_set_timer_context(&ins->keepalive_timer, d);
    btstack_run_loop_set_timer_handler(&ins->keepalive_timer, &sw2_keepalive_timer_cb);
    btstack_run_loop_set_timer(&ins->keepalive_timer, SW2_KEEPALIVE_MS);
    btstack_run_loop_add_timer(&ins->keepalive_timer);
    ins->keepalive_active = true;
}

static void sw2_start_keepalive_timer(uni_hid_device_t* d) {
    if (!d || d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;
    sw2_instance_t* ins = get_sw2_instance(d);
    sw2_stop_keepalive_timer(ins);
    btstack_run_loop_set_timer_context(&ins->keepalive_timer, d);
    btstack_run_loop_set_timer_handler(&ins->keepalive_timer, &sw2_keepalive_timer_cb);
    btstack_run_loop_set_timer(&ins->keepalive_timer, SW2_KEEPALIVE_MS);
    btstack_run_loop_add_timer(&ins->keepalive_timer);
    ins->keepalive_active = true;
}

static void sw2_advance_command_state(uni_hid_device_t* d, const uint8_t* value, uint16_t value_len);

static void sw2_cmd_timeout_cb(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = (uni_hid_device_t*)btstack_run_loop_get_timer_context(ts);
    if (!d)
        return;
    sw2_instance_t* ins = get_sw2_instance(d);
    btstack_run_loop_remove_timer(ts);
    ins->setup_timer_active = false;
    if (ins->state == SW2_STATE_READY || ins->state == SW2_STATE_DISCONNECTED)
        return;

    logi("Switch2: command response timeout in state '%s' (init=%u pair=%u), advancing\n", sw2_state_to_str(ins->state),
         ins->init_step, ins->pairing_step);
    sw2_advance_command_state(d, NULL, 0);
}

static void sw2_arm_cmd_timer(uni_hid_device_t* d, uint32_t timeout_ms) {
    if (!d || d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;
    sw2_instance_t* ins = get_sw2_instance(d);
    sw2_stop_setup_timer(ins);
    btstack_run_loop_set_timer_context(&ins->setup_timer, d);
    btstack_run_loop_set_timer_handler(&ins->setup_timer, &sw2_cmd_timeout_cb);
    btstack_run_loop_set_timer(&ins->setup_timer, timeout_ms);
    btstack_run_loop_add_timer(&ins->setup_timer);
    ins->setup_timer_active = true;
}

static uint8_t sw2_send_command(uni_hid_device_t* d,
                                uint8_t cmd,
                                uint8_t subcmd,
                                const uint8_t* payload,
                                uint8_t payload_len) {
    if (!d || d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    if (payload_len > 32)
        payload_len = 32;

    sw2_kick_connection_timeout(d);
    sw2_instance_t* ins = get_sw2_instance(d);
    uint16_t handle = ins->cmd_write_handle ? ins->cmd_write_handle : SW2_DEFAULT_CMD_WRITE_HANDLE;

    // 8-byte Switch 2 BLE command header followed by up to 32 bytes of command payload:
    //   [0]: command opcode (e.g., 0x02 SPI, 0x09 LEDs, 0x15 Pairing)
    //   [1]: 0x91 (host-to-controller BLE transport framing tag)
    //   [2]: 0x01
    //   [3]: subcommand opcode
    //   [4]: 0x00
    //   [5]: payload length in bytes
    //   [6..7]: 0x00, 0x00
    uint8_t buffer[8 + 32];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = cmd;
    buffer[1] = 0x91;
    buffer[2] = 0x01;
    buffer[3] = subcmd;
    buffer[4] = 0x00;
    buffer[5] = payload_len;
    buffer[6] = 0x00;
    buffer[7] = 0x00;
    if (payload && payload_len > 0) {
        memcpy(&buffer[8], payload, payload_len);
    }

    // Log rather than disconnecting on non-zero return status so synthetic GATT unit tests
    // (where no lower-level BTstack hci_connection_t exists) can drive the state machine;
    // real hardware stalls are caught by sw2_arm_cmd_timer / sw2_arm_setup_timer.
    uint8_t status = gatt_client_write_value_of_characteristic_without_response(d->conn.handle, handle,
                                                                                (uint16_t)(8u + payload_len), buffer);
    if (status != ERROR_CODE_SUCCESS) {
        logd("Switch2: cmd 0x%02x/0x%02x write returned status=0x%02x\n", cmd, subcmd, status);
    }
    return status;
}

static void sw2_send_spi_read_calibration(uni_hid_device_t* d) {
    uint8_t req[8] = {0x0b, 0x7e, 0x00, 0x00, 0x42, 0xc0, 0x1f, 0x00};
    little_endian_store_32(req, 4, SW2_CALIBRATION_USER_JOYSTICK_1);
    sw2_arm_cmd_timer(d, SW2_CMD_TIMEOUT_MS);
    sw2_send_command(d, SW2_CMD_SPI, SW2_SUBCMD_SPI_READ, req, sizeof(req));
}

static void sw2_send_pairing_step(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_sw2_instance(d);
    sw2_arm_cmd_timer(d, SW2_CMD_TIMEOUT_MS);
    switch (ins->pairing_step) {
        case 0: {
            bd_addr_t local_addr = {0};
            gap_local_bd_addr(local_addr);
            uint8_t buf[14];
            memset(buf, 0, sizeof(buf));
            buf[0] = 0x00;
            buf[1] = 0x02;
            memcpy(&buf[2], local_addr, 6);
            memcpy(&buf[8], local_addr, 6);
            sw2_send_command(d, SW2_CMD_PAIRING, SW2_SUBCMD_PAIR_SET_MAC, buf, sizeof(buf));
            break;
        }
        case 1:
            sw2_send_command(d, SW2_CMD_PAIRING, SW2_SUBCMD_PAIR_LTK1, sw2_ltk1, sizeof(sw2_ltk1));
            break;
        case 2:
            sw2_send_command(d, SW2_CMD_PAIRING, SW2_SUBCMD_PAIR_LTK2, sw2_ltk2, sizeof(sw2_ltk2));
            break;
        case 3: {
            uint8_t fin[1] = {0x00};
            sw2_send_command(d, SW2_CMD_PAIRING, SW2_SUBCMD_PAIR_FINISH, fin, sizeof(fin));
            break;
        }
        default:
            break;
    }
}

static void sw2_send_init_step(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_sw2_instance(d);
    if (ins->init_step >= ARRAY_SIZE(sw2_init_sequence))
        return;
    const sw2_init_cmd_t* step = &sw2_init_sequence[ins->init_step];
    sw2_arm_cmd_timer(d, SW2_CMD_TIMEOUT_MS);
    sw2_send_command(d, step->cmd, step->subcmd, step->payload, step->len);
}

static void sw2_write_cccd_notify(uni_hid_device_t* d, uint16_t cccd_handle) {
    static uint8_t cccd_enable[2] = {0x01, 0x00};
    sw2_kick_connection_timeout(d);
    (void)gatt_client_write_characteristic_descriptor_using_descriptor_handle(
        uni_hid_parser_switch2_handle_gatt_event, d->conn.handle, cccd_handle, sizeof(cccd_enable), cccd_enable);
}

static void sw2_start_enable_cmd_notify(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_sw2_instance(d);
    ins->state = SW2_STATE_ENABLE_CMD_NOTIFY;
    if (!ins->notify_registered) {
        gatt_client_listen_for_characteristic_value_updates(
            &ins->notify_listener, uni_hid_parser_switch2_handle_gatt_event, d->conn.handle, NULL);
        ins->notify_registered = true;
    }
    if (ins->cmd_response_cccd_handle == 0) {
        uint16_t base =
            ins->cmd_response_value_handle ? ins->cmd_response_value_handle : SW2_DEFAULT_CMD_RESPONSE_HANDLE;
        ins->cmd_response_cccd_handle = (uint16_t)(base + 1u);
    }
    sw2_arm_setup_timer(d, SW2_SETUP_TIMEOUT_MS);
    sw2_write_cccd_notify(d, ins->cmd_response_cccd_handle);
}

static void sw2_start_enable_input_notify(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_sw2_instance(d);
    ins->state = SW2_STATE_ENABLE_INPUT_NOTIFY;
    if (ins->input_report_cccd_handle == 0) {
        uint16_t base =
            ins->input_report_value_handle ? ins->input_report_value_handle : SW2_DEFAULT_INPUT_REPORT_HANDLE;
        ins->input_report_cccd_handle = (uint16_t)(base + 1u);
    }
    sw2_arm_setup_timer(d, SW2_SETUP_TIMEOUT_MS);
    sw2_write_cccd_notify(d, ins->input_report_cccd_handle);
}

static void sw2_on_setup_stabilized(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = (uni_hid_device_t*)btstack_run_loop_get_timer_context(ts);
    if (!d)
        return;
    sw2_instance_t* ins = get_sw2_instance(d);
    btstack_run_loop_remove_timer(ts);
    ins->setup_timer_active = false;
    if (ins->state != SW2_STATE_WRITE_BOOTSTRAP_GATE || d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;

    static uint8_t bootstrap_val[2] = {0x01, 0x00};
    uint16_t gate_handle =
        ins->bootstrap_gate_value_handle ? ins->bootstrap_gate_value_handle : SW2_DEFAULT_BOOTSTRAP_GATE_HANDLE;
    sw2_arm_setup_timer(d, SW2_SETUP_TIMEOUT_MS);
    uint8_t status = gatt_client_write_value_of_characteristic(uni_hid_parser_switch2_handle_gatt_event, d->conn.handle,
                                                               gate_handle, sizeof(bootstrap_val), bootstrap_val);
    if (status != ERROR_CODE_SUCCESS) {
        logi("Switch2: bootstrap gate write not started (status=0x%02x), proceeding to cmd notify\n", status);
        sw2_start_enable_cmd_notify(d);
    }
}

static void sw2_arm_stabilize_timer(uni_hid_device_t* d, uint32_t delay_ms) {
    if (!d || d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;
    sw2_instance_t* ins = get_sw2_instance(d);
    sw2_stop_setup_timer(ins);
    btstack_run_loop_set_timer_context(&ins->setup_timer, d);
    btstack_run_loop_set_timer_handler(&ins->setup_timer, &sw2_on_setup_stabilized);
    btstack_run_loop_set_timer(&ins->setup_timer, delay_ms);
    btstack_run_loop_add_timer(&ins->setup_timer);
    ins->setup_timer_active = true;
}

static bool sw2_parse_single_stick_cal(const uint8_t* data, sw2_cal_stick_t* out) {
    // Each stick's SPI flash calibration block is 9 bytes containing six 12-bit values:
    //   data[0..2]: center_x (12b), center_y (12b)
    //   data[3..5]: max_span_x (12b above center_x), max_span_y (12b above center_y)
    //   data[6..8]: min_span_x (12b below center_x), min_span_y (12b below center_y)
    // Reject all-0xFF (unprogrammed flash) or all-0x00 sentinel blocks.
    bool all_ff = true;
    bool all_zero = true;
    for (int i = 0; i < 9; i++) {
        if (data[i] != 0xff)
            all_ff = false;
        if (data[i] != 0x00)
            all_zero = false;
    }
    if (all_ff || all_zero)
        return false;

    int16_t cx = (int16_t)(data[0] | ((data[1] & 0x0f) << 8));
    int16_t cy = (int16_t)((data[1] >> 4) | (data[2] << 4));
    int16_t max_x = (int16_t)(data[3] | ((data[4] & 0x0f) << 8));
    int16_t max_y = (int16_t)((data[4] >> 4) | (data[5] << 4));
    int16_t min_x = (int16_t)(data[6] | ((data[7] & 0x0f) << 8));
    int16_t min_y = (int16_t)((data[7] >> 4) | (data[8] << 4));

    // Reject degenerate zero-span calibration (max <= center or center <= min).
    if (cx <= 0 || cy <= 0 || max_x <= 0 || max_y <= 0 || min_x <= 0 || min_y <= 0)
        return false;
    if (cx <= min_x || cy <= min_y)
        return false;

    out->x.center = cx;
    out->x.max = (int16_t)(cx + max_x);
    out->x.min = (int16_t)(cx - min_x);
    out->y.center = cy;
    out->y.max = (int16_t)(cy + max_y);
    out->y.min = (int16_t)(cy - min_y);
    return true;
}

static bool sw2_parse_spi_calibration(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    // 16-byte SPI response header + 9-byte single-stick calibration block = 25 bytes minimum
    // (real hardware returns 16 + 11 = 27 bytes when reading 0x0b bytes at 0x001fc042).
    if (!d || !report || len < 25)
        return false;
    if (report[0] != SW2_CMD_SPI)
        return false;
    if (little_endian_read_32(report, 12) != SW2_CALIBRATION_USER_JOYSTICK_1)
        return false;

    sw2_instance_t* ins = get_sw2_instance(d);
    const uint8_t* cal_data = &report[16];
    uint8_t spi_len = report[8];
    sw2_cal_stick_t left_cal;
    sw2_cal_stick_t right_cal;
    bool left_ok = sw2_parse_single_stick_cal(&cal_data[0], &left_cal);
    bool right_ok = false;

    // A single stick calibration occupies 9 bytes at SW2_CALIBRATION_USER_JOYSTICK_1 (0x1FC042).
    // Hardware SPI reads request 0x0b (11) bytes (27-byte response), so only parse a contiguous
    // second stick calibration at cal_data[9..17] when len >= 34 and the SPI length includes at
    // least 18 bytes (or 0 in synthetic unit test packets) on dual-stick controllers.
    if (d->controller_type != CONTROLLER_TYPE_Switch2JoyConLeft &&
        d->controller_type != CONTROLLER_TYPE_Switch2JoyConRight && len >= 34 && (spi_len == 0 || spi_len >= 18)) {
        right_ok = sw2_parse_single_stick_cal(&cal_data[9], &right_cal);
    }

    if (left_ok) {
        ins->cal_left = left_cal;
        // On Joy-Con 2 Right (0x2066), the single stick's calibration is stored in the first
        // stick slot in SPI flash (0x1FC042 / 0x130A8), matching SDL_hidapi_switch2.c.
        if (d->controller_type == CONTROLLER_TYPE_Switch2JoyConRight) {
            ins->cal_right = left_cal;
        }
    }
    if (right_ok)
        ins->cal_right = right_cal;
    if (left_ok || right_ok)
        ins->calibrated = true;
    return true;
}

static void sw2_advance_command_state(uni_hid_device_t* d, const uint8_t* value, uint16_t value_len) {
    sw2_instance_t* ins = get_sw2_instance(d);
    sw2_kick_connection_timeout(d);

    switch (ins->state) {
        case SW2_STATE_INIT_SEQUENCE:
            ins->init_step++;
            if (ins->init_step < ARRAY_SIZE(sw2_init_sequence)) {
                sw2_send_init_step(d);
            } else {
                ins->state = SW2_STATE_READ_CALIBRATION;
                sw2_send_spi_read_calibration(d);
            }
            break;

        case SW2_STATE_READ_CALIBRATION:
            if (value != NULL) {
                (void)sw2_parse_spi_calibration(d, value, value_len);
            }
            if (ins->needs_pair) {
                ins->state = SW2_STATE_PAIRING;
                ins->pairing_step = 0;
                sw2_send_pairing_step(d);
            } else {
                sw2_start_enable_input_notify(d);
            }
            break;

        case SW2_STATE_PAIRING:
            ins->pairing_step++;
            if (ins->pairing_step < 4) {
                sw2_send_pairing_step(d);
            } else {
                sw2_start_enable_input_notify(d);
            }
            break;

        default:
            break;
    }
}

void uni_hid_parser_switch2_handle_gatt_event(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET || !packet)
        return;

    uint8_t event = hci_event_packet_get_type(packet);
    hci_con_handle_t con_handle = UNI_BT_CONN_HANDLE_INVALID;

    switch (event) {
        case GATT_EVENT_SERVICE_QUERY_RESULT:
            con_handle = gatt_event_service_query_result_get_handle(packet);
            break;
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            con_handle = gatt_event_characteristic_query_result_get_handle(packet);
            break;
        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
            con_handle = gatt_event_all_characteristic_descriptors_query_result_get_handle(packet);
            break;
        case GATT_EVENT_QUERY_COMPLETE:
            con_handle = gatt_event_query_complete_get_handle(packet);
            break;
        case GATT_EVENT_NOTIFICATION:
            con_handle = gatt_event_notification_get_handle(packet);
            break;
        default:
            return;
    }

    uni_hid_device_t* d = uni_hid_device_get_instance_for_connection_handle(con_handle);
    if (!d)
        return;

    sw2_instance_t* ins = get_sw2_instance(d);

    if (event == GATT_EVENT_NOTIFICATION) {
        uint16_t value_handle = gatt_event_notification_get_value_handle(packet);
        uint16_t value_len = gatt_event_notification_get_value_length(packet);
        const uint8_t* value = gatt_event_notification_get_value(packet);

        uint16_t input_handle =
            ins->input_report_value_handle ? ins->input_report_value_handle : SW2_DEFAULT_INPUT_REPORT_HANDLE;
        uint16_t cmd_rsp_handle =
            ins->cmd_response_value_handle ? ins->cmd_response_value_handle : SW2_DEFAULT_CMD_RESPONSE_HANDLE;

        if (value_handle == input_handle && ins->state == SW2_STATE_READY) {
            uni_hid_parse_input_report(d, value, value_len);
            uni_hid_device_process_controller(d);
            return;
        }

        if (value_handle == cmd_rsp_handle) {
            if (value_len < 2 || (value[0] == 0x00 && value[1] == 0x00))
                return;
            if (value[1] != 0x01 && value[1] != 0x91 && !(value[0] == SW2_CMD_SPI && value_len >= 25))
                return;
            sw2_advance_command_state(d, value, value_len);
        }
        return;
    }

    switch (ins->state) {
        case SW2_STATE_DISCOVER_SERVICES:
            if (event == GATT_EVENT_SERVICE_QUERY_RESULT) {
                if (ins->service_count < SW2_MAX_GATT_SERVICES) {
                    gatt_event_service_query_result_get_service(packet, &ins->services[ins->service_count]);
                    ins->service_count++;
                }
            } else if (event == GATT_EVENT_QUERY_COMPLETE) {
                uint8_t att_status = gatt_event_query_complete_get_att_status(packet);
                if (att_status != ATT_ERROR_SUCCESS || ins->service_count == 0) {
                    loge("Switch2: service discovery failed (status=0x%02x, count=%u)\n", att_status,
                         ins->service_count);
                    uni_hid_device_disconnect(d);
                    break;
                }
                ins->state = SW2_STATE_DISCOVER_CHARS;
                ins->service_idx = 0;
                sw2_arm_setup_timer(d, SW2_SETUP_TIMEOUT_MS);
                (void)gatt_client_discover_characteristics_for_service(uni_hid_parser_switch2_handle_gatt_event,
                                                                       d->conn.handle, &ins->services[0]);
            }
            break;

        case SW2_STATE_DISCOVER_CHARS:
            if (event == GATT_EVENT_CHARACTERISTIC_QUERY_RESULT) {
                gatt_client_characteristic_t chr;
                gatt_event_characteristic_query_result_get_characteristic(packet, &chr);
                if (chr.uuid16 == 0x2b29 || chr.value_handle == SW2_DEFAULT_BOOTSTRAP_GATE_HANDLE) {
                    ins->bootstrap_gate_value_handle = chr.value_handle;
                }
                if (chr.value_handle == SW2_DEFAULT_INPUT_REPORT_HANDLE ||
                    sw2_uuid128_matches(chr.uuid128, sw2_input_report_uuid128)) {
                    ins->input_report_value_handle = chr.value_handle;
                    ins->input_report_end_handle = chr.end_handle;
                } else if (chr.value_handle == SW2_DEFAULT_CMD_WRITE_HANDLE ||
                           sw2_uuid128_matches(chr.uuid128, sw2_cmd_write_uuid128)) {
                    ins->cmd_write_handle = chr.value_handle;
                } else if (chr.value_handle == SW2_DEFAULT_CMD_RESPONSE_HANDLE ||
                           sw2_uuid128_matches(chr.uuid128, sw2_cmd_response_uuid128)) {
                    ins->cmd_response_value_handle = chr.value_handle;
                    ins->cmd_response_end_handle = chr.end_handle;
                } else if (sw2_uuid128_matches(chr.uuid128, sw2_vibration_pro_uuid128) ||
                           sw2_uuid128_matches(chr.uuid128, sw2_vibration_joycon_l_uuid128) ||
                           sw2_uuid128_matches(chr.uuid128, sw2_vibration_joycon_r_uuid128) ||
                           (ins->vibration_handle == 0 && chr.value_handle == SW2_DEFAULT_VIBRATION_HANDLE)) {
                    ins->vibration_handle = chr.value_handle;
                }
            } else if (event == GATT_EVENT_QUERY_COMPLETE) {
                ins->service_idx++;
                if (ins->service_idx < ins->service_count) {
                    sw2_arm_setup_timer(d, SW2_SETUP_TIMEOUT_MS);
                    (void)gatt_client_discover_characteristics_for_service(
                        uni_hid_parser_switch2_handle_gatt_event, d->conn.handle, &ins->services[ins->service_idx]);
                } else {
                    if (ins->input_report_value_handle == 0)
                        ins->input_report_value_handle = SW2_DEFAULT_INPUT_REPORT_HANDLE;
                    if (ins->cmd_write_handle == 0)
                        ins->cmd_write_handle = SW2_DEFAULT_CMD_WRITE_HANDLE;
                    if (ins->vibration_handle == 0)
                        ins->vibration_handle = SW2_DEFAULT_VIBRATION_HANDLE;
                    if (ins->cmd_response_value_handle == 0)
                        ins->cmd_response_value_handle = SW2_DEFAULT_CMD_RESPONSE_HANDLE;

                    ins->cmd_response_cccd_handle = (uint16_t)(ins->cmd_response_value_handle + 1u);
                    ins->input_report_cccd_handle = (uint16_t)(ins->input_report_value_handle + 1u);

                    // Skip GATT descriptor discovery (`0x2902` READ_BY_TYPE is rejected by Switch 2
                    // firmware) and enter the 500 ms stabilization window before writing the
                    // 2-byte `{0x01, 0x00}` bootstrap gate to handle 0x0004 (`0x2b29`).
                    ins->state = SW2_STATE_WRITE_BOOTSTRAP_GATE;
                    sw2_arm_stabilize_timer(d, SW2_SETUP_STABILIZE_MS);
                }
            }
            break;

        case SW2_STATE_WRITE_BOOTSTRAP_GATE:
            if (event == GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT) {
                gatt_client_characteristic_descriptor_t desc;
                gatt_event_all_characteristic_descriptors_query_result_get_characteristic_descriptor(packet, &desc);
                if (desc.uuid16 == ORG_BLUETOOTH_DESCRIPTOR_GATT_CLIENT_CHARACTERISTIC_CONFIGURATION) {
                    if (ins->desc_target == 0) {
                        ins->cmd_response_cccd_handle = desc.handle;
                        ins->desc_target = 1;
                    } else {
                        ins->input_report_cccd_handle = desc.handle;
                    }
                }
            } else if (event == GATT_EVENT_QUERY_COMPLETE) {
                uint8_t att_status = gatt_event_query_complete_get_att_status(packet);
                if (att_status != ATT_ERROR_SUCCESS) {
                    logi("Switch2: bootstrap gate write status=0x%02x (continuing)\n", att_status);
                }
                sw2_start_enable_cmd_notify(d);
            }
            break;

        case SW2_STATE_ENABLE_CMD_NOTIFY:
            if (event == GATT_EVENT_QUERY_COMPLETE) {
                uint8_t att_status = gatt_event_query_complete_get_att_status(packet);
                if ((att_status == ATT_ERROR_INSUFFICIENT_AUTHENTICATION ||
                     att_status == ATT_ERROR_INSUFFICIENT_ENCRYPTION || att_status == ATT_ERROR_ATTRIBUTE_NOT_FOUND) &&
                    !ins->smp_requested) {
                    logi("Switch2: cmd notify CCCD requires SMP (att_status=0x%02x), requesting Just Works\n",
                         att_status);
                    ins->smp_requested = true;
                    ins->waiting_encryption = true;
                    sm_set_authentication_requirements(0);
                    sm_request_pairing(d->conn.handle);
                    break;
                }
                if (att_status != ATT_ERROR_SUCCESS) {
                    loge("Switch2: enable cmd notify failed (att_status=0x%02x), disconnecting\n", att_status);
                    uni_hid_device_disconnect(d);
                    break;
                }
                ins->waiting_encryption = false;
                ins->state = SW2_STATE_INIT_SEQUENCE;
                ins->init_step = 0;
                sw2_send_init_step(d);
            }
            break;

        case SW2_STATE_ENABLE_INPUT_NOTIFY:
            if (event == GATT_EVENT_QUERY_COMPLETE) {
                uint8_t att_status = gatt_event_query_complete_get_att_status(packet);
                if ((att_status == ATT_ERROR_INSUFFICIENT_AUTHENTICATION ||
                     att_status == ATT_ERROR_INSUFFICIENT_ENCRYPTION || att_status == ATT_ERROR_ATTRIBUTE_NOT_FOUND) &&
                    !ins->smp_requested) {
                    logi("Switch2: input notify CCCD requires SMP (att_status=0x%02x), requesting Just Works\n",
                         att_status);
                    ins->smp_requested = true;
                    ins->waiting_encryption = true;
                    sm_set_authentication_requirements(0);
                    sm_request_pairing(d->conn.handle);
                    break;
                }
                if (att_status != ATT_ERROR_SUCCESS) {
                    loge("Switch2: enable input notify failed (att_status=0x%02x), disconnecting\n", att_status);
                    uni_hid_device_disconnect(d);
                    break;
                }
                ins->waiting_encryption = false;
                sw2_stop_setup_timer(ins);
                ins->state = SW2_STATE_READY;
                gap_update_connection_parameters(d->conn.handle, 6, 6, 0, 400);
                if (uni_bt_conn_get_state(&d->conn) != UNI_BT_CONN_STATE_DEVICE_READY) {
                    uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_DEVICE_PENDING_READY);
                    if (!uni_hid_device_set_ready_complete(d)) {
                        break;
                    }
                }
                if (!ins->player_leds) {
                    uni_hid_parser_switch2_set_player_leds(d, 0x01);
                } else if (ins->player_leds_dirty) {
                    uni_hid_parser_switch2_set_player_leds(d, ins->player_leds);
                }
                sw2_send_vibration_packet(d, 0, 0);
                sw2_start_keepalive_timer(d);
            }
            break;

        default:
            break;
    }
}

void uni_hid_parser_switch2_on_le_connected(struct uni_hid_device_s* d) {
    if (!d)
        return;
    sw2_kick_connection_timeout(d);
    uni_hid_device_guess_controller_type_from_pid_vid(d);
    if (!d->conn.connected) {
        uni_hid_device_connect(d);
    }
    uni_hid_device_set_ready(d);
}

void uni_hid_parser_switch2_on_encrypted(struct uni_hid_device_s* d) {
    if (!d)
        return;
    sw2_kick_connection_timeout(d);
    sw2_instance_t* ins = get_sw2_instance(d);
    ins->paired_from_bond = !ins->needs_pair;
    ins->waiting_encryption = false;
    if (ins->state == SW2_STATE_IDLE) {
        uni_hid_parser_switch2_on_le_connected(d);
    } else if (ins->state == SW2_STATE_ENABLE_CMD_NOTIFY) {
        sw2_arm_setup_timer(d, SW2_SETUP_TIMEOUT_MS);
        sw2_write_cccd_notify(d, ins->cmd_response_cccd_handle);
    } else if (ins->state == SW2_STATE_ENABLE_INPUT_NOTIFY) {
        sw2_arm_setup_timer(d, SW2_SETUP_TIMEOUT_MS);
        sw2_write_cccd_notify(d, ins->input_report_cccd_handle);
    }
}

void uni_hid_parser_switch2_setup(struct uni_hid_device_s* d) {
    if (!d)
        return;

    sw2_instance_t* ins = get_sw2_instance(d);
    bool saved_needs_pair = ins->needs_pair;
    bool saved_paired_from_bond = ins->paired_from_bond;
    uint8_t saved_player_leds = ins->player_leds;
    bool saved_player_leds_dirty = ins->player_leds_dirty;

    uni_hid_parser_switch2_deinit(d);
    memset(ins, 0, sizeof(*ins));
    ins->needs_pair = saved_needs_pair;
    ins->paired_from_bond = saved_paired_from_bond;
    ins->player_leds = saved_player_leds;
    ins->player_leds_dirty = saved_player_leds_dirty;
    sw2_set_default_calibration(ins);

    d->controller.klass = UNI_CONTROLLER_CLASS_GAMEPAD;

    // Guard synthetic/stack devices (`d->conn.handle == UNI_BT_CONN_HANDLE_INVALID`) so
    // `setup_synthetic_device()` in unit tests does not link stack timers into BTstack.
    if (d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;

    // Update this connection's BLE parameters via HCI_LE_Connection_Update (per-connection,
    // without mutating BTstack's global defaults for other BLE devices) to the minimum 7.5 ms
    // interval with 0 peripheral latency instead of BTstack's default (30 ms interval, latency 4):
    //   - conn_interval_min:   6 * 1.25 ms = 7.5 ms
    //   - conn_interval_max:   6 * 1.25 ms = 7.5 ms
    //   - conn_latency:        0 (no skipped connection events)
    //   - supervision_timeout: 400 * 10 ms = 4000 ms (4 s)
    gap_update_connection_parameters(d->conn.handle, 6, 6, 0, 400);

    ins->state = SW2_STATE_DISCOVER_SERVICES;
    sw2_arm_setup_timer(d, SW2_SETUP_TIMEOUT_MS);
    (void)gatt_client_discover_primary_services(uni_hid_parser_switch2_handle_gatt_event, d->conn.handle);
}

void uni_hid_parser_switch2_deinit(struct uni_hid_device_s* d) {
    if (!d)
        return;
    sw2_instance_t* ins = get_sw2_instance(d);
    sw2_stop_setup_timer(ins);
    sw2_stop_keepalive_timer(ins);
    if (ins->notify_registered) {
        gatt_client_stop_listening_for_characteristic_value_updates(&ins->notify_listener);
        ins->notify_registered = false;
    }
    ins->rumble_active = false;
    ins->state = SW2_STATE_DISCONNECTED;
}

void uni_hid_parser_switch2_init_report(struct uni_hid_device_s* d) {
    ARG_UNUSED(d);
}

static int32_t sw2_scale_raw_axis(int16_t raw, const sw2_cal_axis_t* cal) {
    if (!cal)
        return 0;
    int32_t delta = (int32_t)raw - (int32_t)cal->center;
    int32_t span =
        (delta >= 0) ? ((int32_t)cal->max - (int32_t)cal->center) : ((int32_t)cal->center - (int32_t)cal->min);
    if (span <= 0)
        return 0;
    return (delta * 512) / span;
}

static int32_t sw2_clamp_axis(int32_t val) {
    if (val > 511)
        return 511;
    if (val < -512)
        return -512;
    return val;
}

void uni_hid_parser_switch2_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    // Switch 2 63-byte BLE Input Notification Layout (handle 0x000a):
    //   report[0..2]:   Packet counter / transport header
    //   report[3]:      Battery percentage (0..100, scaled to 0..255)
    //   report[4..7]:   32-bit little-endian composed button word:
    //                     byte 4 (right): 0x01=Y, 0x02=X, 0x04=B, 0x08=A, 0x10=SR(R), 0x20=SL(R), 0x40=R, 0x80=ZR
    //                     byte 5 (shared): 0x01=Minus, 0x02=Plus, 0x04=R3, 0x08=L3, 0x10=Home, 0x20=Capture, 0x40=Chat
    //                     byte 6 (left):  0x01=Down, 0x02=Up, 0x04=Right, 0x08=Left, 0x10=SR(L), 0x20=SL(L), 0x40=L,
    //                     0x80=ZL byte 7 (extra): 0x01=GR, 0x02=GL
    //   report[10..12]: 12-bit packed left thumbstick:  lx = [10] | (([11] & 0x0f) << 8), ly = ([11] >> 4) | ([12] <<
    //   4) report[13..15]: 12-bit packed right thumbstick: rx = [13] | (([14] & 0x0f) << 8), ry = ([14] >> 4) | ([15]
    //   << 4)
    //   report[42..43]: 16-bit little-endian IMU sample timestamp
    //   report[43]:     Signed board temperature in degrees Celsius (int8_t)
    //   report[48..53]: 3-axis accelerometer (int16_t LE: ax, ay, az; 4096 LSB/g, +/-8g full scale)
    //   report[54..59]: 3-axis gyroscope (int16_t LE: gx, gy, gz; 13371 LSB per 936 deg/s, +/-2000 dps)
    if (!d || !report || len < 12)
        return;

    // Handle SPI Flash Calibration response (report[0] == 0x02, addr == 0x001fc042 at [12..15]).
    if (report[0] == SW2_CMD_SPI && len >= 25 && little_endian_read_32(report, 12) == SW2_CALIBRATION_USER_JOYSTICK_1) {
        (void)sw2_parse_spi_calibration(d, report, len);
        return;
    }

    sw2_instance_t* ins = get_sw2_instance(d);
    uni_gamepad_t* gp = &d->controller.gamepad;

    gp->dpad = 0;
    gp->buttons = 0;
    gp->misc_buttons = 0;
    gp->brake = 0;
    gp->throttle = 0;

    // Battery percentage at report[3] (0..100 -> 0..255)
    uint8_t bat_pct = report[3];
    if (bat_pct > 100)
        bat_pct = 100;
    d->controller.battery = (uint8_t)(((uint32_t)bat_pct * 255u) / 100u);

    // 32-bit composed button word at report[4..7]
    uint32_t b = little_endian_read_32(report, 4);

    // Unpack 12-bit sticks
    int16_t lx = SW2_DEFAULT_STICK_CENTER;
    int16_t ly = SW2_DEFAULT_STICK_CENTER;
    int16_t rx = SW2_DEFAULT_STICK_CENTER;
    int16_t ry = SW2_DEFAULT_STICK_CENTER;
    if (len >= 13) {
        lx = (int16_t)(report[10] | ((report[11] & 0x0f) << 8));
        ly = (int16_t)((report[11] >> 4) | (report[12] << 4));
    }
    if (len >= 16) {
        rx = (int16_t)(report[13] | ((report[14] & 0x0f) << 8));
        ry = (int16_t)((report[14] >> 4) | (report[15] << 4));
    }

    // Fallback for Joy-Con 2 compact input reports (0x07 Left / 0x08 Right) where report[4] == 0x07,
    // 16-bit buttons are at report[2..3], and the 12-bit stick is at report[5..7].
    if (lx == 0 && ly == 0 && rx == 0 && ry == 0 && report[4] == 0x07) {
        int16_t cx = (int16_t)(report[5] | ((report[6] & 0x0f) << 8));
        int16_t cy = (int16_t)((report[6] >> 4) | (report[7] << 4));
        if (d->controller_type == CONTROLLER_TYPE_Switch2JoyConLeft) {
            b = ((uint32_t)report[2] << 16) | ((uint32_t)report[3] << 8);
            lx = cx;
            ly = cy;
        } else if (d->controller_type == CONTROLLER_TYPE_Switch2JoyConRight) {
            b = (uint32_t)report[2] | ((uint32_t)report[3] << 8);
            rx = cx;
            ry = cy;
        }
    }

    int32_t cal_lx = sw2_scale_raw_axis(lx, &ins->cal_left.x);
    int32_t cal_ly = sw2_scale_raw_axis(ly, &ins->cal_left.y);
    int32_t cal_rx = sw2_scale_raw_axis(rx, &ins->cal_right.x);
    int32_t cal_ry = sw2_scale_raw_axis(ry, &ins->cal_right.y);

    switch (d->controller_type) {
        case CONTROLLER_TYPE_Switch2JoyConLeft:
            // Standalone horizontal Joy-Con 2 Left (rotated 90 deg CCW):
            // Physical D-Pad Left/Down/Up/Right -> face buttons A/B/X/Y
            if (b & 0x080000u)
                gp->buttons |= BUTTON_A;
            if (b & 0x010000u)
                gp->buttons |= BUTTON_B;
            if (b & 0x020000u)
                gp->buttons |= BUTTON_X;
            if (b & 0x040000u)
                gp->buttons |= BUTTON_Y;

            // Side rail SL/SR -> shoulders L/R
            if (b & 0x200000u)
                gp->buttons |= BUTTON_SHOULDER_L;
            if (b & 0x100000u)
                gp->buttons |= BUTTON_SHOULDER_R;

            // L/ZL -> triggers L/R
            if (b & 0x400000u) {
                gp->buttons |= BUTTON_TRIGGER_L;
                gp->brake = 1023;
            }
            if (b & 0x800000u) {
                gp->buttons |= BUTTON_TRIGGER_R;
                gp->throttle = 1023;
            }

            // Stick click & misc
            if (b & 0x000800u)
                gp->buttons |= BUTTON_THUMB_L;
            if (b & 0x000100u)
                gp->misc_buttons |= MISC_BUTTON_SELECT;
            if (b & 0x002000u)
                gp->misc_buttons |= MISC_BUTTON_CAPTURE;

            // Horizontal CCW stick rotation (matches SDL HandleMiniControllerStateL):
            // axis_x = -cal_y (invert = true), axis_y = -cal_x (invert = true)
            gp->axis_x = sw2_clamp_axis(-cal_ly);
            gp->axis_y = sw2_clamp_axis(-cal_lx);
            gp->axis_rx = 0;
            gp->axis_ry = 0;
            break;

        case CONTROLLER_TYPE_Switch2JoyConRight:
            // Standalone horizontal Joy-Con 2 Right (rotated 90 deg CW):
            // Physical A/X/B/Y -> face buttons A/B/X/Y
            if (b & 0x000008u)
                gp->buttons |= BUTTON_A;
            if (b & 0x000002u)
                gp->buttons |= BUTTON_B;
            if (b & 0x000004u)
                gp->buttons |= BUTTON_X;
            if (b & 0x000001u)
                gp->buttons |= BUTTON_Y;

            // Side rail SL/SR -> shoulders L/R
            if (b & 0x000020u)
                gp->buttons |= BUTTON_SHOULDER_L;
            if (b & 0x000010u)
                gp->buttons |= BUTTON_SHOULDER_R;

            // R/ZR -> triggers L/R
            if (b & 0x000040u) {
                gp->buttons |= BUTTON_TRIGGER_L;
                gp->brake = 1023;
            }
            if (b & 0x000080u) {
                gp->buttons |= BUTTON_TRIGGER_R;
                gp->throttle = 1023;
            }

            // Stick click & misc
            if (b & 0x000400u)
                gp->buttons |= BUTTON_THUMB_L;
            if (b & 0x000200u)
                gp->misc_buttons |= MISC_BUTTON_START;
            if (b & 0x001000u)
                gp->misc_buttons |= MISC_BUTTON_SYSTEM;

            // Horizontal CW stick rotation (matches SDL HandleMiniControllerStateR):
            // axis_x = cal_y (invert = false), axis_y = cal_x (invert = false)
            gp->axis_x = sw2_clamp_axis(cal_ry);
            gp->axis_y = sw2_clamp_axis(cal_rx);
            gp->axis_rx = 0;
            gp->axis_ry = 0;
            break;

        case CONTROLLER_TYPE_Switch2ProController:
        default:
            // Standard Switch Pro Controller 2 layout (reverse Nintendo face buttons)
            if (b & 0x000004u)
                gp->buttons |= BUTTON_A;
            if (b & 0x000008u)
                gp->buttons |= BUTTON_B;
            if (b & 0x000001u)
                gp->buttons |= BUTTON_X;
            if (b & 0x000002u)
                gp->buttons |= BUTTON_Y;

            if (b & 0x010000u)
                gp->dpad |= DPAD_DOWN;
            if (b & 0x020000u)
                gp->dpad |= DPAD_UP;
            if (b & 0x040000u)
                gp->dpad |= DPAD_RIGHT;
            if (b & 0x080000u)
                gp->dpad |= DPAD_LEFT;

            if (b & 0x400000u)
                gp->buttons |= BUTTON_SHOULDER_L;
            if (b & 0x000040u)
                gp->buttons |= BUTTON_SHOULDER_R;
            // Switch 2 Pro Controller has digital-only ZL/ZR triggers (like Joy-Con 2);
            // only the Switch 2 GameCube controller has analog triggers.
            if (b & 0x800000u) {
                gp->buttons |= BUTTON_TRIGGER_L;
                gp->brake = 1023;
            }
            if (b & 0x000080u) {
                gp->buttons |= BUTTON_TRIGGER_R;
                gp->throttle = 1023;
            }

            if (b & 0x000800u)
                gp->buttons |= BUTTON_THUMB_L;
            if (b & 0x000400u)
                gp->buttons |= BUTTON_THUMB_R;

            if (b & 0x000100u)
                gp->misc_buttons |= MISC_BUTTON_SELECT;
            if (b & 0x000200u)
                gp->misc_buttons |= MISC_BUTTON_START;
            if (b & 0x001000u)
                gp->misc_buttons |= MISC_BUTTON_SYSTEM;
            if (b & 0x002000u)
                gp->misc_buttons |= MISC_BUTTON_CAPTURE;

            gp->axis_x = sw2_clamp_axis(cal_lx);
            gp->axis_y = sw2_clamp_axis(-cal_ly);
            gp->axis_rx = sw2_clamp_axis(cal_rx);
            gp->axis_ry = sw2_clamp_axis(-cal_ry);
            break;
    }

    // Board temperature byte at report[43]
    if (len >= 44) {
        ins->temperature_c = (int8_t)report[43];
    }

    // 6-axis IMU at report[48..59] (requires len >= 60).
    // Converts raw sensor counts into Bluepad32's canonical right-handed Y-up SI coordinate frame
    // ([0] = +X right / pitch, [1] = +Y up / yaw, [2] = +Z toward player / roll; determinant = +1)
    // in m/s^2 (SW2_ACCEL_SCALE) and rad/s (SW2_GYRO_SCALE), matching SDL_hidapi_switch2.c:
    // - Pro Controller 2 (0x2069, native +X_s right, +Y_s forward, +Z_s up):
    //     [X, Y, Z] = [+X_s, +Z_s, -Y_s]
    // - Solo Joy-Con 2 Left (0x2067, rotated 90 deg CCW around +Z_s into horizontal grip):
    //     [X, Y, Z] = [-Y_s, +Z_s, -X_s]
    // - Solo Joy-Con 2 Right (0x2066, rotated 90 deg CW around +Z_s into horizontal grip):
    //     [X, Y, Z] = [+Y_s, +Z_s, +X_s]
    if (len >= 60) {
        uint16_t imu_ts = little_endian_read_16(report, 42);
        uint16_t sample_dt = ins->has_imu_ts ? (uint16_t)(imu_ts - ins->last_imu_ts) : 0;
        ins->last_imu_ts = imu_ts;
        ins->has_imu_ts = true;
        (void)sample_dt;

        int16_t ax = (int16_t)little_endian_read_16(report, 48);
        int16_t ay = (int16_t)little_endian_read_16(report, 50);
        int16_t az = (int16_t)little_endian_read_16(report, 52);
        int16_t gx = (int16_t)little_endian_read_16(report, 54);
        int16_t gy = (int16_t)little_endian_read_16(report, 56);
        int16_t gz = (int16_t)little_endian_read_16(report, 58);

        switch (d->controller_type) {
            case CONTROLLER_TYPE_Switch2JoyConLeft:
                // Solo Joy-Con 2 Left (horizontal 90 deg CCW, right-handed Y-up det = +1)
                gp->accel[0] = -(float)ay * SW2_ACCEL_SCALE;
                gp->accel[1] = (float)az * SW2_ACCEL_SCALE;
                gp->accel[2] = -(float)ax * SW2_ACCEL_SCALE;
                gp->gyro[0] = -(float)gy * SW2_GYRO_SCALE;
                gp->gyro[1] = (float)gz * SW2_GYRO_SCALE;
                gp->gyro[2] = -(float)gx * SW2_GYRO_SCALE;
                break;

            case CONTROLLER_TYPE_Switch2JoyConRight:
                // Solo Joy-Con 2 Right (horizontal 90 deg CW, right-handed Y-up det = +1)
                gp->accel[0] = (float)ay * SW2_ACCEL_SCALE;
                gp->accel[1] = (float)az * SW2_ACCEL_SCALE;
                gp->accel[2] = (float)ax * SW2_ACCEL_SCALE;
                gp->gyro[0] = (float)gy * SW2_GYRO_SCALE;
                gp->gyro[1] = (float)gz * SW2_GYRO_SCALE;
                gp->gyro[2] = (float)gx * SW2_GYRO_SCALE;
                break;

            case CONTROLLER_TYPE_Switch2ProController:
            default:
                // Pro Controller 2 (+X_s right, +Y_s forward, +Z_s up -> canonical right-handed Y-up)
                gp->accel[0] = (float)ax * SW2_ACCEL_SCALE;
                gp->accel[1] = (float)az * SW2_ACCEL_SCALE;
                gp->accel[2] = -(float)ay * SW2_ACCEL_SCALE;
                gp->gyro[0] = (float)gx * SW2_GYRO_SCALE;
                gp->gyro[1] = (float)gz * SW2_GYRO_SCALE;
                gp->gyro[2] = -(float)gy * SW2_GYRO_SCALE;
                break;
        }
    }
}

void uni_hid_parser_switch2_set_player_leds(struct uni_hid_device_s* d, uint8_t leds) {
    if (!d)
        return;
    sw2_instance_t* ins = get_sw2_instance(d);
    ins->player_leds = (uint8_t)(leds & 0x0fu);

    if (ins->state != SW2_STATE_READY) {
        ins->player_leds_dirty = true;
        return;
    }

    uint8_t status = sw2_send_player_leds_cmd(d, ins->player_leds);
    ins->player_leds_dirty = (status == GATT_CLIENT_BUSY);
}

void uni_hid_parser_switch2_play_dual_rumble(struct uni_hid_device_s* d,
                                             uint16_t start_delay_ms,
                                             uint16_t duration_ms,
                                             uint8_t weak_magnitude,
                                             uint8_t strong_magnitude) {
    if (!d)
        return;
    sw2_instance_t* ins = get_sw2_instance(d);

    if ((weak_magnitude == 0 && strong_magnitude == 0) || duration_ms == 0) {
        ins->rumble_active = false;
        ins->rumble_weak = 0;
        ins->rumble_strong = 0;
        sw2_send_vibration_packet(d, 0, 0);
        return;
    }

    uint32_t now = btstack_run_loop_get_time_ms();
    ins->rumble_start_ms = now + start_delay_ms;
    ins->rumble_end_ms = ins->rumble_start_ms + duration_ms;
    ins->rumble_weak = weak_magnitude;
    ins->rumble_strong = strong_magnitude;
    ins->rumble_active = true;

    if (start_delay_ms == 0) {
        sw2_send_vibration_packet(d, weak_magnitude, strong_magnitude);
    }
}

int uni_hid_parser_switch2_device_extra_info(const struct uni_hid_device_s* d, char* buf, size_t len) {
    if (!d || !buf || len == 0)
        return -1;
    const sw2_instance_t* ins = get_sw2_instance_const(d);
    return snprintf(buf, len, "state=%s, cal=%s, temp=%dC, in=0x%04x/0x%04x cmd=0x%04x/0x%04x/0x%04x vib=0x%04x",
                    sw2_state_to_str(ins->state), ins->calibrated ? "user" : "default", (int)ins->temperature_c,
                    ins->input_report_value_handle, ins->input_report_cccd_handle, ins->cmd_write_handle,
                    ins->cmd_response_value_handle, ins->cmd_response_cccd_handle, ins->vibration_handle);
}
