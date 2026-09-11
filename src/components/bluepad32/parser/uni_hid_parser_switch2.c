// SPDX-License-Identifier: Apache-2.0
// Switch 2 BLE GATT client (Joy-Con 2 / Pro Controller 2).

#include "parser/uni_hid_parser_switch2.h"

#include <string.h>
#include <stdio.h>

#include "bt/uni_bt_defines.h"
#include "controller/uni_controller.h"
#include "hid_usage.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

#include <btstack.h>

#include "bt/uni_bt.h"
#include "bt/uni_bt_le.h"
#include "sdkconfig.h"

// Nintendo Switch 2 GATT UUIDs
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

#define SW2_CMD_PAIR 0x15
#define SW2_SUBCMD_PAIR_SET_MAC 0x01
#define SW2_SUBCMD_PAIR_LTK1 0x04
#define SW2_SUBCMD_PAIR_LTK2 0x02
#define SW2_SUBCMD_PAIR_FINISH 0x03
#define SW2_CMD_MEMORY 0x02
#define SW2_SUBCMD_MEMORY_READ 0x04
#define SW2_CMD_LEDS 0x09
#define SW2_SUBCMD_LEDS_SET_PLAYER 0x07
#define SW2_CMD_FEATURE 0x0c
#define SW2_SUBCMD_FEATURE_INIT 0x02
#define SW2_SUBCMD_FEATURE_ENABLE 0x04
#define SW2_FEATURE_MOTION 0x04

#define SW2_ADDR_CONTROLLER_INFO 0x00013000
#define SW2_CALIBRATION_JOYSTICK_1 0x0130A8
#define SW2_CALIBRATION_JOYSTICK_2 0x0130E8
#define SW2_CALIBRATION_USER_JOYSTICK_1 0x1fc042
#define SW2_CALIBRATION_USER_JOYSTICK_2 0x1fc062

#define SW2_MFG_COMPANY_ID 0x0553
#define SW2_MAX_SERVICES 3

// Well-known ATT handles on Switch 2 Pro (from Nadeflore / Bit-Axis)
#define SW2_HANDLE_INPUT_NOTIFY 0x000au
#define SW2_HANDLE_INPUT_ALT_NOTIFY 0x000eu
#define SW2_HANDLE_CMD_WRITE 0x0014u
#define SW2_HANDLE_VIBRATION_PRO 0x0016u
#define SW2_HANDLE_CMD_NOTIFY 0x001au
// CCCD handles (value_handle + 1). BTstack READ_BY_TYPE for 0x2902 fails on Switch 2.
#define SW2_HANDLE_BOOTSTRAP_CCCD 0x0004u
#define SW2_HANDLE_INPUT_NOTIFY_CCCD 0x000bu
#define SW2_HANDLE_INPUT_ALT_NOTIFY_CCCD 0x000fu
#define SW2_HANDLE_CMD_NOTIFY_CCCD 0x001bu
#define SW2_SETUP_STABILIZE_MS 500
#define SW2_CMD_TIMEOUT_MS 2000

typedef enum {
    SW2_STATE_IDLE,
    SW2_STATE_DISCOVER_SERVICE,
    SW2_STATE_DISCOVER_CHARS,
    SW2_STATE_STABILIZE,
    SW2_STATE_BOOTSTRAP,
    SW2_STATE_ENABLE_CMD_NOTIFY,
    SW2_STATE_INIT_BURST,
    SW2_STATE_PAIR,
    SW2_STATE_READ_CALIBRATION,
    SW2_STATE_ENABLE_INPUT_NOTIFY,
    SW2_STATE_ENABLE_FEATURES,
    SW2_STATE_READY,
} sw2_state_t;

typedef struct {
    uint8_t cmd;
    uint8_t subcmd;
    uint8_t data_len;
    const uint8_t* data;
} sw2_init_cmd_t;

typedef struct {
    sw2_state_t state;
    uint8_t init_step;
    bool needs_pair;
    bool waiting_encryption;
    uint8_t cccd_smp_tried;
    bool cmd_response_pending;
    uint8_t expected_cmd;
    uint8_t vibration_packet_id;
    uint8_t service_count;
    uint8_t service_discover_idx;
    uint16_t product_id;
    gatt_client_service_t services[SW2_MAX_SERVICES];
    uint16_t cmd_write_handle;
    uint16_t cmd_notify_handle;
    uint16_t input_notify_handle;
    uint16_t vibration_handle;
    btstack_timer_source_t setup_timer;
    int16_t stick_l_center[2];
    int16_t stick_l_max[2];
    int16_t stick_l_min[2];
    int16_t stick_r_center[2];
    int16_t stick_r_max[2];
    int16_t stick_r_min[2];
    uint8_t home_latch_frames;
    int8_t pair_partner_idx;   // other Joy-Con device index, -1 if none
    int8_t pair_output_idx;    // merged gamepad slot (player 1), -1 when solo
    uint8_t pair_role;         // sw2_pair_role_t
} sw2_instance_t;

typedef enum {
    SW2_PAIR_NONE = 0,
    SW2_PAIR_PRIMARY = 1,    // Left Joy-Con slot when paired; emits merged gamepad
    SW2_PAIR_SECONDARY = 2,  // Right Joy-Con; input merged into primary
} sw2_pair_role_t;

// Nintendo Home is often a single composed-report frame; latch so PS System isn't lost
// before Core0 reads the lock-free BT pad staging buffer.
#define SW2_HOME_LATCH_FRAMES 24

