// SPDX-License-Identifier: Apache-2.0
// MegaCadeDev
// https://github.com/MegaCadeDev/OGX-Mini-2026
//
// Protocol reference: SDL3 hidapi_steam_triton + controller_structs.h,
// and community notes (pattontim/sc2-research). BLE state report ID 0x45.
// Pairing: hold RB + B + Steam on the controller.

/**
 * @file uni_hid_parser_steam_triton.c
 * @brief Valve Steam Controller 2026 ("Triton") BLE GATT driver & input report parser
 *        (`28de:1302` USB, `28de:1303` BLE, `28de:1304` Wireless Puck, `28de:1305` Nereid).
 *
 * ### Architectural Overview & Why HOGP Is Bypassed
 * On Steam Controller 2026 ("Triton") BLE hardware (`28de:1303`), connecting BTstack's
 * standard HID Over GATT Profile client (`hids_host_connect()`) after Device Information
 * Service (DIS, `0x180A`) completes stalls indefinitely during HID Report Map discovery.
 * Matching SDL3 (`SDL_hidapi_steam_triton.c`), `uni_bt_le.c` bypasses `hids_host_connect()`
 * for Triton devices and invokes `uni_hid_parser_steam_triton_setup()`, which drives
 * Valve's proprietary 128-bit GATT service (`100F6C32-1735-4313-B402-38567131E5F3`):
 *
 * ```text
 * TRITON_GATT_IDLE
 *   └─> TRITON_GATT_FIND_SERVICE (discover service 100F6C32-1735-4313-B402-38567131E5F3)
 *         └─> TRITON_GATT_FIND_CHARS (discover 100F6Cxx-... characteristics):
 *               • 0x7c: Timestamped BLE input stream (report 0x47, preferred)
 *               • 0x7a: Standard BLE input stream (report 0x45, fallback if 0x7c absent)
 *               • 0x34: Control / feature register write (report_value_handle)
 *               • 0xb5: Haptic rumble output report 0x80 (rumble_value_handle)
 *               └─> TRITON_GATT_ENABLE_NOTIFY (register listener + enable CCCD on 0x7c/0x7a)
 *                     └─> TRITON_GATT_READY (write k_enter_valve_mode {0xc0, 0x87, 0x03, 0x08, 0x07, 0x00}
 *                                            to 0x34, disarm conn_timer, call set_ready_complete)
 * ```
 *
 * ### State Report Layout (`0x42` USB, `0x45` BLE, `0x47` BLE Timestamped)
 * With `p = &report[1]` (`payload_len = len - 1`, minimum `29` bytes for buttons/triggers/sticks):
 * - `p[0]`:      Sequence / flags byte
 * - `p[1..4]`:   32-bit little-endian button bitmask (`TRITON_BTN_*`)
 * - `p[5..6]`:   Left trigger (`int16_t` LE, `0..32767` -> `brake` `0..1023`)
 * - `p[7..8]`:   Right trigger (`int16_t` LE, `0..32767` -> `throttle` `0..1023`)
 * - `p[9..16]`:  Left & right thumbsticks (`int16_t` LE: `lx, ly, rx, ry` -> `[-512, 511]`)
 * - `p[17..28]`: Trackpad coordinates & pressure fields
 * - `p[29..32]`: Sensor timestamp block:
 *                - Reports `0x42` / `0x45`: `imu_off = 29`, `imu_ts_size = 4` -> IMU at `p[33..44]`
 *                - Report `0x47`: 2 extra pad bytes + 2B timestamp (`imu_off = 31`, `imu_ts_size = 2`) -> IMU at
 * `p[33..44]`
 * - `p[33..38]`: 3-axis accelerometer (`int16_t` LE: `ax, ay, az`, `+/-2g` full scale = `16384 LSB/g`)
 * - `p[39..44]`: 3-axis gyroscope (`int16_t` LE: `gx, gy, gz`, `+/-2000 dps` full scale = `16.384 LSB/(deg/s)`)
 */

#include "parser/uni_hid_parser_steam_triton.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <btstack.h>

#include "bt/uni_bt_conn.h"
#include "controller/uni_controller.h"
#include "controller/uni_gamepad.h"
#include "parser/uni_hid_parser.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

#define TRITON_CONN_TIMEOUT_MS 10000
#define TRITON_MIN_STATE_PAYLOAD_LEN 29u

