// SPDX-License-Identifier: Apache-2.0
// Steam Controller 2026 (Triton) over Bluetooth LE.
//
// Triton does NOT use HOGP the way Xbox/DualSense do. SDL/Android talks to
// Valve's proprietary GATT service (same UUID as classic Steam Controller).
// Going through hids_host_connect hangs forever on Pico W after DIS.
//
// Protocol reference: SDL HIDDeviceBLESteamController + hidapi_steam_triton.
// Pairing: hold RB + B + Steam on the controller.

#include "parser/uni_hid_parser_steam_triton.h"

#include <string.h>

#include <btstack.h>
#include <btstack_run_loop.h>

#include "bt/uni_bt_conn.h"
#include "controller/uni_controller.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

#define VALVE_VID 0x28de
#define TRITON_PID_USB 0x1302
#define TRITON_PID_BLE 0x1303
#define TRITON_PID_PUCK 0x1304
#define TRITON_PID_NEREID 0x1305

#define ID_TRITON_CONTROLLER_STATE 0x42
#define ID_TRITON_BATTERY_STATUS 0x43
#define ID_TRITON_CONTROLLER_STATE_BLE 0x45
#define ID_TRITON_CONTROLLER_STATE_TIMESTAMP 0x47

#define ID_OUT_REPORT_HAPTIC_RUMBLE 0x80
#define TRITON_LIZARD_REFRESH_MS 3000
#define TRITON_RUMBLE_RESEND_MS 40

/* button bits — SDL TritonButtons */
#define TRITON_BTN_A 0x00000001u
#define TRITON_BTN_B 0x00000002u
#define TRITON_BTN_X 0x00000004u
#define TRITON_BTN_Y 0x00000008u
#define TRITON_BTN_QAM 0x00000010u
#define TRITON_BTN_R3 0x00000020u
#define TRITON_BTN_VIEW 0x00000040u
#define TRITON_BTN_R4 0x00000080u
#define TRITON_BTN_R5 0x00000100u
#define TRITON_BTN_RB 0x00000200u
#define TRITON_BTN_DPAD_DOWN 0x00000400u
#define TRITON_BTN_DPAD_RIGHT 0x00000800u
#define TRITON_BTN_DPAD_LEFT 0x00001000u
#define TRITON_BTN_DPAD_UP 0x00002000u
#define TRITON_BTN_MENU 0x00004000u
#define TRITON_BTN_L3 0x00008000u
#define TRITON_BTN_STEAM 0x00010000u
#define TRITON_BTN_L4 0x00020000u
#define TRITON_BTN_L5 0x00040000u
#define TRITON_BTN_LB 0x00080000u
#define TRITON_BTN_RPAD_CLICK 0x00400000u
#define TRITON_BTN_LPAD_CLICK 0x04000000u

/* "100F6C32-1735-4313-B402-38567131E5F3" */
static const uint8_t k_valve_service_uuid[16] = {0x10, 0x0f, 0x6c, 0x32, 0x17, 0x35, 0x43, 0x13,
                                                 0xb4, 0x02, 0x38, 0x56, 0x71, 0x31, 0xe5, 0xf3};

/* SDL enterValveMode / lizard-off style write to report characteristic. */
static const uint8_t k_enter_valve_mode[] = {0xc0, 0x87, 0x03, 0x08, 0x07, 0x00};

typedef enum {
    TRITON_Q_SERVICE = 0,
    TRITON_Q_CHARS,
    TRITON_Q_CCCD,
    TRITON_Q_VALVE_MODE,
    TRITON_Q_READY,
} triton_query_state_t;

typedef struct {
    gatt_client_service_t service;
    gatt_client_characteristic_t input_chr;
    gatt_client_notification_t notification;
    btstack_timer_source_t lizard_timer;
    btstack_timer_source_t rumble_timer;
    uint16_t report_value_handle;
    uint16_t rumble_value_handle;
    uint8_t query_state;
    uint8_t report_id;
    bool lizard_timer_active;
    bool rumble_timer_active;
    uint8_t rumble_weak;
    uint8_t rumble_strong;
    uint16_t rumble_ms_left;
} steam_triton_instance_t;