_Static_assert(sizeof(sw2_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "Switch2 instance too big");

static gatt_client_notification_t sw2_notify_listeners[CONFIG_BLUEPAD32_MAX_DEVICES];
static bool sw2_notify_listener_registered[CONFIG_BLUEPAD32_MAX_DEVICES];
static uint8_t sw2_first_input_logged[CONFIG_BLUEPAD32_MAX_DEVICES];
static btstack_timer_source_t sw2_keepalive_timers[CONFIG_BLUEPAD32_MAX_DEVICES];
static bool sw2_keepalive_active[CONFIG_BLUEPAD32_MAX_DEVICES];

#define SW2_KEEPALIVE_MS 5
#define SW2_KEEPALIVE_PAIRED_MS 10

#ifndef SW2_DEBUG_RAW_INPUT
#define SW2_DEBUG_RAW_INPUT 0
#endif

#define SW2_RAW_LOG_MAX 64
static uint8_t sw2_last_btn_dword[CONFIG_BLUEPAD32_MAX_DEVICES][4];
static uni_gamepad_t sw2_joycon_half_gp[CONFIG_BLUEPAD32_MAX_DEVICES];
static bool sw2_paired_keepalive_alternate;
static bool sw2_merge_emit_scheduled;
static btstack_timer_source_t sw2_merge_emit_timer;
static uint32_t sw2_last_input_ms[CONFIG_BLUEPAD32_MAX_DEVICES];

static const uint8_t sw2_ltk1[17] = {0x00, 0xea, 0xbd, 0x47, 0x13, 0x89, 0x35, 0x42, 0xc6, 0x79,
                                       0xee, 0x07, 0xf2, 0x53, 0x2c, 0x6c, 0x31};
static const uint8_t sw2_ltk2[17] = {0x00, 0x40, 0xb0, 0x8a, 0x5f, 0xcd, 0x1f, 0x9b, 0x41, 0x12,
                                       0x5c, 0xac, 0xc6, 0x3f, 0x38, 0xa0, 0x73};

static const uint8_t sw2_led_pattern[8] = {0x01, 0x03, 0x07, 0x0f, 0x09, 0x05, 0x0d, 0x06};

// TommyWabg controller.py sw2_init_commands (after cmd-response notify is enabled)
static const uint8_t sw2_init_p03_0d[] = {0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const uint8_t sw2_init_p15_03[] = {0x00};
static const uint8_t sw2_init_p0c_02[] = {0x2f, 0x00, 0x00, 0x00};
static const uint8_t sw2_init_p0a_08[] = {0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                         0x35, 0x00, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t sw2_init_p0c_04[] = {0x2f, 0x00, 0x00, 0x00};
static const uint8_t sw2_init_p03_0a[] = {0x09, 0x00, 0x00, 0x00};
static const uint8_t sw2_init_p01_01[] = {0x00, 0x00, 0x00, 0x00};
static const uint8_t sw2_init_p09_07[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const sw2_init_cmd_t sw2_init_sequence[] = {
    {0x03, 0x0d, sizeof(sw2_init_p03_0d), sw2_init_p03_0d},
    {0x07, 0x01, 0, NULL},
    {0x16, 0x01, 0, NULL},
    {0x15, 0x03, sizeof(sw2_init_p15_03), sw2_init_p15_03},
    {0x0c, 0x02, sizeof(sw2_init_p0c_02), sw2_init_p0c_02},
    {0x11, 0x03, 0, NULL},
    {0x0a, 0x08, sizeof(sw2_init_p0a_08), sw2_init_p0a_08},
    {0x0c, 0x04, sizeof(sw2_init_p0c_04), sw2_init_p0c_04},
    {0x03, 0x0a, sizeof(sw2_init_p03_0a), sw2_init_p03_0a},
    {0x10, 0x01, 0, NULL},
    {0x01, 0x0c, 0, NULL},
    {0x01, 0x01, sizeof(sw2_init_p01_01), sw2_init_p01_01},
    {0x09, 0x07, sizeof(sw2_init_p09_07), sw2_init_p09_07},
};

static void sw2_gatt_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);

void uni_hid_parser_switch2_send_keepalive(uni_hid_device_t* d);

static sw2_instance_t* get_instance(uni_hid_device_t* d) { return (sw2_instance_t*)&d->parser_data[0]; }

static void sw2_send_command(uni_hid_device_t* d, uint8_t cmd, uint8_t subcmd, const uint8_t* data, uint8_t data_len);

static void sw2_stop_keepalive_timer(uni_hid_device_t* d);

static void sw2_request_fast_connection(uni_hid_device_t* d);

static bool sw2_is_joycon_pid(uint16_t pid) {
    return pid == UNI_SW2_JOYCON_L_PID || pid == UNI_SW2_JOYCON_R_PID;
}

static void sw2_break_joycon_pair(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    if (ins->pair_role == SW2_PAIR_NONE)
        return;

    const int partner_idx = ins->pair_partner_idx;
    ins->pair_role = SW2_PAIR_NONE;
    ins->pair_partner_idx = -1;

    if (partner_idx >= 0 && partner_idx < CONFIG_BLUEPAD32_MAX_DEVICES) {
        uni_hid_device_t* partner = uni_hid_device_get_instance_for_idx(partner_idx);
        if (partner) {
            sw2_instance_t* pins = get_instance(partner);
            pins->pair_role = SW2_PAIR_NONE;
            pins->pair_partner_idx = -1;
            pins->pair_output_idx = -1;
        }
    }
    ins->pair_output_idx = -1;
}

static int sw2_find_joycon_partner_idx(const uni_hid_device_t* d) {
    const sw2_instance_t* ins = get_instance((uni_hid_device_t*)d);
    if (!sw2_is_joycon_pid(ins->product_id))
        return -1;

    const uint16_t want_pid =
        (ins->product_id == UNI_SW2_JOYCON_L_PID) ? UNI_SW2_JOYCON_R_PID : UNI_SW2_JOYCON_L_PID;
    const int my_idx = uni_hid_device_get_idx_for_instance(d);

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        if (i == my_idx)
            continue;
        uni_hid_device_t* other = uni_hid_device_get_instance_for_idx(i);
        if (!other || uni_bt_conn_get_state(&other->conn) != UNI_BT_CONN_STATE_DEVICE_READY)
            continue;
        if (!uni_hid_parser_switch2_is_ble_device(other))
            continue;
        const sw2_instance_t* oins = get_instance(other);
        if (oins->product_id != want_pid || oins->state != SW2_STATE_READY)
            continue;
        if (oins->pair_role != SW2_PAIR_NONE)
            continue;
        return i;
    }
    return -1;
}

static void sw2_establish_joycon_pair(uni_hid_device_t* d, int partner_idx) {
    uni_hid_device_t* partner = uni_hid_device_get_instance_for_idx(partner_idx);
    if (!partner)
        return;

    int left_idx;
    int right_idx;
    if (get_instance(d)->product_id == UNI_SW2_JOYCON_L_PID) {
        left_idx = uni_hid_device_get_idx_for_instance(d);
        right_idx = partner_idx;
    } else {
        left_idx = partner_idx;
        right_idx = uni_hid_device_get_idx_for_instance(d);
    }

    uni_hid_device_t* left_d = uni_hid_device_get_instance_for_idx(left_idx);
    uni_hid_device_t* right_d = uni_hid_device_get_instance_for_idx(right_idx);
    if (!left_d || !right_d)
        return;

    sw2_instance_t* lins = get_instance(left_d);
    sw2_instance_t* rins = get_instance(right_d);
    const int8_t output_idx = (int8_t)((left_idx < right_idx) ? left_idx : right_idx);

    lins->pair_role = SW2_PAIR_PRIMARY;
    lins->pair_partner_idx = (int8_t)right_idx;
    lins->pair_output_idx = output_idx;
    rins->pair_role = SW2_PAIR_SECONDARY;
    rins->pair_partner_idx = (int8_t)left_idx;
    rins->pair_output_idx = output_idx;

    uint8_t led[4] = {sw2_led_pattern[output_idx < 8 ? output_idx : 0], 0, 0, 0};
    sw2_send_command(left_d, SW2_CMD_LEDS, SW2_SUBCMD_LEDS_SET_PLAYER, led, sizeof(led));
    sw2_send_command(right_d, SW2_CMD_LEDS, SW2_SUBCMD_LEDS_SET_PLAYER, led, sizeof(led));

    sw2_stop_keepalive_timer(right_d);
    sw2_request_fast_connection(left_d);
    sw2_request_fast_connection(right_d);

    logi("Switch2: Joy-Con pair L@%d + R@%d -> player slot %d\n", left_idx, right_idx, output_idx);
}

static void sw2_try_joycon_pair(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    if (!sw2_is_joycon_pid(ins->product_id))
        return;

    const int partner_idx = sw2_find_joycon_partner_idx(d);
    if (partner_idx >= 0) {
        sw2_establish_joycon_pair(d, partner_idx);
        return;
    }

    uni_bt_enable_new_connections_unsafe(true);
    uni_bt_le_scan_start();
    logi("Switch2: Joy-Con solo — BLE scan on for partner Joy-Con\n");
}

static void sw2_merge_joycon_pair(uni_gamepad_t* dst, const uni_gamepad_t* left_gp, const uni_gamepad_t* right_gp) {
    uni_gamepad_t merged = {0};

    merged.axis_x = left_gp->axis_x;
    merged.axis_y = left_gp->axis_y;
    merged.brake = left_gp->brake;
    merged.axis_rx = right_gp->axis_rx;
    merged.axis_ry = right_gp->axis_ry;
    merged.throttle = right_gp->throttle;
    merged.buttons = left_gp->buttons | right_gp->buttons;
    merged.misc_buttons = left_gp->misc_buttons | right_gp->misc_buttons;
    merged.dpad = left_gp->dpad | right_gp->dpad;
    for (int i = 0; i < 3; i++) {
        merged.accel[i] = left_gp->accel[i];
        merged.gyro[i] = left_gp->gyro[i];
    }
    *dst = merged;
}

static void sw2_get_paired_left_right_idx(const uni_hid_device_t* d, int* left_idx, int* right_idx) {
    const sw2_instance_t* ins = get_instance((uni_hid_device_t*)d);
    const int my_idx = uni_hid_device_get_idx_for_instance(d);
    if (ins->pair_role == SW2_PAIR_NONE || ins->pair_partner_idx < 0) {
        *left_idx = my_idx;
        *right_idx = my_idx;
        return;
    }
    if (ins->product_id == UNI_SW2_JOYCON_L_PID) {
        *left_idx = my_idx;
        *right_idx = ins->pair_partner_idx;
    } else {
        *left_idx = ins->pair_partner_idx;
        *right_idx = my_idx;
    }
}

static void sw2_flush_paired_emit(uni_hid_device_t* primary) {
    sw2_instance_t* pins = get_instance(primary);
    if (pins->state != SW2_STATE_READY || pins->pair_role != SW2_PAIR_PRIMARY || pins->pair_partner_idx < 0)
        return;

    int left_idx;
    int right_idx;
    sw2_get_paired_left_right_idx(primary, &left_idx, &right_idx);
    if (left_idx < 0 || right_idx < 0 || left_idx >= CONFIG_BLUEPAD32_MAX_DEVICES ||
        right_idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
        return;

    sw2_merge_joycon_pair(&primary->controller.gamepad, &sw2_joycon_half_gp[left_idx],
                          &sw2_joycon_half_gp[right_idx]);
    primary->controller.klass = UNI_CONTROLLER_CLASS_GAMEPAD;
    uni_hid_device_process_controller(primary);
}

static void sw2_merge_emit_timer_cb(btstack_timer_source_t* ts) {
    sw2_merge_emit_scheduled = false;
    uni_hid_device_t* primary = (uni_hid_device_t*)btstack_run_loop_get_timer_context(ts);
    if (primary)
        sw2_flush_paired_emit(primary);
}

static void sw2_schedule_paired_emit(uni_hid_device_t* primary) {
    if (sw2_merge_emit_scheduled)
        return;
    sw2_merge_emit_scheduled = true;
    btstack_run_loop_set_timer_handler(&sw2_merge_emit_timer, sw2_merge_emit_timer_cb);
    btstack_run_loop_set_timer_context(&sw2_merge_emit_timer, primary);
    btstack_run_loop_set_timer(&sw2_merge_emit_timer, 0);
    btstack_run_loop_add_timer(&sw2_merge_emit_timer);
}

static void sw2_emit_paired_or_solo(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    const int idx = uni_hid_device_get_idx_for_instance(d);

    if (idx >= 0 && idx < CONFIG_BLUEPAD32_MAX_DEVICES)
        sw2_joycon_half_gp[idx] = d->controller.gamepad;

    if (ins->pair_role == SW2_PAIR_NONE) {
        d->controller.klass = UNI_CONTROLLER_CLASS_GAMEPAD;
        uni_hid_device_process_controller(d);
        return;
    }

    uni_hid_device_t* primary = uni_hid_device_get_instance_for_idx(
        (ins->pair_role == SW2_PAIR_PRIMARY) ? idx : ins->pair_partner_idx);
    if (!primary || get_instance(primary)->state != SW2_STATE_READY)
        return;

    sw2_schedule_paired_emit(primary);
}

static void sw2_process_merged_or_solo(uni_hid_device_t* d) {
    sw2_emit_paired_or_solo(d);
}

static const char* sw2_state_name(sw2_state_t state) {
    switch (state) {
        case SW2_STATE_IDLE:
            return "idle";
        case SW2_STATE_DISCOVER_SERVICE:
            return "discover_service";
        case SW2_STATE_DISCOVER_CHARS:
            return "discover_chars";
        case SW2_STATE_STABILIZE:
            return "stabilize";
        case SW2_STATE_BOOTSTRAP:
            return "bootstrap";
        case SW2_STATE_ENABLE_CMD_NOTIFY:
            return "enable_cmd_notify";
        case SW2_STATE_INIT_BURST:
            return "init_burst";
        case SW2_STATE_PAIR:
            return "pair";
        case SW2_STATE_READ_CALIBRATION:
            return "read_calibration";
        case SW2_STATE_ENABLE_INPUT_NOTIFY:
            return "enable_input_notify";
        case SW2_STATE_ENABLE_FEATURES:
            return "enable_features";
        case SW2_STATE_READY:
            return "ready";
        default:
            return "?";
    }
}

static void sw2_set_state(sw2_instance_t* ins, sw2_state_t state) {
    if (ins->state != state) {
        logi("Switch2: state %s -> %s\n", sw2_state_name(ins->state), sw2_state_name(state));
    }
    ins->state = state;
}

static bool uuid128_equal(const uint8_t* a, const uint8_t* b) { return memcmp(a, b, 16) == 0; }

static bool sw2_is_nintendo_service(const gatt_client_service_t* svc) {
    if (svc->uuid16 != 0)
        return false;
    const uint8_t* u = svc->uuid128;
    for (unsigned i = 0; i <= 12; i++) {
        if (u[i] == 0xab && u[i + 1] == 0x7d && u[i + 2] == 0xe9 && u[i + 3] == 0xbe)
            return true;
        if (u[i] == 0xbe && u[i + 1] == 0xe9 && u[i + 2] == 0x7d && u[i + 3] == 0xab)
            return true;
    }
    return false;
}

static void sw2_bind_char_by_handle(sw2_instance_t* ins, const gatt_client_characteristic_t* ch) {
    switch (ch->value_handle) {
        case SW2_HANDLE_INPUT_NOTIFY:
            if (ch->properties & ATT_PROPERTY_NOTIFY) {
                ins->input_notify_handle = ch->value_handle;
                logi("Switch2:   input notify h=%u end=%u\n", ch->value_handle, ch->end_handle);
            }
            break;
        case SW2_HANDLE_INPUT_ALT_NOTIFY:
            if (ch->properties & ATT_PROPERTY_NOTIFY) {
                logi("Switch2:   alt input notify h=%u end=%u\n", ch->value_handle, ch->end_handle);
            }
            break;
        case SW2_HANDLE_CMD_WRITE:
            if (ch->properties & (ATT_PROPERTY_WRITE | ATT_PROPERTY_WRITE_WITHOUT_RESPONSE)) {
                ins->cmd_write_handle = ch->value_handle;
                logi("Switch2:   cmd write h=%u\n", ch->value_handle);
            }
            break;
        case SW2_HANDLE_CMD_NOTIFY:
            if (ch->properties & ATT_PROPERTY_NOTIFY) {
                ins->cmd_notify_handle = ch->value_handle;
                logi("Switch2:   cmd notify h=%u end=%u\n", ch->value_handle, ch->end_handle);
            }
            break;
        case SW2_HANDLE_VIBRATION_PRO:
            if (ch->properties & (ATT_PROPERTY_WRITE | ATT_PROPERTY_WRITE_WITHOUT_RESPONSE)) {
                ins->vibration_handle = ch->value_handle;
                logi("Switch2:   vibration h=%u\n", ch->value_handle);
            }
            break;
        default:
            break;
    }
}

static void sw2_set_default_calibration(sw2_instance_t* ins) {
    ins->stick_l_center[0] = ins->stick_l_center[1] = 2048;
    ins->stick_l_max[0] = ins->stick_l_max[1] = 1500;
    ins->stick_l_min[0] = ins->stick_l_min[1] = 1500;
    ins->stick_r_center[0] = ins->stick_r_center[1] = 2048;
    ins->stick_r_max[0] = ins->stick_r_max[1] = 1500;
    ins->stick_r_min[0] = ins->stick_r_min[1] = 1500;
}

static int16_t sw2_decode_s16(const uint8_t* p) {
    return (int16_t)little_endian_read_16(p, 0);
}

static void sw2_get_stick_xy(const uint8_t* data, int16_t* x, int16_t* y) {
    uint32_t value = data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16);
    *x = (int16_t)(value & 0x0fff);
    *y = (int16_t)((value >> 12) & 0x0fff);
}

static int32_t sw2_apply_stick_axis(int16_t raw, int16_t center, int16_t max_abs, int16_t min_abs) {
    int32_t signed_value = raw - center;
    if (signed_value > 32) {
        return (signed_value * 512) / max_abs;
    }
    if (signed_value < -32) {
        return (signed_value * 512) / min_abs;
    }
    return 0;
}

static int32_t sw2_clamp_axis(int32_t v) {
    if (v > 512)
        return 512;
    if (v < -512)
        return -512;
    return v;
}

static void sw2_build_command(uint8_t* out, uint8_t* out_len, uint8_t cmd, uint8_t subcmd, const uint8_t* data,
                              uint8_t data_len) {
    out[0] = cmd;
    out[1] = 0x91;
    out[2] = 0x01;
    out[3] = subcmd;
    out[4] = 0x00;
    out[5] = data_len;
    out[6] = 0x00;
    out[7] = 0x00;
    if (data_len > 0 && data != NULL) {
        memcpy(&out[8], data, data_len);
    }
    *out_len = 8 + data_len;
}

static void sw2_advance_after_command(uni_hid_device_t* d);

static void sw2_read_calibration(uni_hid_device_t* d);

static void sw2_subscribe_input(uni_hid_device_t* d);

static uint8_t sw2_full_report_base(uint16_t len);

static int16_t sw2_pro2_inreport_base(const uint8_t* report, uint16_t len);

static void sw2_apply_sticks(uni_gamepad_t* gp, sw2_instance_t* ins, const uint8_t* report, uint8_t left_off,
                             uint8_t right_off);

static void sw2_stop_notify_listener_for_device(uni_hid_device_t* d) {
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0)
        return;
    if (sw2_notify_listener_registered[idx]) {
        gatt_client_stop_listening_for_characteristic_value_updates(&sw2_notify_listeners[idx]);
        sw2_notify_listener_registered[idx] = false;
    }
}

static void sw2_register_notify_listener(uni_hid_device_t* d) {
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0)
        return;
    if (sw2_notify_listener_registered[idx])
        return;
    gatt_client_listen_for_characteristic_value_updates(&sw2_notify_listeners[idx], sw2_gatt_handler, d->conn.handle,
                                                        NULL);
    sw2_notify_listener_registered[idx] = true;
    logi("Switch2: registered GATT notify listener\n");
}