// IMU conversion scales to SI units:
// - Accelerometer: 16384 LSB/g (+/-2g full scale over int16_t) -> m/s^2 (UNI_STANDARD_GRAVITY = 9.80665 m/s^2)
// - Gyroscope: 16.384 LSB/(deg/s) (+/-2000 dps full scale over int16_t) -> rad/s (UNI_DEG_TO_RAD = pi / 180)
#define TRITON_ACCEL_SCALE ((2.0f * UNI_STANDARD_GRAVITY) / 32768.0f)
#define TRITON_GYRO_SCALE ((2000.0f * UNI_DEG_TO_RAD) / 32768.0f)

// Valve proprietary GATT service UUID: "100F6C32-1735-4313-B402-38567131E5F3"
static const uint8_t k_valve_service_uuid[16] = {0x10, 0x0f, 0x6c, 0x32, 0x17, 0x35, 0x43, 0x13,
                                                 0xb4, 0x02, 0x38, 0x56, 0x71, 0x31, 0xe5, 0xf3};

// Enter Valve Mode / lizard-mode suppression payload written to characteristic 0x34
static const uint8_t k_enter_valve_mode[6] = {0xc0, 0x87, 0x03, 0x08, 0x07, 0x00};

/**
 * @brief Steam Triton custom BLE GATT discovery and subscription states.
 */
typedef enum {
    TRITON_GATT_IDLE = 0,       ///< Uninitialized or synthetic test device.
    TRITON_GATT_FIND_SERVICE,   ///< Discovering primary service `100F6C32-1735-4313-B402-38567131E5F3`.
    TRITON_GATT_FIND_CHARS,     ///< Discovering characteristics (`0x7c`, `0x7a`, `0x34`, `0xb5`).
    TRITON_GATT_ENABLE_NOTIFY,  ///< Enabling CCCD notifications on the selected input characteristic.
    TRITON_GATT_READY,          ///< Valve Mode active and streaming input notifications.
    TRITON_GATT_DISCONNECTED,   ///< Disconnected or torn down via `uni_hid_parser_steam_triton_deinit()`.
} triton_gatt_state_t;

/**
 * @brief Per-device Steam Triton state stored in `d->parser_data[]`.
 *
 * Requires 8-byte alignment on `d->parser_data` (`__attribute__((aligned(8)))`) because
 * `gatt_client_notification_t` and `btstack_timer_source_t` embed 64-bit pointers on
 * 64-bit host targets.
 */
