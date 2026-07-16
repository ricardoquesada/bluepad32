// SPDX-License-Identifier: Apache-2.0
// Steam Controller 2026 (Triton) over Bluetooth LE.
//
// Protocol reference: SDL3 hidapi_steam_triton + controller_structs.h,
// and community notes (pattontim/sc2-research). BLE state report ID 0x45.
// Pairing: hold RB + B + Steam on the controller.

#include "parser/uni_hid_parser_steam_triton.h"

#include <string.h>

#include <btstack.h>
#include <btstack_run_loop.h>
#include <ble/gatt-service/hids_client.h>

#include "controller/uni_controller.h"
#include "bt/uni_bt_conn.h"
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
#define ID_SET_SETTINGS_VALUES 0x87
#define SETTING_LIZARD_MODE 9
#define LIZARD_MODE_OFF 0
#define TRITON_FEATURE_REPORT_ID 1
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

typedef struct {
    btstack_timer_source_t lizard_timer;
    btstack_timer_source_t rumble_timer;
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

static void send_lizard_off(uni_hid_device_t* d) {
    if (!d || d->hids_cid == 0)
        return;
    /* Feature report ID 1: SET_SETTINGS_VALUES → LIZARD_MODE = OFF (SDL3 pattern). */
    uint8_t payload[8] = {0};
    payload[0] = ID_SET_SETTINGS_VALUES;
    payload[1] = 3; /* sizeof(ControllerSetting) */
    payload[2] = SETTING_LIZARD_MODE;
    payload[3] = LIZARD_MODE_OFF;
    payload[4] = 0;
    uint8_t st = hids_client_send_write_report(d->hids_cid, TRITON_FEATURE_REPORT_ID, HID_REPORT_TYPE_FEATURE,
                                               payload, sizeof(payload));
    if (st)
        logd("Steam Triton: lizard-off write status=%#x\n", st);
}

static void send_rumble_now(uni_hid_device_t* d, uint8_t weak, uint8_t strong) {
    if (!d || d->hids_cid == 0)
        return;
    /* Output report 0x80 MsgHapticRumble without report ID (HIDS supplies it). */
    uint8_t payload[9] = {0};
    payload[0] = 0; /* type */
    payload[1] = 0; /* intensity lo */
    payload[2] = 0; /* intensity hi */
    uint16_t left = (uint16_t)strong * 257u;
    uint16_t right = (uint16_t)weak * 257u;
    payload[3] = (uint8_t)(left & 0xff);
    payload[4] = (uint8_t)(left >> 8);
    payload[5] = 0; /* left gain */
    payload[6] = (uint8_t)(right & 0xff);
    payload[7] = (uint8_t)(right >> 8);
    payload[8] = 0; /* right gain */
    uint8_t st = hids_client_send_write_report(d->hids_cid, ID_OUT_REPORT_HAPTIC_RUMBLE, HID_REPORT_TYPE_OUTPUT,
                                               payload, sizeof(payload));
    if (st)
        logd("Steam Triton: rumble write status=%#x\n", st);
}

static void lizard_timer_cb(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = (uni_hid_device_t*)ts->context;
    if (!d)
        return;
    steam_triton_instance_t* ins = get_ins(d);
    send_lizard_off(d);
    btstack_run_loop_set_timer(ts, TRITON_LIZARD_REFRESH_MS);
    btstack_run_loop_add_timer(ts);
    (void)ins;
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

void uni_hid_parser_steam_triton_teardown(uni_hid_device_t* d) {
    if (!d || !uni_hid_parser_steam_triton_is_device(d))
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
}

void uni_hid_parser_steam_triton_setup(uni_hid_device_t* d) {
    steam_triton_instance_t* ins = get_ins(d);
    uni_hid_parser_steam_triton_teardown(d);
    memset(ins, 0, sizeof(*ins));

    d->controller.klass = UNI_CONTROLLER_CLASS_GAMEPAD;
    logi("Steam Triton: setup pid=0x%04x hids_cid=%u\n", d->product_id, d->hids_cid);

    /* Default BLE intervals are often ~30–50 ms; request ~7.5–11 ms like Xbox BLE. */
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

void uni_hid_parser_steam_triton_init_report(uni_hid_device_t* d) {
    ARG_UNUSED(d);
    /* Full-state reports — do not clear between reports. */
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
                d->controller.battery = report[2]; /* ucBatteryLevel */
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