static void sw2_disarm_cmd_timeout(sw2_instance_t* ins) {
    btstack_run_loop_remove_timer(&ins->setup_timer);
}

static void sw2_cmd_timeout(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = (uni_hid_device_t*)btstack_run_loop_get_timer_context(ts);
    if (!d)
        return;
    sw2_instance_t* ins = get_instance(d);
    if (!ins->cmd_response_pending)
        return;
    logi("Switch2: cmd 0x%02x timeout in %s step %u — continuing\n", ins->expected_cmd, sw2_state_name(ins->state),
         ins->init_step);
    ins->cmd_response_pending = false;
    sw2_advance_after_command(d);
}

static void sw2_arm_cmd_timeout(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    sw2_disarm_cmd_timeout(ins);
    btstack_run_loop_set_timer_context(&ins->setup_timer, d);
    btstack_run_loop_set_timer_handler(&ins->setup_timer, sw2_cmd_timeout);
    btstack_run_loop_set_timer(&ins->setup_timer, SW2_CMD_TIMEOUT_MS);
    btstack_run_loop_add_timer(&ins->setup_timer);
}

static void sw2_write_cccd_notify(uni_hid_device_t* d, uint16_t cccd_handle) {
    static const uint8_t enable_notify[2] = {0x01, 0x00};
    gatt_client_write_characteristic_descriptor_using_descriptor_handle(
        sw2_gatt_handler, d->conn.handle, cccd_handle, sizeof(enable_notify), (uint8_t*)enable_notify);
}

static void sw2_write_bootstrap_gate(uni_hid_device_t* d) {
    static const uint8_t enable_notify[2] = {0x01, 0x00};
    gatt_client_write_value_of_characteristic(sw2_gatt_handler, d->conn.handle, SW2_HANDLE_BOOTSTRAP_CCCD,
                                              sizeof(enable_notify), (uint8_t*)enable_notify);
}

static void sw2_enable_cmd_notify_cccd(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    sw2_set_state(ins, SW2_STATE_ENABLE_CMD_NOTIFY);
    logi("Switch2: enable cmd notify CCCD h=%u\n", SW2_HANDLE_CMD_NOTIFY_CCCD);
    sw2_write_cccd_notify(d, SW2_HANDLE_CMD_NOTIFY_CCCD);
}

static void sw2_on_setup_stabilized(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = (uni_hid_device_t*)btstack_run_loop_get_timer_context(ts);
    if (!d)
        return;
    sw2_instance_t* ins = get_instance(d);
    sw2_set_state(ins, SW2_STATE_BOOTSTRAP);
    logi("Switch2: bootstrap gate h=%u\n", SW2_HANDLE_BOOTSTRAP_CCCD);
    sw2_write_bootstrap_gate(d);
}

static void sw2_stop_keepalive_timer(uni_hid_device_t* d) {
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES || !sw2_keepalive_active[idx])
        return;
    btstack_run_loop_remove_timer(&sw2_keepalive_timers[idx]);
    sw2_keepalive_active[idx] = false;
}