typedef struct {
    triton_gatt_state_t state;
    uint16_t notify_value_handle;
    uint16_t notify_end_handle;
    uint16_t report_value_handle;
    uint16_t rumble_value_handle;
    uint8_t stream_report_id;
    bool notify_registered;
    bool conn_timer_active;
    bool rumble_timer_active;
    gatt_client_service_t service;
    gatt_client_characteristic_t notify_chr;
    gatt_client_notification_t notify_registration;
    btstack_timer_source_t conn_timer;
    btstack_timer_source_t rumble_timer;
} steam_triton_instance_t;
_Static_assert(sizeof(steam_triton_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "steam_triton_instance_t too large");

static steam_triton_instance_t* get_triton_instance(struct uni_hid_device_s* d) {
    return (steam_triton_instance_t*)&d->parser_data[0];
}

static const steam_triton_instance_t* get_triton_instance_const(const struct uni_hid_device_s* d) {
    return (const steam_triton_instance_t*)&d->parser_data[0];
}

static const char* triton_state_to_str(triton_gatt_state_t state) {
    switch (state) {
        case TRITON_GATT_IDLE:
            return "idle";
        case TRITON_GATT_FIND_SERVICE:
            return "find_service";
        case TRITON_GATT_FIND_CHARS:
            return "find_chars";
        case TRITON_GATT_ENABLE_NOTIFY:
            return "enable_notify";
        case TRITON_GATT_READY:
            return "ready";
        case TRITON_GATT_DISCONNECTED:
            return "disconnected";
        default:
            return "unknown";
    }
}

static bool uuid_is_valve_family(const uint8_t u[16]) {
    return u[0] == 0x10 && u[1] == 0x0f && u[2] == 0x6c && memcmp(&u[4], &k_valve_service_uuid[4], 12) == 0;
}

bool uni_hid_parser_steam_triton_is_triton_device(uint16_t vid, uint16_t pid) {
    if (vid != UNI_TRITON_VALVE_VID)
        return false;
    switch (pid) {
        case UNI_TRITON_PID_USB:
        case UNI_TRITON_PID_BLE:
        case UNI_TRITON_PID_PUCK:
        case UNI_TRITON_PID_NEREID:
            return true;
        default:
            return false;
    }
}

bool uni_hid_parser_steam_triton_is_device(const struct uni_hid_device_s* d) {
    if (!d)
        return false;
    return uni_hid_parser_steam_triton_is_triton_device(d->vendor_id, d->product_id);
}

static void stop_conn_timer(steam_triton_instance_t* ins) {
    if (ins->conn_timer_active) {
        btstack_run_loop_remove_timer(&ins->conn_timer);
        ins->conn_timer_active = false;
    }
}

static void stop_rumble_timer(steam_triton_instance_t* ins) {
    if (ins->rumble_timer_active) {
        btstack_run_loop_remove_timer(&ins->rumble_timer);
        ins->rumble_timer_active = false;
    }
}

static void conn_timeout_cb(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = (uni_hid_device_t*)btstack_run_loop_get_timer_context(ts);
    if (!d)
        return;
    steam_triton_instance_t* ins = get_triton_instance(d);
    btstack_run_loop_remove_timer(ts);
    ins->conn_timer_active = false;
    if (ins->state == TRITON_GATT_READY || ins->state == TRITON_GATT_DISCONNECTED)
        return;

    loge("Steam Triton: GATT setup timed out in state '%s' (%d), disconnecting\n", triton_state_to_str(ins->state),
         (int)ins->state);
    ins->state = TRITON_GATT_DISCONNECTED;
    uni_hid_device_disconnect(d);
}

static void arm_conn_timer(uni_hid_device_t* d, uint32_t timeout_ms) {
    if (!d || d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;
    steam_triton_instance_t* ins = get_triton_instance(d);
    stop_conn_timer(ins);
    btstack_run_loop_set_timer_context(&ins->conn_timer, d);
    btstack_run_loop_set_timer_handler(&ins->conn_timer, &conn_timeout_cb);
    btstack_run_loop_set_timer(&ins->conn_timer, timeout_ms);
    btstack_run_loop_add_timer(&ins->conn_timer);
    ins->conn_timer_active = true;
}

static void send_valve_report_write(uni_hid_device_t* d, const uint8_t* data, uint16_t len) {
    if (!d || !data || len == 0 || d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;
    steam_triton_instance_t* ins = get_triton_instance(d);
    if (!ins->report_value_handle)
        return;

    uint8_t status = gatt_client_write_value_of_characteristic_without_response(
        d->conn.handle, ins->report_value_handle, len, (uint8_t*)data);
    if (status != ERROR_CODE_SUCCESS) {
        logd("Steam Triton: report write status=0x%02x\n", status);
    }
}

static void send_rumble_now(uni_hid_device_t* d, uint8_t weak_magnitude, uint8_t strong_magnitude) {
    if (!d || d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;
    steam_triton_instance_t* ins = get_triton_instance(d);
    uint16_t handle = ins->rumble_value_handle ? ins->rumble_value_handle : ins->report_value_handle;
    if (!handle)
        return;

    uint16_t left = (uint16_t)strong_magnitude * 257u;
    uint16_t right = (uint16_t)weak_magnitude * 257u;
    uint8_t pkt[11] = {
        0xc0,
        ID_TRITON_OUT_REPORT_HAPTIC_RUMBLE,
        9,
        0,
        0,
        0,
        (uint8_t)(left & 0xffu),
        (uint8_t)(left >> 8),
        0,
        (uint8_t)(right & 0xffu),
        (uint8_t)(right >> 8),
    };

    uint8_t status =
        gatt_client_write_value_of_characteristic_without_response(d->conn.handle, handle, sizeof(pkt), pkt);
    if (status != ERROR_CODE_SUCCESS) {
        logd("Steam Triton: rumble write status=0x%02x\n", status);
    }
}

static void rumble_timer_cb(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = (uni_hid_device_t*)btstack_run_loop_get_timer_context(ts);
    if (!d)
        return;
    steam_triton_instance_t* ins = get_triton_instance(d);
    btstack_run_loop_remove_timer(ts);
    ins->rumble_timer_active = false;
    send_rumble_now(d, 0, 0);
}

void uni_hid_parser_steam_triton_handle_gatt_event(uint8_t packet_type,
                                                   uint16_t channel,
                                                   uint8_t* packet,
                                                   uint16_t size) {
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
        case GATT_EVENT_QUERY_COMPLETE:
            con_handle = gatt_event_query_complete_get_handle(packet);
            break;
        case GATT_EVENT_NOTIFICATION:
            con_handle = gatt_event_notification_get_handle(packet);
            break;
        default:
            return;
    }

    uni_hid_device_t* device = uni_hid_device_get_instance_for_connection_handle(con_handle);
    if (!device)
        return;

    steam_triton_instance_t* ins = get_triton_instance(device);

    if (event == GATT_EVENT_NOTIFICATION) {
        if (ins->state != TRITON_GATT_READY)
            return;
        uint16_t value_len = gatt_event_notification_get_value_length(packet);
        const uint8_t* value = gatt_event_notification_get_value(packet);
        if (!value || value_len == 0 || value_len > 63)
            return;

        // Support both synthetic report-ID-prefixed notifications (46B / 30B / battery 0x43)
        // and raw BLE characteristic notifications (where the leading report ID is implied by
        // the subscribed characteristic 0x7c -> 0x47 or 0x7a -> 0x45).
        bool has_report_id = ((value[0] == ID_TRITON_CONTROLLER_STATE || value[0] == ID_TRITON_CONTROLLER_STATE_BLE ||
                               value[0] == ID_TRITON_CONTROLLER_STATE_TIMESTAMP) &&
                              (value_len == 30 || value_len == 46)) ||
                             (value[0] == ID_TRITON_BATTERY_STATUS && value_len >= 3 && value_len <= 16);

        if (has_report_id) {
            uni_hid_parse_input_report(device, value, value_len);
        } else {
            uint8_t report[64];
            report[0] = ins->stream_report_id ? ins->stream_report_id : ID_TRITON_CONTROLLER_STATE_BLE;
            memcpy(&report[1], value, value_len);
            uni_hid_parse_input_report(device, report, (uint16_t)(value_len + 1u));
        }
        uni_hid_device_process_controller(device);
        return;
    }

    switch (ins->state) {
        case TRITON_GATT_FIND_SERVICE:
            switch (event) {
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    gatt_event_service_query_result_get_service(packet, &ins->service);
                    break;
                case GATT_EVENT_QUERY_COMPLETE: {
                    uint8_t att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS || ins->service.start_group_handle == 0) {
                        loge("Steam Triton: Valve GATT service not found (att=0x%02x)\n", att_status);
                        break;
                    }
                    ins->state = TRITON_GATT_FIND_CHARS;
                    (void)gatt_client_discover_characteristics_for_service(
                        uni_hid_parser_steam_triton_handle_gatt_event, con_handle, &ins->service);
                    break;
                }
                default:
                    break;
            }
            break;

        case TRITON_GATT_FIND_CHARS:
            switch (event) {
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT: {
                    gatt_client_characteristic_t chr;
                    gatt_event_characteristic_query_result_get_characteristic(packet, &chr);
                    if (!uuid_is_valve_family(chr.uuid128))
                        break;

                    uint8_t suffix = chr.uuid128[3];
                    switch (suffix) {
                        case 0x7a:
                            // BLE input stream (report 0x45); prefer 0x7c (report 0x47) if already found.
                            if (ins->stream_report_id != ID_TRITON_CONTROLLER_STATE_TIMESTAMP) {
                                ins->notify_chr = chr;
                                ins->notify_value_handle = chr.value_handle;
                                ins->notify_end_handle = chr.end_handle;
                                ins->stream_report_id = ID_TRITON_CONTROLLER_STATE_BLE;
                            }
                            break;
                        case 0x7c:
                            // Timestamped BLE input stream (report 0x47, preferred over 0x7a).
                            ins->notify_chr = chr;
                            ins->notify_value_handle = chr.value_handle;
                            ins->notify_end_handle = chr.end_handle;
                            ins->stream_report_id = ID_TRITON_CONTROLLER_STATE_TIMESTAMP;
                            break;
                        case 0x34:
                            ins->report_value_handle = chr.value_handle;
                            break;
                        case 0xb5:
                            ins->rumble_value_handle = chr.value_handle;
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case GATT_EVENT_QUERY_COMPLETE: {
                    uint8_t att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS || ins->notify_value_handle == 0) {
                        loge("Steam Triton: input characteristic missing (att=0x%02x)\n", att_status);
                        break;
                    }
                    ins->state = TRITON_GATT_ENABLE_NOTIFY;
                    if (!ins->notify_registered) {
                        gatt_client_listen_for_characteristic_value_updates(
                            &ins->notify_registration, uni_hid_parser_steam_triton_handle_gatt_event, con_handle,
                            &ins->notify_chr);
                        ins->notify_registered = true;
                    }
                    (void)gatt_client_write_client_characteristic_configuration(
                        uni_hid_parser_steam_triton_handle_gatt_event, con_handle, &ins->notify_chr,
                        GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                    break;
                }
                default:
                    break;
            }
            break;

        case TRITON_GATT_ENABLE_NOTIFY:
            if (event == GATT_EVENT_QUERY_COMPLETE) {
                uint8_t att_status = gatt_event_query_complete_get_att_status(packet);
                if (att_status != ATT_ERROR_SUCCESS) {
                    loge("Steam Triton: CCCD enable failed (att=0x%02x)\n", att_status);
                    break;
                }
                if (ins->report_value_handle != 0) {
                    send_valve_report_write(device, k_enter_valve_mode, sizeof(k_enter_valve_mode));
                }
                stop_conn_timer(ins);
                ins->state = TRITON_GATT_READY;
                if (uni_bt_conn_get_state(&device->conn) != UNI_BT_CONN_STATE_DEVICE_READY) {
                    uni_bt_conn_set_state(&device->conn, UNI_BT_CONN_STATE_DEVICE_PENDING_READY);
                    uni_hid_device_set_ready_complete(device);
                }
            }
            break;

        default:
            break;
    }
}

void uni_hid_parser_steam_triton_deinit(struct uni_hid_device_s* d) {
    if (!d)
        return;
    steam_triton_instance_t* ins = get_triton_instance(d);
    stop_conn_timer(ins);
    stop_rumble_timer(ins);
    if (ins->notify_registered) {
        gatt_client_stop_listening_for_characteristic_value_updates(&ins->notify_registration);
        ins->notify_registered = false;
    }
    ins->state = TRITON_GATT_DISCONNECTED;
}

void uni_hid_parser_steam_triton_setup(struct uni_hid_device_s* d) {
    if (!d)
        return;

    steam_triton_instance_t* ins = get_triton_instance(d);
    uni_hid_parser_steam_triton_deinit(d);
    memset(ins, 0, sizeof(*ins));

    d->controller.klass = UNI_CONTROLLER_CLASS_GAMEPAD;

    // Guard synthetic/stack devices (`d->conn.handle == UNI_BT_CONN_HANDLE_INVALID`) so
    // `setup_synthetic_device()` in unit tests does not link stack timers into BTstack.
    if (d->conn.handle == UNI_BT_CONN_HANDLE_INVALID)
        return;

    ins->state = TRITON_GATT_FIND_SERVICE;
    arm_conn_timer(d, TRITON_CONN_TIMEOUT_MS);
    uint8_t status = gatt_client_discover_primary_services_by_uuid128(uni_hid_parser_steam_triton_handle_gatt_event,
                                                                      d->conn.handle, k_valve_service_uuid);
    if (status != ERROR_CODE_SUCCESS) {
        logd("Steam Triton: service discovery status=0x%02x\n", status);
    }
}

void uni_hid_parser_steam_triton_init_report(struct uni_hid_device_s* d) {
    ARG_UNUSED(d);
}

static int32_t triton_scale_stick(int16_t raw, bool invert) {
    // Widen to int32_t before negating so raw == INT16_MIN (-32768) does not overflow
    // signed 16-bit arithmetic, and clamp to Bluepad32's [-512, 511] range.
    int32_t val = (int32_t)raw >> 6;
    if (invert)
        val = -val;
    if (val > 511)
        val = 511;
    if (val < -512)
        val = -512;
    return val;
}

static void parse_state_payload(struct uni_hid_device_s* d, uint8_t report_id, const uint8_t* p, uint16_t payload_len) {
    // Minimum state report is 30 bytes (1B report ID + 29B payload: seq + buttons + triggers + sticks + pads).
    if (payload_len < TRITON_MIN_STATE_PAYLOAD_LEN)
        return;

    uni_controller_t* ctl = &d->controller;
    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
    ctl->gamepad.dpad = 0;
    ctl->gamepad.buttons = 0;
    ctl->gamepad.misc_buttons = 0;

    uint32_t buttons = little_endian_read_32(p, 1);

    // D-Pad mapping
    if (buttons & TRITON_BTN_DPAD_UP)
        ctl->gamepad.dpad |= DPAD_UP;
    if (buttons & TRITON_BTN_DPAD_DOWN)
        ctl->gamepad.dpad |= DPAD_DOWN;
    if (buttons & TRITON_BTN_DPAD_LEFT)
        ctl->gamepad.dpad |= DPAD_LEFT;
    if (buttons & TRITON_BTN_DPAD_RIGHT)
        ctl->gamepad.dpad |= DPAD_RIGHT;

    // Face, shoulder, trigger, and thumb buttons
    if (buttons & TRITON_BTN_A)
        ctl->gamepad.buttons |= BUTTON_A;
    if (buttons & TRITON_BTN_B)
        ctl->gamepad.buttons |= BUTTON_B;
    if (buttons & TRITON_BTN_X)
        ctl->gamepad.buttons |= BUTTON_X;
    if (buttons & TRITON_BTN_Y)
        ctl->gamepad.buttons |= BUTTON_Y;
    if (buttons & TRITON_BTN_LB)
        ctl->gamepad.buttons |= BUTTON_SHOULDER_L;
    if (buttons & TRITON_BTN_RB)
        ctl->gamepad.buttons |= BUTTON_SHOULDER_R;
    if (buttons & TRITON_BTN_LT_FULL)
        ctl->gamepad.buttons |= BUTTON_TRIGGER_L;
    if (buttons & TRITON_BTN_RT_FULL)
        ctl->gamepad.buttons |= BUTTON_TRIGGER_R;
    if (buttons & (TRITON_BTN_L3 | TRITON_BTN_LPAD_CLICK))
        ctl->gamepad.buttons |= BUTTON_THUMB_L;
    if (buttons & (TRITON_BTN_R3 | TRITON_BTN_RPAD_CLICK))
        ctl->gamepad.buttons |= BUTTON_THUMB_R;

    // Misc buttons: VIEW -> SELECT, MENU -> START, STEAM -> SYSTEM, QAM -> CAPTURE
    if (buttons & TRITON_BTN_VIEW)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;
    if (buttons & TRITON_BTN_MENU)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_START;
    if (buttons & TRITON_BTN_STEAM)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;
    if (buttons & TRITON_BTN_QAM)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_CAPTURE;

    // 16-bit triggers (0..32767 -> 0..1023)
    int16_t lt = (int16_t)little_endian_read_16(p, 5);
    int16_t rt = (int16_t)little_endian_read_16(p, 7);
    if (lt < 0)
        lt = 0;
    if (rt < 0)
        rt = 0;
    ctl->gamepad.brake = (int32_t)(lt >> 5);
    ctl->gamepad.throttle = (int32_t)(rt >> 5);
    if (ctl->gamepad.brake > 0)
        ctl->gamepad.buttons |= BUTTON_TRIGGER_L;
    if (ctl->gamepad.throttle > 0)
        ctl->gamepad.buttons |= BUTTON_TRIGGER_R;

    // 16-bit signed thumbsticks (-32768..32767 -> -512..511, Y inverted)
    int16_t lx = (int16_t)little_endian_read_16(p, 9);
    int16_t ly = (int16_t)little_endian_read_16(p, 11);
    int16_t rx = (int16_t)little_endian_read_16(p, 13);
    int16_t ry = (int16_t)little_endian_read_16(p, 15);
    ctl->gamepad.axis_x = triton_scale_stick(lx, false);
    ctl->gamepad.axis_y = triton_scale_stick(ly, true);
    ctl->gamepad.axis_rx = triton_scale_stick(rx, false);
    ctl->gamepad.axis_ry = triton_scale_stick(ry, true);

    // 6-axis IMU block at p + 33..44 (requires payload_len >= 45, i.e., len >= 46):
    // - Reports 0x42 / 0x45: imu_off = 29, imu_ts_size = 4 -> imu at p + 33
    // - Report 0x47: imu_off = 31, imu_ts_size = 2 -> imu at p + 33
    uint16_t imu_off = (report_id == ID_TRITON_CONTROLLER_STATE_TIMESTAMP) ? 31u : 29u;
    uint16_t imu_ts_size = (report_id == ID_TRITON_CONTROLLER_STATE_TIMESTAMP) ? 2u : 4u;
    if (payload_len >= (uint16_t)(imu_off + imu_ts_size + 12u)) {
        const uint8_t* imu = &p[imu_off + imu_ts_size];
        int16_t ax = (int16_t)little_endian_read_16(imu, 0);
        int16_t ay = (int16_t)little_endian_read_16(imu, 2);
        int16_t az = (int16_t)little_endian_read_16(imu, 4);
        int16_t gx = (int16_t)little_endian_read_16(imu, 6);
        int16_t gy = (int16_t)little_endian_read_16(imu, 8);
        int16_t gz = (int16_t)little_endian_read_16(imu, 10);

        // Native SDL Triton sensor frame (+X right, +Y forward, +Z up) ->
        // Bluepad32 canonical right-handed Y-up frame ([0]=+X right, [1]=+Y up, [2]=+Z back).
        ctl->gamepad.accel[0] = (float)ax * TRITON_ACCEL_SCALE;
        ctl->gamepad.accel[1] = (float)az * TRITON_ACCEL_SCALE;
        ctl->gamepad.accel[2] = -(float)ay * TRITON_ACCEL_SCALE;
        ctl->gamepad.gyro[0] = (float)gx * TRITON_GYRO_SCALE;
        ctl->gamepad.gyro[1] = (float)gz * TRITON_GYRO_SCALE;
        ctl->gamepad.gyro[2] = -(float)gy * TRITON_GYRO_SCALE;
    }
}

void uni_hid_parser_steam_triton_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    if (!d || !report || len < 2)
        return;

    const uint8_t id = report[0];
    switch (id) {
        case ID_TRITON_CONTROLLER_STATE:
        case ID_TRITON_CONTROLLER_STATE_BLE:
        case ID_TRITON_CONTROLLER_STATE_TIMESTAMP:
            parse_state_payload(d, id, &report[1], (uint16_t)(len - 1u));
            break;
        case ID_TRITON_BATTERY_STATUS:
            if (len >= 3) {
                uint8_t bat_pct = report[2];
                if (bat_pct > 100)
                    bat_pct = 100;
                d->controller.battery = (uint8_t)(((uint32_t)bat_pct * 255u) / 100u);
            }
            break;
        default:
            break;
    }
}

void uni_hid_parser_steam_triton_play_dual_rumble(struct uni_hid_device_s* d,
                                                  uint16_t start_delay_ms,
                                                  uint16_t duration_ms,
                                                  uint8_t weak_magnitude,
                                                  uint8_t strong_magnitude) {
    if (!d)
        return;
    steam_triton_instance_t* ins = get_triton_instance(d);
    stop_rumble_timer(ins);

    if ((weak_magnitude == 0 && strong_magnitude == 0) || duration_ms == 0) {
        send_rumble_now(d, 0, 0);
        return;
    }

    send_rumble_now(d, weak_magnitude, strong_magnitude);
    uint32_t total_ms = (uint32_t)start_delay_ms + (uint32_t)duration_ms;
    btstack_run_loop_set_timer_context(&ins->rumble_timer, d);
    btstack_run_loop_set_timer_handler(&ins->rumble_timer, &rumble_timer_cb);
    btstack_run_loop_set_timer(&ins->rumble_timer, total_ms);
    btstack_run_loop_add_timer(&ins->rumble_timer);
    ins->rumble_timer_active = true;
}

int uni_hid_parser_steam_triton_device_extra_info(const struct uni_hid_device_s* d, char* buf, size_t len) {
    if (!d || !buf || len == 0)
        return -1;
    const steam_triton_instance_t* ins = get_triton_instance_const(d);
    return snprintf(buf, len, "pid=0x%04x, state=%s, stream=0x%02x, notify=0x%04x, report=0x%04x, rumble=0x%04x",
                    d->product_id, triton_state_to_str(ins->state), ins->stream_report_id, ins->notify_value_handle,
                    ins->report_value_handle, ins->rumble_value_handle);
}