_Static_assert(sizeof(steam_triton_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "Triton instance too big");

static steam_triton_instance_t* get_ins(uni_hid_device_t* d) {
    return (steam_triton_instance_t*)&d->parser_data[0];
}

static int16_t read_i16_le(const uint8_t* p) {
    return (int16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32_le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool uuid_is_valve_family(const uint8_t u[16]) {
    static const uint8_t tail[12] = {0x17, 0x35, 0x43, 0x13, 0xb4, 0x02, 0x38, 0x56, 0x71, 0x31, 0xe5, 0xf3};
    return u[0] == 0x10 && u[1] == 0x0f && u[2] == 0x6c && memcmp(&u[4], tail, 12) == 0;
}

bool uni_hid_parser_steam_triton_is_device(const uni_hid_device_t* d) {
    if (!d)
        return false;
    if (d->vendor_id != VALVE_VID)
        return false;
    switch (d->product_id) {
        case TRITON_PID_USB:
        case TRITON_PID_BLE:
        case TRITON_PID_PUCK:
        case TRITON_PID_NEREID:
            return true;
        default:
            return false;
    }
}

static void parse_state_payload(uni_hid_device_t* d, uint8_t report_id, const uint8_t* p, uint16_t payload_len) {
    /* Need through right stick (seq + buttons + 2 trig + 4 stick axes = 17). */
    if (payload_len < 17)
        return;

    uni_controller_t* ctl = &d->controller;
    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
    ctl->gamepad.dpad = 0;
    ctl->gamepad.buttons = 0;
    ctl->gamepad.misc_buttons = 0;
    ctl->gamepad.accel[0] = ctl->gamepad.accel[1] = ctl->gamepad.accel[2] = 0;
    ctl->gamepad.gyro[0] = ctl->gamepad.gyro[1] = ctl->gamepad.gyro[2] = 0;

    uint32_t buttons = read_u32_le(&p[1]);

    if (buttons & TRITON_BTN_DPAD_UP)
        ctl->gamepad.dpad |= DPAD_UP;
    if (buttons & TRITON_BTN_DPAD_DOWN)
        ctl->gamepad.dpad |= DPAD_DOWN;
    if (buttons & TRITON_BTN_DPAD_LEFT)
        ctl->gamepad.dpad |= DPAD_LEFT;
    if (buttons & TRITON_BTN_DPAD_RIGHT)
        ctl->gamepad.dpad |= DPAD_RIGHT;

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
    if (buttons & TRITON_BTN_L3)
        ctl->gamepad.buttons |= BUTTON_THUMB_L;
    if (buttons & TRITON_BTN_R3)
        ctl->gamepad.buttons |= BUTTON_THUMB_R;
    if (buttons & TRITON_BTN_LPAD_CLICK)
        ctl->gamepad.buttons |= BUTTON_THUMB_L;
    if (buttons & TRITON_BTN_RPAD_CLICK)
        ctl->gamepad.buttons |= BUTTON_THUMB_R;
    /* L4/L5/R4/R5 grips intentionally unmapped (no host paddle channel). */

    /* SDL maps Triton VIEW→Start and MENU→Back (labels are not Xbox-style). */
    if (buttons & TRITON_BTN_VIEW)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_START;
    if (buttons & TRITON_BTN_MENU)
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SELECT;
    if (buttons & (TRITON_BTN_STEAM | TRITON_BTN_QAM))
        ctl->gamepad.misc_buttons |= MISC_BUTTON_SYSTEM;

    int16_t lt = read_i16_le(&p[5]);
    int16_t rt = read_i16_le(&p[7]);
    if (lt < 0)
        lt = 0;
    if (rt < 0)
        rt = 0;
    ctl->gamepad.brake = (int32_t)lt * AXIS_NORMALIZE_RANGE / 32767;
    ctl->gamepad.throttle = (int32_t)rt * AXIS_NORMALIZE_RANGE / 32767;

    int16_t lx = read_i16_le(&p[9]);
    int16_t ly = read_i16_le(&p[11]);
    int16_t rx = read_i16_le(&p[13]);
    int16_t ry = read_i16_le(&p[15]);
    /* SDL inverted stick Y; Bluepad ±512 range. */
    ctl->gamepad.axis_x = lx >> 6;
    ctl->gamepad.axis_y = (-ly) >> 6;
    ctl->gamepad.axis_rx = rx >> 6;
    ctl->gamepad.axis_ry = (-ry) >> 6;

    /*
     * IMU after pads (SDL TritonMTUNoQuat / TritonMTUNoQuat32TS).
     * 0x42/0x45: pads @17 (12) then IMU ts u32 + accel/gyro.
     * 0x47: trackpad ts u16 @17, pads @19 (12), IMU ts u16 + accel/gyro @31.
     */
    uint16_t imu_off;
    uint16_t imu_ts_size;
    if (report_id == ID_TRITON_CONTROLLER_STATE_TIMESTAMP) {
        imu_off = 31;
        imu_ts_size = 2;
    } else {
        imu_off = 29;
        imu_ts_size = 4;
    }
    if (payload_len >= (uint16_t)(imu_off + imu_ts_size + 12)) {
        const uint8_t* imu = &p[imu_off + imu_ts_size];
        int16_t ax = read_i16_le(&imu[0]);
        int16_t ay = read_i16_le(&imu[2]);
        int16_t az = read_i16_le(&imu[4]);
        int16_t gx = read_i16_le(&imu[6]);
        int16_t gy = read_i16_le(&imu[8]);
        int16_t gz = read_i16_le(&imu[10]);
        /* Same axis remapping as SDL_hidapi_steam_triton. */
        ctl->gamepad.accel[0] = ax;
        ctl->gamepad.accel[1] = az;
        ctl->gamepad.accel[2] = -ay;
        ctl->gamepad.gyro[0] = gx;
        ctl->gamepad.gyro[1] = gz;
        ctl->gamepad.gyro[2] = -gy;
    }
}

static void send_valve_report_write(uni_hid_device_t* d, const uint8_t* data, uint16_t len) {
    steam_triton_instance_t* ins = get_ins(d);
    if (!ins->report_value_handle)
        return;
    gatt_client_write_value_of_characteristic(NULL, d->conn.handle, ins->report_value_handle, len, (uint8_t*)data);
}

static void send_lizard_off(uni_hid_device_t* d) {
    send_valve_report_write(d, k_enter_valve_mode, sizeof(k_enter_valve_mode));
}

static void send_rumble_now(uni_hid_device_t* d, uint8_t weak, uint8_t strong) {
    steam_triton_instance_t* ins = get_ins(d);
    if (!ins->rumble_value_handle && !ins->report_value_handle)
        return;

    uint8_t payload[11] = {0};
    payload[0] = 0xc0;
    payload[1] = ID_OUT_REPORT_HAPTIC_RUMBLE;
    payload[2] = 9;
    payload[3] = 0;
    payload[4] = 0;
    payload[5] = 0;
    uint16_t left = (uint16_t)strong * 257u;
    uint16_t right = (uint16_t)weak * 257u;
    payload[6] = (uint8_t)(left & 0xff);
    payload[7] = (uint8_t)(left >> 8);
    payload[8] = 0;
    payload[9] = (uint8_t)(right & 0xff);
    payload[10] = (uint8_t)(right >> 8);

    uint16_t handle = ins->rumble_value_handle ? ins->rumble_value_handle : ins->report_value_handle;
    gatt_client_write_value_of_characteristic(NULL, d->conn.handle, handle, sizeof(payload), payload);
}

static void lizard_timer_cb(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = (uni_hid_device_t*)ts->context;
    if (!d)
        return;
    send_lizard_off(d);
    btstack_run_loop_set_timer(ts, TRITON_LIZARD_REFRESH_MS);
    btstack_run_loop_add_timer(ts);
}

static void rumble_timer_cb(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = (uni_hid_device_t*)ts->context;
    if (!d)
        return;
    steam_triton_instance_t* ins = get_ins(d);
    if (ins->rumble_ms_left == 0) {
        send_rumble_now(d, 0, 0);
        ins->rumble_timer_active = false;
        return;
    }
    if (ins->rumble_ms_left > TRITON_RUMBLE_RESEND_MS)
        ins->rumble_ms_left = (uint16_t)(ins->rumble_ms_left - TRITON_RUMBLE_RESEND_MS);
    else
        ins->rumble_ms_left = 0;

    send_rumble_now(d, ins->rumble_weak, ins->rumble_strong);
    btstack_run_loop_set_timer(ts, TRITON_RUMBLE_RESEND_MS);
    btstack_run_loop_add_timer(ts);
}

static void triton_on_ready(uni_hid_device_t* d) {
    steam_triton_instance_t* ins = get_ins(d);
    ins->query_state = TRITON_Q_READY;
    logi("Steam Triton: Valve GATT ready (input report 0x%02x)\n", ins->report_id);

    if (d->conn.handle != HCI_CON_HANDLE_INVALID && d->conn.handle != UNI_BT_CONN_HANDLE_INVALID) {
        int rc = gap_request_connection_parameter_update(d->conn.handle, 6, 9, 0, 600);
        if (rc)
            logi("Steam Triton: conn-param update status=%d\n", rc);
    }

    send_lizard_off(d);
    ins->lizard_timer.process = &lizard_timer_cb;
    ins->lizard_timer.context = d;
    ins->lizard_timer_active = true;
    btstack_run_loop_set_timer(&ins->lizard_timer, TRITON_LIZARD_REFRESH_MS);
    btstack_run_loop_add_timer(&ins->lizard_timer);

    uni_hid_device_set_ready_complete(d);
}

static void uni_triton_gatt_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    ARG_UNUSED(channel);
    ARG_UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET)
        return;

    uint8_t event = hci_event_packet_get_type(packet);
    hci_con_handle_t handle = HCI_CON_HANDLE_INVALID;

    switch (event) {
        case GATT_EVENT_SERVICE_QUERY_RESULT:
            handle = gatt_event_service_query_result_get_handle(packet);
            break;
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
            handle = gatt_event_characteristic_query_result_get_handle(packet);
            break;
        case GATT_EVENT_QUERY_COMPLETE:
            handle = gatt_event_query_complete_get_handle(packet);
            break;
        case GATT_EVENT_NOTIFICATION:
            handle = gatt_event_notification_get_handle(packet);
            break;
        default:
            return;
    }

    uni_hid_device_t* device = uni_hid_device_get_instance_for_connection_handle(handle);
    if (!device) {
        loge("Steam Triton: no device for handle %#x (event=%#x)\n", handle, event);
        return;
    }
    steam_triton_instance_t* ins = get_ins(device);
    const hci_con_handle_t con = device->conn.handle;

    if (event == GATT_EVENT_NOTIFICATION) {
        uint16_t value_len = gatt_event_notification_get_value_length(packet);
        const uint8_t* value = gatt_event_notification_get_value(packet);
        if (!value || value_len == 0 || value_len > 62)
            return;
        uint8_t report[64];
        report[0] = ins->report_id ? ins->report_id : ID_TRITON_CONTROLLER_STATE_BLE;
        memcpy(&report[1], value, value_len);
        uni_hid_parser_steam_triton_parse_input_report(device, report, (uint16_t)(value_len + 1u));
        uni_hid_device_process_controller(device);
        return;
    }

    switch (ins->query_state) {
        case TRITON_Q_SERVICE:
            switch (event) {
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    gatt_event_service_query_result_get_service(packet, &ins->service);
                    break;
                case GATT_EVENT_QUERY_COMPLETE: {
                    uint8_t st = gatt_event_query_complete_get_att_status(packet);
                    if (st != ATT_ERROR_SUCCESS || ins->service.start_group_handle == 0) {
                        loge("Steam Triton: Valve service not found (att=%#x)\n", st);
                        break;
                    }
                    logi("Steam Triton: Valve service handles %#x..%#x\n", ins->service.start_group_handle,
                         ins->service.end_group_handle);
                    ins->query_state = TRITON_Q_CHARS;
                    gatt_client_discover_characteristics_for_service(uni_triton_gatt_handler, con, &ins->service);
                    break;
                }
                default:
                    break;
            }
            break;

        case TRITON_Q_CHARS:
            switch (event) {
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT: {
                    gatt_client_characteristic_t chr;
                    gatt_event_characteristic_query_result_get_characteristic(packet, &chr);
                    if (!uuid_is_valve_family(chr.uuid128))
                        break;
                    uint8_t suffix = chr.uuid128[3];
                    if (suffix == 0x7a) {
                        /* Prefer 0x47 if already seen; otherwise take 0x45. */
                        if (ins->report_id != ID_TRITON_CONTROLLER_STATE_TIMESTAMP) {
                            ins->input_chr = chr;
                            ins->report_id = ID_TRITON_CONTROLLER_STATE_BLE;
                        }
                    } else if (suffix == 0x7c) {
                        ins->input_chr = chr;
                        ins->report_id = ID_TRITON_CONTROLLER_STATE_TIMESTAMP;
                    } else if (suffix == 0x34) {
                        ins->report_value_handle = chr.value_handle;
                    } else if (suffix == 0xb5) {
                        /* reportId 0x80 = suffix 0xB5 (SDL: id = uuid_byte - 0x35) */
                        ins->rumble_value_handle = chr.value_handle;
                    }
                    break;
                }
                case GATT_EVENT_QUERY_COMPLETE: {
                    uint8_t st = gatt_event_query_complete_get_att_status(packet);
                    if (st != ATT_ERROR_SUCCESS || ins->input_chr.value_handle == 0) {
                        loge("Steam Triton: input characteristic missing (att=%#x)\n", st);
                        break;
                    }
                    logi("Steam Triton: input handle %#x report=0x%02x report_chr=%#x rumble=%#x\n",
                         ins->input_chr.value_handle, ins->report_id, ins->report_value_handle,
                         ins->rumble_value_handle);
                    ins->query_state = TRITON_Q_CCCD;
                    gatt_client_listen_for_characteristic_value_updates(&ins->notification, uni_triton_gatt_handler, con,
                                                                       &ins->input_chr);
                    gatt_client_write_client_characteristic_configuration(
                        uni_triton_gatt_handler, con, &ins->input_chr,
                        GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
                    break;
                }
                default:
                    break;
            }
            break;

        case TRITON_Q_CCCD:
            if (event == GATT_EVENT_QUERY_COMPLETE) {
                uint8_t st = gatt_event_query_complete_get_att_status(packet);
                if (st != ATT_ERROR_SUCCESS) {
                    loge("Steam Triton: CCCD enable failed %#x\n", st);
                    break;
                }
                if (ins->report_value_handle) {
                    ins->query_state = TRITON_Q_VALVE_MODE;
                    gatt_client_write_value_of_characteristic(uni_triton_gatt_handler, con, ins->report_value_handle,
                                                             sizeof(k_enter_valve_mode), (uint8_t*)k_enter_valve_mode);
                } else {
                    triton_on_ready(device);
                }
            }
            break;

        case TRITON_Q_VALVE_MODE:
            if (event == GATT_EVENT_QUERY_COMPLETE) {
                /* SDL ignores write failure for Triton; still mark ready. */
                triton_on_ready(device);
            }
            break;

        default:
            break;
    }
}

void uni_hid_parser_steam_triton_teardown(uni_hid_device_t* d) {
    if (!d)
        return;
    steam_triton_instance_t* ins = get_ins(d);
    if (ins->lizard_timer_active) {
        btstack_run_loop_remove_timer(&ins->lizard_timer);
        ins->lizard_timer_active = false;
    }
    if (ins->rumble_timer_active) {
        btstack_run_loop_remove_timer(&ins->rumble_timer);
        ins->rumble_timer_active = false;
    }
    if (ins->query_state >= TRITON_Q_CCCD)
        gatt_client_stop_listening_for_characteristic_value_updates(&ins->notification);
}

void uni_hid_parser_steam_triton_setup(uni_hid_device_t* d) {
    steam_triton_instance_t* ins = get_ins(d);
    uni_hid_parser_steam_triton_teardown(d);
    memset(ins, 0, sizeof(*ins));

    d->controller.klass = UNI_CONTROLLER_CLASS_GAMEPAD;
    logi("Steam Triton: Valve GATT setup pid=0x%04x (skip HOGP)\n", d->product_id);

    ins->query_state = TRITON_Q_SERVICE;
    uint8_t st = gatt_client_discover_primary_services_by_uuid128(uni_triton_gatt_handler, d->conn.handle,
                                                                  k_valve_service_uuid);
    if (st != ERROR_CODE_SUCCESS)
        loge("Steam Triton: service discover status=%#x\n", st);
}

void uni_hid_parser_steam_triton_init_report(uni_hid_device_t* d) {
    ARG_UNUSED(d);
}

void uni_hid_parser_steam_triton_parse_input_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
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
            if (len >= 3)
                d->controller.battery = report[2];
            break;
        default:
            break;
    }
}

void uni_hid_parser_steam_triton_play_dual_rumble(uni_hid_device_t* d,
                                                  uint16_t start_delay_ms,
                                                  uint16_t duration_ms,
                                                  uint8_t weak_magnitude,
                                                  uint8_t strong_magnitude) {
    ARG_UNUSED(start_delay_ms);
    if (!d)
        return;
    steam_triton_instance_t* ins = get_ins(d);

    if (ins->rumble_timer_active) {
        btstack_run_loop_remove_timer(&ins->rumble_timer);
        ins->rumble_timer_active = false;
    }

    ins->rumble_weak = weak_magnitude;
    ins->rumble_strong = strong_magnitude;
    ins->rumble_ms_left = duration_ms;

    if (weak_magnitude == 0 && strong_magnitude == 0) {
        send_rumble_now(d, 0, 0);
        return;
    }

    send_rumble_now(d, weak_magnitude, strong_magnitude);
    ins->rumble_timer.process = &rumble_timer_cb;
    ins->rumble_timer.context = d;
    ins->rumble_timer_active = true;
    btstack_run_loop_set_timer(&ins->rumble_timer, TRITON_RUMBLE_RESEND_MS);
    btstack_run_loop_add_timer(&ins->rumble_timer);
}