static void sw2_keepalive_timer_cb(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = (uni_hid_device_t*)btstack_run_loop_get_timer_context(ts);
    if (!d)
        return;
    sw2_instance_t* ins = get_instance(d);
    if (ins->state != SW2_STATE_READY) {
        sw2_stop_keepalive_timer(d);
        return;
    }
    if (ins->pair_role == SW2_PAIR_SECONDARY) {
        sw2_stop_keepalive_timer(d);
        return;
    }

    uint16_t interval = SW2_KEEPALIVE_MS;
    if (ins->pair_role == SW2_PAIR_PRIMARY && ins->pair_partner_idx >= 0) {
        interval = SW2_KEEPALIVE_PAIRED_MS;
        const int my_idx = uni_hid_device_get_idx_for_instance(d);
        const int partner_idx = ins->pair_partner_idx;
        const uint32_t now = btstack_run_loop_get_time_ms();
        const uint32_t last =
            (my_idx >= 0 && my_idx < CONFIG_BLUEPAD32_MAX_DEVICES &&
             partner_idx >= 0 && partner_idx < CONFIG_BLUEPAD32_MAX_DEVICES)
                ? (sw2_last_input_ms[my_idx] > sw2_last_input_ms[partner_idx] ? sw2_last_input_ms[my_idx]
                                                                                : sw2_last_input_ms[partner_idx])
                : 0;
        if (last != 0 && (now - last) < SW2_KEEPALIVE_PAIRED_MS) {
            btstack_run_loop_set_timer(ts, 1);
            btstack_run_loop_add_timer(ts);
            return;
        }
        uni_hid_device_t* partner = uni_hid_device_get_instance_for_idx(partner_idx);
        if (partner && get_instance(partner)->state == SW2_STATE_READY) {
            if (sw2_paired_keepalive_alternate)
                uni_hid_parser_switch2_send_keepalive(partner);
            else
                uni_hid_parser_switch2_send_keepalive(d);
            sw2_paired_keepalive_alternate = !sw2_paired_keepalive_alternate;
        } else {
            uni_hid_parser_switch2_send_keepalive(d);
        }
    } else {
        uni_hid_parser_switch2_send_keepalive(d);
    }

    btstack_run_loop_set_timer(ts, interval);
    btstack_run_loop_add_timer(ts);
}

static void sw2_start_keepalive_timer(uni_hid_device_t* d) {
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
        return;
    sw2_stop_keepalive_timer(d);
    btstack_run_loop_set_timer_handler(&sw2_keepalive_timers[idx], sw2_keepalive_timer_cb);
    btstack_run_loop_set_timer_context(&sw2_keepalive_timers[idx], d);
    btstack_run_loop_set_timer(&sw2_keepalive_timers[idx], SW2_KEEPALIVE_MS);
    btstack_run_loop_add_timer(&sw2_keepalive_timers[idx]);
    sw2_keepalive_active[idx] = true;
}

static void sw2_request_fast_connection(uni_hid_device_t* d) {
    // TommyWabg requests throughput-optimized params (~7.5 ms). interval=6 => 6*1.25ms.
    int rc = gap_request_connection_parameter_update(d->conn.handle, 6, 6, 0, 400);
    logi("Switch2: conn param update (7.5ms) rc=%d\n", rc);
}

static void sw2_finish_ready(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    sw2_disarm_cmd_timeout(ins);
    sw2_set_state(ins, SW2_STATE_READY);
    if (ins->vibration_handle == 0) {
        logi("Switch2: WARNING vibration h=0 — expect link timeout without keepalive\n");
    }
    sw2_request_fast_connection(d);
    uni_hid_parser_switch2_send_keepalive(d);
    uint8_t led[4] = {sw2_led_pattern[0], 0, 0, 0};
    sw2_send_command(d, SW2_CMD_LEDS, SW2_SUBCMD_LEDS_SET_PLAYER, led, sizeof(led));
    uni_hid_device_connect(d);
    uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_DEVICE_PENDING_READY);
    sw2_try_joycon_pair(d);
    uni_hid_device_set_ready_complete(d);
    sw2_start_keepalive_timer(d);
    logi("Switch2: device ready (pid=0x%04x)\n", ins->product_id);
}

static void sw2_discover_next_service_chars(uni_hid_device_t* d, hci_con_handle_t handle) {
    sw2_instance_t* ins = get_instance(d);
    gatt_client_discover_characteristics_for_service(sw2_gatt_handler, handle, &ins->services[ins->service_discover_idx]);
}

static void sw2_send_init_burst_step(uni_hid_device_t* d);

static void sw2_start_init_burst(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    sw2_set_state(ins, SW2_STATE_INIT_BURST);
    ins->init_step = 0;
    sw2_send_init_burst_step(d);
}

static void sw2_send_init_burst_step(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    if (ins->init_step >= ARRAY_SIZE(sw2_init_sequence)) {
        sw2_read_calibration(d);
        return;
    }
    const sw2_init_cmd_t* entry = &sw2_init_sequence[ins->init_step];
    sw2_send_command(d, entry->cmd, entry->subcmd, entry->data, entry->data_len);
}

static void sw2_start_gatt_setup(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    if (ins->cmd_write_handle == 0 || ins->cmd_notify_handle == 0 || ins->input_notify_handle == 0) {
        loge("Switch2: missing characteristics (cmd_w=%u cmd_n=%u in=%u)\n", ins->cmd_write_handle,
             ins->cmd_notify_handle, ins->input_notify_handle);
        uni_hid_device_disconnect(d);
        return;
    }
    logi("Switch2: GATT handles cmd_w=%u cmd_n=%u in_n=%u vib=%u\n", ins->cmd_write_handle,
         ins->cmd_notify_handle, ins->input_notify_handle, ins->vibration_handle);
    sw2_set_state(ins, SW2_STATE_STABILIZE);
    btstack_run_loop_set_timer_context(&ins->setup_timer, d);
    btstack_run_loop_set_timer_handler(&ins->setup_timer, sw2_on_setup_stabilized);
    btstack_run_loop_set_timer(&ins->setup_timer, SW2_SETUP_STABILIZE_MS);
    btstack_run_loop_add_timer(&ins->setup_timer);
}

static void sw2_start_pairing(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    sw2_set_state(ins, SW2_STATE_PAIR);
    ins->init_step = 0;
    bd_addr_t local;
    gap_local_bd_addr(local);
    uint8_t buf[14];
    buf[0] = 0x00;
    buf[1] = 0x02;
    memcpy(&buf[2], local, 6);
    memcpy(&buf[8], local, 6);
    sw2_send_command(d, SW2_CMD_PAIR, SW2_SUBCMD_PAIR_SET_MAC, buf, sizeof(buf));
}

static void sw2_read_calibration(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    sw2_set_state(ins, SW2_STATE_READ_CALIBRATION);
    uint8_t req[8];
    req[0] = 0x0b;
    req[1] = 0x7e;
    req[2] = 0x00;
    req[3] = 0x00;
    little_endian_store_32(req, 4, SW2_CALIBRATION_USER_JOYSTICK_1);
    sw2_send_command(d, SW2_CMD_MEMORY, SW2_SUBCMD_MEMORY_READ, req, sizeof(req));
}

static void sw2_subscribe_input(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    sw2_set_state(ins, SW2_STATE_ENABLE_INPUT_NOTIFY);
    ins->init_step = 0;
    logi("Switch2: enable input notify CCCD h=%u (primary 0x0A only)\n", SW2_HANDLE_INPUT_NOTIFY_CCCD);
    sw2_write_cccd_notify(d, SW2_HANDLE_INPUT_NOTIFY_CCCD);
}

static void sw2_handle_command_response(uni_hid_device_t* d, const uint8_t* data, uint16_t len) {
    sw2_instance_t* ins = get_instance(d);
    if (len < 2) {
        return;
    }
    if (data[0] == 0 && data[1] == 0) {
        return;
    }
    if (data[1] != 0x01) {
        return;
    }
    sw2_disarm_cmd_timeout(ins);
    ins->cmd_response_pending = false;
    uni_hid_device_kick_connection_timeout(d);
    sw2_advance_after_command(d);
}

static void sw2_advance_after_command(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);

    switch (ins->state) {
        case SW2_STATE_PAIR:
            ins->init_step++;
            if (ins->init_step == 1) {
                sw2_send_command(d, SW2_CMD_PAIR, SW2_SUBCMD_PAIR_LTK1, sw2_ltk1, sizeof(sw2_ltk1));
            } else if (ins->init_step == 2) {
                sw2_send_command(d, SW2_CMD_PAIR, SW2_SUBCMD_PAIR_LTK2, sw2_ltk2, sizeof(sw2_ltk2));
            } else if (ins->init_step == 3) {
                sw2_send_command(d, SW2_CMD_PAIR, SW2_SUBCMD_PAIR_FINISH, (const uint8_t*)"\0", 1);
            } else {
                ins->needs_pair = false;
                sw2_subscribe_input(d);
            }
            break;

        case SW2_STATE_INIT_BURST:
            ins->init_step++;
            if (ins->init_step < ARRAY_SIZE(sw2_init_sequence)) {
                sw2_send_init_burst_step(d);
            } else {
                sw2_read_calibration(d);
            }
            break;

        case SW2_STATE_READ_CALIBRATION:
            /* Motion is already enabled in init burst (0x0c/0x04 with 0x2f). A second
             * feature-enable here disconnects Switch 2 Pro / Joy-Con. */
            if (ins->needs_pair) {
                sw2_start_pairing(d);
            } else {
                sw2_subscribe_input(d);
            }
            break;

        default:
            break;
    }
}

static void sw2_send_command(uni_hid_device_t* d, uint8_t cmd, uint8_t subcmd, const uint8_t* data, uint8_t data_len) {
    sw2_instance_t* ins = get_instance(d);
    uint8_t buffer[64];
    uint8_t buffer_len = 0;
    sw2_build_command(buffer, &buffer_len, cmd, subcmd, data, data_len);
    ins->expected_cmd = cmd;
    ins->cmd_response_pending = true;
    if (ins->state != SW2_STATE_READY) {
        logi("Switch2: cmd 0x%02x sub 0x%02x (%s step %u)\n", cmd, subcmd, sw2_state_name(ins->state),
             ins->init_step);
    }
    // Switch 2 cmd channel (0x14) is write-without-response only; Write Request returns att=0x03.
    uint8_t status =
        gatt_client_write_value_of_characteristic_without_response(d->conn.handle, ins->cmd_write_handle, buffer_len,
                                                                   buffer);
    if (status != ERROR_CODE_SUCCESS) {
        loge("Switch2: cmd write failed status=%u (cmd=0x%02x)\n", status, cmd);
        uni_hid_device_disconnect(d);
        return;
    }
    if (ins->state == SW2_STATE_INIT_BURST || ins->state == SW2_STATE_PAIR ||
        ins->state == SW2_STATE_READ_CALIBRATION || ins->state == SW2_STATE_ENABLE_FEATURES) {
        sw2_arm_cmd_timeout(d);
    }
    uni_hid_device_kick_connection_timeout(d);
}

static bool sw2_is_input_notification(const sw2_instance_t* ins, uint16_t value_handle, uint16_t len) {
    ARG_UNUSED(len);
    if (value_handle == ins->cmd_notify_handle || value_handle == SW2_HANDLE_CMD_NOTIFY)
        return false;
    if (value_handle == ins->input_notify_handle)
        return true;
    return value_handle == SW2_HANDLE_INPUT_NOTIFY || value_handle == SW2_HANDLE_INPUT_ALT_NOTIFY;
}

static void sw2_log_raw_packet(uni_hid_device_t* d, uint16_t value_handle, const uint8_t* report, uint16_t len) {
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES || len < 8)
        return;

    const bool first = !sw2_first_input_logged[idx];
    const bool btn_changed = memcmp(&report[4], sw2_last_btn_dword[idx], 4) != 0;
    if (!first && !btn_changed)
        return;

    sw2_first_input_logged[idx] = 1;
    memcpy(sw2_last_btn_dword[idx], &report[4], 4);

    const uint16_t store_len = len <= SW2_RAW_LOG_MAX ? len : SW2_RAW_LOG_MAX;
    char line[320];
    int pos = snprintf(line, sizeof(line), "[SW2 RAW idx=%d h=%u len=%u btn=%02x%02x%02x%02x]", idx, value_handle, len,
                       report[4], report[5], report[6], report[7]);
    for (uint16_t i = 0; i < store_len && pos > 0 && (size_t)pos < sizeof(line) - 4; i++) {
        pos += snprintf(line + pos, (size_t)(sizeof(line) - pos), " %02x", report[i]);
    }
    if (len > store_len && pos > 0 && (size_t)pos < sizeof(line) - 5) {
        snprintf(line + pos, (size_t)(sizeof(line) - pos), " ...");
    }
    logi("%s\n", line);
}

static bool sw2_is_gamepad_input_report(uint16_t value_handle, const uint8_t* report, uint16_t len) {
    ARG_UNUSED(report);
    // Bit-Axis / Nadeflore: 63-byte gamepad stream is on handle 0x0A (not 0x0E).
    return value_handle == SW2_HANDLE_INPUT_NOTIFY && len == 63;
}

static void sw2_parse_input_notify(uni_hid_device_t* d, uint16_t value_handle, const uint8_t* report,
                                   uint16_t len) {
    if (len < 8)
        return;
#if SW2_DEBUG_RAW_INPUT
    sw2_log_raw_packet(d, value_handle, report, len);
#endif
    uni_hid_device_kick_connection_timeout(d);

    sw2_instance_t* ins = get_instance(d);
    if (ins->state != SW2_STATE_READY)
        return;
    const int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx >= 0 && idx < CONFIG_BLUEPAD32_MAX_DEVICES)
        sw2_last_input_ms[idx] = btstack_run_loop_get_time_ms();
    if (!sw2_is_gamepad_input_report(value_handle, report, len))
        return;

    uni_hid_parser_switch2_parse_input_report(d, report, len);
    sw2_process_merged_or_solo(d);
}

static void sw2_gatt_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;

    uni_hid_device_t* d = NULL;
    hci_con_handle_t handle = 0;
    uint8_t att_status;
    uint8_t event = hci_event_packet_get_type(packet);

    switch (event) {
        case GATT_EVENT_SERVICE_QUERY_RESULT: {
            gatt_client_service_t service;
            gatt_event_service_query_result_get_service(packet, &service);
            handle = gatt_event_service_query_result_get_handle(packet);
            d = uni_hid_device_get_instance_for_connection_handle(handle);
            if (d) {
                sw2_instance_t* ins = get_instance(d);
                if (ins->service_count < SW2_MAX_SERVICES) {
                    ins->services[ins->service_count] = service;
                    logi("Switch2: service[%u] uuid16=0x%04x nintendo=%u handles=%u..%u\n",
                         ins->service_count, service.uuid16, sw2_is_nintendo_service(&service) ? 1u : 0u,
                         service.start_group_handle, service.end_group_handle);
                    ins->service_count++;
                }
            }
            break;
        }
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT: {
            gatt_client_characteristic_t ch;
            gatt_event_characteristic_query_result_get_characteristic(packet, &ch);
            handle = gatt_event_characteristic_query_result_get_handle(packet);
            d = uni_hid_device_get_instance_for_connection_handle(handle);
            if (!d)
                break;
            sw2_instance_t* ins = get_instance(d);
            sw2_bind_char_by_handle(ins, &ch);
            if (uuid128_equal(ch.uuid128, sw2_input_report_uuid128) && (ch.properties & ATT_PROPERTY_NOTIFY)) {
                ins->input_notify_handle = ch.value_handle;
            }
            if (uuid128_equal(ch.uuid128, sw2_cmd_response_uuid128) && (ch.properties & ATT_PROPERTY_NOTIFY)) {
                ins->cmd_notify_handle = ch.value_handle;
            }
            if (uuid128_equal(ch.uuid128, sw2_cmd_write_uuid128)) {
                ins->cmd_write_handle = ch.value_handle;
            }
            if (uuid128_equal(ch.uuid128, sw2_vibration_pro_uuid128) ||
                uuid128_equal(ch.uuid128, sw2_vibration_joycon_l_uuid128) ||
                uuid128_equal(ch.uuid128, sw2_vibration_joycon_r_uuid128)) {
                ins->vibration_handle = ch.value_handle;
            }
            break;
        }
        case GATT_EVENT_QUERY_COMPLETE:
            att_status = gatt_event_query_complete_get_att_status(packet);
            handle = gatt_event_query_complete_get_handle(packet);
            d = uni_hid_device_get_instance_for_connection_handle(handle);
            if (!d || !uni_hid_parser_switch2_is_ble_device(d))
                break;
            {
                sw2_instance_t* ins = get_instance(d);
                if (att_status != ATT_ERROR_SUCCESS) {
                    if (att_status == ATT_ERROR_INSUFFICIENT_AUTHENTICATION ||
                        att_status == ATT_ERROR_INSUFFICIENT_ENCRYPTION ||
                        (att_status == ATT_ERROR_ATTRIBUTE_NOT_FOUND &&
                         (ins->state == SW2_STATE_ENABLE_CMD_NOTIFY ||
                          ins->state == SW2_STATE_ENABLE_INPUT_NOTIFY))) {
                        if (!ins->cccd_smp_tried) {
                            ins->cccd_smp_tried = 1;
                            ins->waiting_encryption = true;
                            logi("Switch2: CCCD needs encryption (att=0x%02x, state=%s) -> SMP\n", att_status,
                                 sw2_state_name(ins->state));
                            /* Switch 2 requires AuthReq=0 (Just Works, no bonding). Bonding/SC
                             * causes CONFIRM_VALUE_FAILED / AUTH_REQUIREMENTS_MISMATCH. */
                            sm_set_authentication_requirements(0);
                            sm_request_pairing(d->conn.handle);
                            break;
                        }
                    }
                    if (ins->state == SW2_STATE_BOOTSTRAP) {
                        logi("Switch2: bootstrap skipped (att=0x%02x)\n", att_status);
                        sw2_enable_cmd_notify_cccd(d);
                        break;
                    }
                    loge("Switch2: GATT failed in state %s att=0x%02x\n", sw2_state_name(ins->state), att_status);
                    uni_hid_device_disconnect(d);
                    break;
                }
                switch (ins->state) {
                    case SW2_STATE_DISCOVER_SERVICE:
                        if (ins->service_count == 0) {
                            loge("Switch2: service not found\n");
                            uni_hid_device_disconnect(d);
                            break;
                        }
                        ins->service_discover_idx = 0;
                        sw2_set_state(ins, SW2_STATE_DISCOVER_CHARS);
                        logi("Switch2: found %u primary service(s)\n", ins->service_count);
                        sw2_discover_next_service_chars(d, handle);
                        break;
                    case SW2_STATE_DISCOVER_CHARS:
                        ins->service_discover_idx++;
                        if (ins->service_discover_idx < ins->service_count) {
                            sw2_discover_next_service_chars(d, handle);
                            break;
                        }
                        sw2_start_gatt_setup(d);
                        break;
                    case SW2_STATE_BOOTSTRAP:
                        sw2_enable_cmd_notify_cccd(d);
                        break;
                    case SW2_STATE_ENABLE_CMD_NOTIFY:
                        sw2_register_notify_listener(d);
                        sw2_start_init_burst(d);
                        break;
                    case SW2_STATE_ENABLE_INPUT_NOTIFY:
                        sw2_finish_ready(d);
                        break;
                    default:
                        break;
                }
            }
            break;

        case GATT_EVENT_NOTIFICATION: {
            handle = gatt_event_notification_get_handle(packet);
            uint16_t value_handle = gatt_event_notification_get_value_handle(packet);
            uint16_t value_len = gatt_event_notification_get_value_length(packet);
            const uint8_t* value = gatt_event_notification_get_value(packet);
            d = uni_hid_device_get_instance_for_connection_handle(handle);
            if (!d)
                break;
            sw2_instance_t* ins = get_instance(d);
            if (value_handle == ins->cmd_notify_handle) {
                sw2_handle_command_response(d, value, value_len);
            } else if (sw2_is_input_notification(ins, value_handle, value_len)) {
                sw2_parse_input_notify(d, value_handle, value, value_len);
            }
            break;
        }
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
        case GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT:
        default:
            break;
    }
}

bool uni_hid_parser_switch2_is_ble_device(const uni_hid_device_t* d) {
    if (!d || !uni_hid_device_has_controller_type((uni_hid_device_t*)d))
        return false;
    switch (d->controller_type) {
        case CONTROLLER_TYPE_Switch2ProController:
        case CONTROLLER_TYPE_Switch2JoyConLeft:
        case CONTROLLER_TYPE_Switch2JoyConRight:
            return true;
        default:
            return false;
    }
}

bool uni_hid_parser_switch2_needs_pair(const uni_hid_device_t* d) {
    if (!d)
        return false;
    if (d->product_id != UNI_SW2_PRO_PID && d->product_id != UNI_SW2_JOYCON_L_PID &&
        d->product_id != UNI_SW2_JOYCON_R_PID)
        return false;
    const sw2_instance_t* ins = (const sw2_instance_t*)&d->parser_data[0];
    return ins->needs_pair;
}

bool uni_hid_parser_switch2_is_ready(const uni_hid_device_t* d) {
    if (!d || !uni_hid_parser_switch2_is_ble_device(d))
        return false;
    const sw2_instance_t* ins = (const sw2_instance_t*)&d->parser_data[0];
    return ins->state == SW2_STATE_READY;
}

static bool sw2_parse_mfg_data(const uint8_t* data, uint8_t len, uint16_t* pid, bd_addr_t reconnect_mac,
                             bool* needs_pair) {
    if (len < 18)
        return false;
    if (little_endian_read_16(data, 0) != SW2_MFG_COMPANY_ID)
        return false;
    if (little_endian_read_16(data, 5) != UNI_SW2_NINTENDO_VID)
        return false;
    uint16_t product = little_endian_read_16(data, 7);
    if (product != UNI_SW2_JOYCON_R_PID && product != UNI_SW2_JOYCON_L_PID && product != UNI_SW2_PRO_PID)
        return false;
    memcpy(reconnect_mac, &data[12], 6);
    bd_addr_t zero = {0};
    *needs_pair = (memcmp(reconnect_mac, zero, 6) == 0);
    *pid = product;
    return true;
}

static bool sw2_mfg_in_advertisement(const uint8_t* adv_data, uint8_t adv_len, uint16_t* pid, bd_addr_t reconnect_mac,
                                     bool* needs_pair) {
    ad_context_t context;
    for (ad_iterator_init(&context, adv_len, (uint8_t*)adv_data); ad_iterator_has_more(&context);
         ad_iterator_next(&context)) {
        if (ad_iterator_get_data_type(&context) != BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA)
            continue;
        uint8_t mfg_len = ad_iterator_get_data_len(&context);
        const uint8_t* mfg = ad_iterator_get_data(&context);
        if (sw2_parse_mfg_data(mfg, mfg_len, pid, reconnect_mac, needs_pair))
            return true;
    }
    return false;
}

bool uni_bt_le_switch2_handle_advertisement(const uint8_t* packet, uint16_t size) {
    ARG_UNUSED(size);

    bd_addr_t addr;
    bd_addr_t reconnect_mac;
    uint16_t pid = 0;
    bool needs_pair = false;

    const uint8_t* ad_data = gap_event_advertising_report_get_data(packet);
    uint8_t ad_len = gap_event_advertising_report_get_data_length(packet);
    if (!sw2_mfg_in_advertisement(ad_data, ad_len, &pid, reconnect_mac, &needs_pair))
        return false;

    if (!needs_pair) {
        bd_addr_t local;
        gap_local_bd_addr(local);
        if (memcmp(reconnect_mac, local, 6) != 0)
            return false;
    }

    gap_event_advertising_report_get_address(packet, addr);
    {
        uni_hid_device_t* ex = uni_hid_device_get_instance_for_address(addr);
        if (ex) {
            if (uni_bt_conn_is_connected(&ex->conn))
                return true;
            if (uni_bt_conn_get_state(&ex->conn) != UNI_BT_CONN_STATE_DEVICE_READY) {
                if (ex->conn.protocol == UNI_BT_CONN_PROTOCOL_BLE &&
                    (uni_hid_parser_switch2_is_ble_device(ex) || ex->product_id == pid)) {
                    logi("Switch2: drop stale slot %s for reconnect\n", bd_addr_to_str(addr));
                    uni_hid_device_disconnect(ex);
                    uni_hid_device_delete(ex);
                } else {
                    return true;
                }
            }
        }
    }

    bd_addr_type_t addr_type = gap_event_advertising_report_get_address_type(packet);
    uint8_t rssi = gap_event_advertising_report_get_rssi(packet);

    logi("Switch2: found pid=0x%04x addr=%s type=%u %s\n", pid, bd_addr_to_str(addr), addr_type,
         needs_pair ? "(pair)" : "(reconnect)");

    if (uni_hid_device_on_device_discovered(addr, "Switch2", UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD,
                                            rssi) != UNI_ERROR_SUCCESS)
        return true;

    uni_hid_device_t* d = uni_hid_device_create(addr);
    if (!d) {
        loge("Switch2: no device slots\n");
        return true;
    }

    memset(d->parser_data, 0, sizeof(d->parser_data));
    sw2_instance_t* ins = get_instance(d);
    ins->product_id = pid;
    ins->needs_pair = needs_pair;
    sw2_set_default_calibration(ins);

    uni_hid_device_set_vendor_id(d, UNI_SW2_NINTENDO_VID);
    uni_hid_device_set_product_id(d, pid);
    uni_hid_device_guess_controller_type_from_pid_vid(d);
    d->sdp_query_type = SDP_QUERY_NOT_NEEDED;

    uni_hid_device_set_cod(d, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD);
    uni_hid_device_set_name(d, "Switch2");
    uni_bt_conn_set_protocol(&d->conn, UNI_BT_CONN_PROTOCOL_BLE);
    uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_DEVICE_DISCOVERED);
    d->conn.rssi = rssi;

    if (needs_pair) {
        gap_delete_bonding(addr_type, addr);
        logi("Switch2: cleared stored bond for SYNC pair\n");
    }

    gap_stop_scan();
    gap_connect(addr, addr_type);
    return true;
}

void uni_hid_parser_switch2_on_le_connected(uni_hid_device_t* d) {
    logi("Switch2: starting GATT (BLE SMP deferred until needed)\n");
    uni_hid_parser_switch2_setup(d);
}

void uni_hid_parser_switch2_on_encrypted(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    if (ins->waiting_encryption) {
        ins->waiting_encryption = false;
        logi("Switch2: SMP complete — retry CCCD (state=%s)\n", sw2_state_name(ins->state));
        switch (ins->state) {
            case SW2_STATE_ENABLE_CMD_NOTIFY:
                sw2_write_cccd_notify(d, SW2_HANDLE_CMD_NOTIFY_CCCD);
                break;
            case SW2_STATE_ENABLE_INPUT_NOTIFY:
                ins->init_step = 0;
                sw2_write_cccd_notify(d, SW2_HANDLE_INPUT_NOTIFY_CCCD);
                break;
            default:
                ins->service_count = 0;
                ins->service_discover_idx = 0;
                sw2_set_state(ins, SW2_STATE_DISCOVER_SERVICE);
                gatt_client_discover_primary_services(sw2_gatt_handler, d->conn.handle);
                break;
        }
        return;
    }
    if (ins->state != SW2_STATE_IDLE) {
        logi("Switch2: link encrypted (GATT active, state=%s) — input notify unblocked\n",
             sw2_state_name(ins->state));
        return;
    }
    uni_hid_parser_switch2_setup(d);
}

void uni_hid_parser_switch2_setup(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    const bool needs_pair = ins->needs_pair;
    const uint16_t product_id = ins->product_id ? ins->product_id : d->product_id;
    uni_hid_parser_switch2_teardown(d);
    logi("Switch2: setup GATT pid=0x%04x pair=%d handle=0x%04x\n", product_id, needs_pair ? 1 : 0,
         d->conn.handle);
    memset(ins, 0, sizeof(*ins));
    ins->product_id = product_id;
    ins->needs_pair = needs_pair;
    sw2_set_default_calibration(ins);
    sw2_set_state(ins, SW2_STATE_DISCOVER_SERVICE);
    gatt_client_discover_primary_services(sw2_gatt_handler, d->conn.handle);
}

void uni_hid_parser_switch2_teardown(uni_hid_device_t* d) {
    if (!d || !uni_hid_parser_switch2_is_ble_device(d))
        return;
    sw2_break_joycon_pair(d);
    if (sw2_is_joycon_pid(d->product_id))
        uni_bt_enable_new_connections_unsafe(true);
    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx >= 0 && idx < CONFIG_BLUEPAD32_MAX_DEVICES) {
        sw2_first_input_logged[idx] = 0;
        memset(sw2_last_btn_dword[idx], 0, sizeof(sw2_last_btn_dword[idx]));
    }
    sw2_stop_notify_listener_for_device(d);
    sw2_stop_keepalive_timer(d);
    if (sw2_merge_emit_scheduled) {
        btstack_run_loop_remove_timer(&sw2_merge_emit_timer);
        sw2_merge_emit_scheduled = false;
    }
    sw2_disarm_cmd_timeout(get_instance(d));
}

void uni_hid_parser_switch2_init_report(uni_hid_device_t* d) { ARG_UNUSED(d); }

// Switch 2 Pro BLE (and wired USB) use 3 raw button bytes in SwitchPro::InReport — see Switch2ProHost.cpp.
static void sw2_apply_pro2_raw_buttons(uni_gamepad_t* gp, uint8_t br, uint8_t bm, uint8_t bl) {
    gp->buttons |= (br & (1u << 2)) ? BUTTON_X : 0;
    gp->buttons |= (br & (1u << 0)) ? BUTTON_A : 0;
    gp->buttons |= (br & (1u << 1)) ? BUTTON_B : 0;
    gp->buttons |= (br & (1u << 3)) ? BUTTON_Y : 0;
    gp->buttons |= (br & (1u << 4)) ? BUTTON_SHOULDER_R : 0;
    gp->misc_buttons |= (br & (1u << 6)) ? MISC_BUTTON_START : 0;
    gp->buttons |= (br & (1u << 7)) ? BUTTON_THUMB_R : 0;

    const bool zl_on = (bm & (1u << 5)) != 0;
    const bool zr_on = (br & (1u << 5)) != 0;

    gp->misc_buttons |= (bm & (1u << 6)) ? MISC_BUTTON_SELECT : 0;
    gp->buttons |= (bm & (1u << 7)) ? BUTTON_THUMB_L : 0;

    gp->dpad |= (bm & (1u << 0)) ? DPAD_DOWN : 0;
    gp->dpad |= (bm & (1u << 2)) ? DPAD_LEFT : 0;
    gp->dpad |= (bm & (1u << 1)) ? DPAD_RIGHT : 0;
    gp->dpad |= (bm & (1u << 3)) ? DPAD_UP : 0;
    gp->buttons |= (bm & (1u << 4)) ? BUTTON_SHOULDER_L : 0;

    gp->misc_buttons |= (bl & (1u << 0)) ? MISC_BUTTON_SYSTEM : 0;
    /* Switch 2 Pro USB capture: GL bit3, GR bit2, Chat bit4 (were intentionally unmapped). */
    gp->misc_buttons |= (bl & (1u << 3)) ? MISC_BUTTON_SL : 0;    // GL → SL
    gp->misc_buttons |= (bl & (1u << 2)) ? MISC_BUTTON_SR : 0;    // GR → SR
    gp->misc_buttons |= (bl & (1u << 4)) ? MISC_BUTTON_CHAT : 0;  // Chat / C

    if (zl_on)
        gp->brake = 1023;
    if (zr_on)
        gp->throttle = 1023;
}

// BLE 63-byte notify: composed button dword @4 (TommyWabg). USB InReport uses 3 raw bytes after rid.
static int16_t sw2_pro2_inreport_base(const uint8_t* report, uint16_t len) {
    if (len < 13)
        return -1;
    if (len == 63)
        return -1;
    if (report[0] == 0x09 || report[0] == 0x30 || report[0] == 0x31)
        return 2;
    return -1;
}

static void sw2_parse_pro2_inreport(uni_gamepad_t* gp, sw2_instance_t* ins, const uint8_t* report, uint16_t len) {
    const int16_t base = sw2_pro2_inreport_base(report, len);
    if (base < 0 || len < (uint16_t)(base + 10))
        return;

    const uint8_t br = report[base];
    const uint8_t bm = report[base + 1];
    const uint8_t bl = report[base + 2];
    sw2_apply_pro2_raw_buttons(gp, br, bm, bl);
    sw2_apply_sticks(gp, ins, report, (uint8_t)(base + 3), (uint8_t)(base + 6));
}

// SW2 Pro BLE: composed button dword at report offset 4 (PID 0x2069, verified).
// Mapped for OGX-Mini PS3 output (Nintendo labels on wire → Xbox-oriented bluepad32 fields).
static void sw2_apply_composed_buttons(uni_gamepad_t* gp, uint32_t buttons) {
    buttons &= 0x03ffffffu;

    gp->buttons |= (buttons & 0x08) ? BUTTON_B : 0;  // Nintendo A (east)
    gp->buttons |= (buttons & 0x04) ? BUTTON_A : 0;  // Nintendo B (south)
    gp->buttons |= (buttons & 0x02) ? BUTTON_Y : 0;  // Nintendo X (west) → Xbox Y
    gp->buttons |= (buttons & 0x01) ? BUTTON_X : 0;  // Nintendo Y (north) → Xbox X
    gp->buttons |= (buttons & 0x40) ? BUTTON_SHOULDER_R : 0;
    gp->buttons |= (buttons & 0x80) ? BUTTON_TRIGGER_R : 0;
    gp->buttons |= (buttons & 0x400000) ? BUTTON_SHOULDER_L : 0;

    // D-pad directions ↔ system / stick-click (per PS3 layout on OGX-Mini)
    gp->dpad |= (buttons & 0x10000) ? DPAD_DOWN : 0;   // − → d-pad down
    gp->misc_buttons |= (buttons & 0x100) ? MISC_BUTTON_SELECT : 0;  // d-pad down → −
    gp->dpad |= (buttons & 0x20000) ? DPAD_UP : 0;     // + → d-pad up
    gp->misc_buttons |= (buttons & 0x200) ? MISC_BUTTON_START : 0;    // d-pad up → +
    gp->dpad |= (buttons & 0x40000) ? DPAD_RIGHT : 0;  // R3 → d-pad right
    gp->buttons |= (buttons & 0x400) ? BUTTON_THUMB_R : 0;           // d-pad right → R3
    gp->dpad |= (buttons & 0x80000) ? DPAD_LEFT : 0;   // L3 → d-pad left
    gp->buttons |= (buttons & 0x800) ? BUTTON_THUMB_L : 0;           // d-pad left → L3

    // Home/Capture: full-report dword (0x100000/0x200000) or TommyWabg composed (0x1000/0x2000).
    gp->misc_buttons |= (buttons & (0x100000u | 0x1000u)) ? MISC_BUTTON_SYSTEM : 0;  // Home → PS
    gp->misc_buttons |= (buttons & (0x200000u | 0x2000u)) ? MISC_BUTTON_CAPTURE : 0;

    /* Chat / C (bit 14). Joy-Con rail SL/SR (bits 5/4). Pro grip GL/GR (bits 25/24). */
    gp->misc_buttons |= (buttons & 0x4000u) ? MISC_BUTTON_CHAT : 0;
    gp->misc_buttons |= (buttons & (0x20u | 0x2000000u)) ? MISC_BUTTON_SL : 0;  // R_SL | GL
    gp->misc_buttons |= (buttons & (0x10u | 0x1000000u)) ? MISC_BUTTON_SR : 0;  // R_SR | GR

    if (buttons & 0x800000)
        gp->brake = 1023;
    if (buttons & 0x80)
        gp->throttle = 1023;
}

static uint32_t sw2_compose_buttons_from_raw(const uint8_t b1, const uint8_t b2, const uint8_t b3) {
    uint32_t buttons = 0;
    if (b1 & 0x01)
        buttons |= 0x00000004u;
    if (b1 & 0x02)
        buttons |= 0x00000008u;
    if (b1 & 0x04)
        buttons |= 0x00000001u;
    if (b1 & 0x08)
        buttons |= 0x00000002u;
    if (b1 & 0x10)
        buttons |= 0x00000080u;
    if (b1 & 0x20)
        buttons |= 0x00000040u;
    if (b1 & 0x40)
        buttons |= 0x00000200u;
    if (b2 & 0x01)
        buttons |= 0x00010000u;
    if (b2 & 0x02)
        buttons |= 0x00040000u;
    if (b2 & 0x04)
        buttons |= 0x00080000u;
    if (b2 & 0x08)
        buttons |= 0x00020000u;
    if (b2 & 0x10)
        buttons |= 0x00800000u;
    if (b2 & 0x20)
        buttons |= 0x00400000u;
    if (b3 & 0x01)
        buttons |= 0x00001000u;
    if (b3 & 0x02)
        buttons |= 0x00002000u;
    if (b3 & 0x04)
        buttons |= 0x01000000u;  // GR → SR
    if (b3 & 0x08)
        buttons |= 0x02000000u;  // GL → SL
    if (b3 & 0x10)
        buttons |= 0x00004000u;  // Chat / C
    return buttons;
}

static void sw2_parse_gc_style_report(uni_gamepad_t* gp, sw2_instance_t* ins, const uint8_t* report, uint16_t len) {
    if (len < 11)
        return;
    uint32_t buttons = sw2_compose_buttons_from_raw(report[2], report[3], report[4]) & 0x03ffffffu;
    sw2_apply_composed_buttons(gp, buttons);
    sw2_apply_sticks(gp, ins, report, 5, 8);
    if (len > 13) {
        if (report[12] >= 128)
            gp->brake = (uint16_t)((report[12] * 1023) / 255);
        if (report[13] >= 128)
            gp->throttle = (uint16_t)((report[13] * 1023) / 255);
    }
}

// TommyWabg ControllerInputData else-branch: timer @0, buttons @4, sticks @10/13, IMU @48.
static uint8_t sw2_full_report_base(uint16_t len) {
    ARG_UNUSED(len);
    return 0;
}

static void sw2_latch_home_button(uni_gamepad_t* gp, sw2_instance_t* ins) {
    const bool home = (gp->misc_buttons & MISC_BUTTON_SYSTEM) != 0;
    if (home)
        ins->home_latch_frames = SW2_HOME_LATCH_FRAMES;
    if (ins->home_latch_frames > 0) {
        gp->misc_buttons |= MISC_BUTTON_SYSTEM;
        if (!home)
            ins->home_latch_frames--;
    }
}

static void sw2_apply_left_stick_only(uni_gamepad_t* gp, sw2_instance_t* ins, const uint8_t* report, uint8_t off) {
    int16_t lx, ly;
    sw2_get_stick_xy(&report[off], &lx, &ly);
    int32_t nlx = sw2_clamp_axis(sw2_apply_stick_axis(lx, ins->stick_l_center[0], ins->stick_l_max[0],
                                                      ins->stick_l_min[0]));
    int32_t nly = sw2_clamp_axis(-sw2_apply_stick_axis(ly, ins->stick_l_center[1], ins->stick_l_max[1],
                                                       ins->stick_l_min[1]));
    gp->axis_x = (int16_t)nlx;
    gp->axis_y = (int16_t)nly;
}

static void sw2_apply_right_stick_only(uni_gamepad_t* gp, sw2_instance_t* ins, const uint8_t* report, uint8_t off) {
    int16_t rx, ry;
    sw2_get_stick_xy(&report[off], &rx, &ry);
    int32_t nrx = sw2_clamp_axis(sw2_apply_stick_axis(rx, ins->stick_r_center[0], ins->stick_r_max[0],
                                                      ins->stick_r_min[0]));
    int32_t nry = sw2_clamp_axis(-sw2_apply_stick_axis(ry, ins->stick_r_center[1], ins->stick_r_max[1],
                                                       ins->stick_r_min[1]));
    gp->axis_rx = (int16_t)nrx;
    gp->axis_ry = (int16_t)nry;
}

static void sw2_parse_full_report(uni_gamepad_t* gp, sw2_instance_t* ins, const uint8_t* report, uint16_t len,
                                  uint16_t pid) {
    const uint8_t base = sw2_full_report_base(len);
    if (len >= (uint16_t)(base + 8)) {
        uint32_t buttons = little_endian_read_32(report, base + 4) & 0x03ffffffu;
        sw2_apply_composed_buttons(gp, buttons);
    }
    if (len >= (uint16_t)(base + 16)) {
        if (pid == UNI_SW2_JOYCON_L_PID)
            sw2_apply_left_stick_only(gp, ins, report, (uint8_t)(base + 10));
        else if (pid == UNI_SW2_JOYCON_R_PID)
            sw2_apply_right_stick_only(gp, ins, report, (uint8_t)(base + 13));
        else
            sw2_apply_sticks(gp, ins, report, (uint8_t)(base + 10), (uint8_t)(base + 13));
    }
    /* IMU @48 for Pro 2 and Joy-Con 2 (same 63-byte composed layout). */
    if (len >= (uint16_t)(base + 60)) {
        gp->accel[0] = sw2_decode_s16(&report[base + 48]);
        gp->accel[1] = sw2_decode_s16(&report[base + 50]);
        gp->accel[2] = sw2_decode_s16(&report[base + 52]);
        gp->gyro[0] = sw2_decode_s16(&report[base + 54]);
        gp->gyro[1] = sw2_decode_s16(&report[base + 56]);
        gp->gyro[2] = sw2_decode_s16(&report[base + 58]);
    }
}

static void sw2_apply_sticks(uni_gamepad_t* gp, sw2_instance_t* ins, const uint8_t* report, uint8_t left_off,
                             uint8_t right_off) {
    int16_t lx, ly, rx, ry;
    sw2_get_stick_xy(&report[left_off], &lx, &ly);
    sw2_get_stick_xy(&report[right_off], &rx, &ry);

    int32_t nlx = sw2_clamp_axis(sw2_apply_stick_axis(lx, ins->stick_l_center[0], ins->stick_l_max[0],
                                                      ins->stick_l_min[0]));
    int32_t nly = sw2_clamp_axis(-sw2_apply_stick_axis(ly, ins->stick_l_center[1], ins->stick_l_max[1],
                                                       ins->stick_l_min[1]));
    int32_t nrx = sw2_clamp_axis(sw2_apply_stick_axis(rx, ins->stick_r_center[0], ins->stick_r_max[0],
                                                      ins->stick_r_min[0]));
    int32_t nry = sw2_clamp_axis(-sw2_apply_stick_axis(ry, ins->stick_r_center[1], ins->stick_r_max[1],
                                                       ins->stick_r_min[1]));

    gp->axis_x = (int16_t)nlx;
    gp->axis_y = (int16_t)nly;
    gp->axis_rx = (int16_t)nrx;
    gp->axis_ry = (int16_t)nry;
}

void uni_hid_parser_switch2_parse_input_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (len < 8)
        return;

    sw2_instance_t* ins = get_instance(d);
    uni_controller_t* ctl = &d->controller;
    uni_gamepad_t* gp = &ctl->gamepad;
    const uint16_t pid = ins->product_id ? ins->product_id : d->product_id;

    memset(gp, 0, sizeof(*gp));

    if (pid == 0x2073u) {
        sw2_parse_gc_style_report(gp, ins, report, len);
    } else if (pid == UNI_SW2_PRO_PID && sw2_pro2_inreport_base(report, len) >= 0) {
        sw2_parse_pro2_inreport(gp, ins, report, len);
    } else {
        sw2_parse_full_report(gp, ins, report, len, pid);
    }

    sw2_latch_home_button(gp, ins);
    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_switch2_set_player_leds(uni_hid_device_t* d, uint8_t player) {
    if (!d || player < 1 || player > 8)
        return;
    if (get_instance(d)->state != SW2_STATE_READY)
        return;
    uint8_t val[4] = {sw2_led_pattern[player - 1], 0, 0, 0};
    sw2_send_command(d, SW2_CMD_LEDS, SW2_SUBCMD_LEDS_SET_PLAYER, val, sizeof(val));
}

static void sw2_pack_vibration(uint8_t* out5, uint16_t freq, uint16_t amp) {
    uint64_t value = 0;
    value |= (freq & 0x1ff);
    value |= ((uint64_t)(amp & 0x3ff)) << 10;
    value |= ((uint64_t)(0x1e1 & 0x1ff)) << 20;
    value |= ((uint64_t)(amp & 0x3ff)) << 30;
    little_endian_store_32(out5, 0, (uint32_t)value);
    out5[4] = (uint8_t)(value >> 32);
}

// Pro 2: 0x00 + 16-byte rumble block (R) + 16-byte block (L), TommyWabg / Bit-Axis format.
static void sw2_build_pro_vibration_block(uint8_t* out16, uint8_t packet_id, const uint8_t motor[5]) {
    out16[0] = (uint8_t)(0x50 + (packet_id & 0x0f));
    memcpy(&out16[1], motor, 5);
    memcpy(&out16[6], motor, 5);
    memcpy(&out16[11], motor, 5);
}

void uni_hid_parser_switch2_send_keepalive(uni_hid_device_t* d) {
    sw2_instance_t* ins = get_instance(d);
    if (ins->state != SW2_STATE_READY || ins->vibration_handle == 0)
        return;
    // Reference drivers send idle rumble ~every 5 ms; zero amp is fine.
    uni_hid_parser_switch2_play_dual_rumble(d, 0, 0, 0, 0);
}

void uni_hid_parser_switch2_play_dual_rumble(uni_hid_device_t* d, uint16_t start_delay_ms, uint16_t duration_ms,
                                             uint8_t weak_magnitude, uint8_t strong_magnitude) {
    ARG_UNUSED(start_delay_ms);
    ARG_UNUSED(duration_ms);
    sw2_instance_t* ins = get_instance(d);
    if (ins->state != SW2_STATE_READY || ins->vibration_handle == 0)
        return;

    uint8_t motor[5];
    uint16_t amp = ((uint16_t)weak_magnitude + strong_magnitude) / 2;
    amp = (uint16_t)((amp * 4) & 0x3ff);
    sw2_pack_vibration(motor, 0x0e1, amp);

    uint8_t pid = ins->vibration_packet_id & 0x0f;
    uint8_t payload[33];
    uint16_t payload_len;

    if (ins->product_id == UNI_SW2_PRO_PID) {
        payload[0] = 0x00;
        sw2_build_pro_vibration_block(&payload[1], pid, motor);
        sw2_build_pro_vibration_block(&payload[17], pid, motor);
        payload_len = 33;
    } else {
        payload[0] = 0x00;
        payload[1] = (uint8_t)(0x50 + pid);
        memcpy(&payload[2], motor, 5);
        memcpy(&payload[7], motor, 5);
        memcpy(&payload[12], motor, 5);
        payload_len = 17;
    }

    gatt_client_write_value_of_characteristic_without_response(d->conn.handle, ins->vibration_handle, payload_len,
                                                               payload);
    ins->vibration_packet_id++;
}

bool uni_hid_parser_switch2_is_joycon_pair_secondary(const uni_hid_device_t* d) {
    if (!d)
        return false;
    return get_instance((uni_hid_device_t*)d)->pair_role == SW2_PAIR_SECONDARY;
}

bool uni_hid_parser_switch2_keepalive_timer_active(const uni_hid_device_t* d) {
    if (!d)
        return false;
    const int idx = uni_hid_device_get_idx_for_instance(d);
    return idx >= 0 && idx < CONFIG_BLUEPAD32_MAX_DEVICES && sw2_keepalive_active[idx];
}

int uni_hid_parser_switch2_get_gamepad_output_idx(const uni_hid_device_t* d) {
    if (!d)
        return -1;
    const sw2_instance_t* ins = get_instance((uni_hid_device_t*)d);
    if (ins->pair_output_idx >= 0)
        return ins->pair_output_idx;
    return uni_hid_device_get_idx_for_instance(d);
}

int uni_hid_parser_switch2_get_pair_partner_idx(const uni_hid_device_t* d) {
    if (!d)
        return -1;
    const sw2_instance_t* ins = get_instance((uni_hid_device_t*)d);
    if (ins->pair_role == SW2_PAIR_NONE)
        return -1;
    return ins->pair_partner_idx;
}
