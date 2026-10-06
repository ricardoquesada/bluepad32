// SPDX-License-Identifier: Apache-2.0
// SInput HID gamepads (Handheld Legend). Spec: https://docs.handheldlegend.com/s/sinput
//
// Input: the fixed-layout 64-byte input report (ID 0x01): buttons, sticks, triggers, battery, IMU.
// Commands (output report 0x03, response as input report 0x02): haptics (0x01, type 2: ERM), feature discovery
// (0x02), player LED (0x03), RGB (0x04). Not yet implemented: touchpads, type 1 (precise) haptics.
//
// Byte offsets follow SDL's SInput HIDAPI driver (SDL_hidapi_sinput.c). Bluepad32's BLE path delivers the report
// with the Report ID as report[0], so the offsets below include it.

#include "parser/uni_hid_parser_sinput.h"

#include <stdio.h>
#include <string.h>

#include "controller/uni_controller.h"
#include "parser/uni_hid_parser_rumble.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

#define SINPUT_REPORT_ID_INPUT 0x01
#define SINPUT_REPORT_MIN_LEN 19  // Up to and including right trigger.

#define IDX_PLUG_STATUS 1
#define IDX_CHARGE_LEVEL 2
#define IDX_BUTTONS_0 3
#define IDX_BUTTONS_1 4
#define IDX_BUTTONS_2 5
#define IDX_BUTTONS_3 6
#define IDX_LEFT_X 7
#define IDX_LEFT_Y 9
#define IDX_RIGHT_X 11
#define IDX_RIGHT_Y 13
#define IDX_LEFT_TRIGGER 15
#define IDX_RIGHT_TRIGGER 17
// IMU: uint32 timestamp (us, unused: uni_gamepad_t has no timestamp), then int16 accel X/Y/Z and gyro X/Y/Z.
#define IDX_ACCEL_X 23
#define IDX_ACCEL_Y 25
#define IDX_ACCEL_Z 27
#define IDX_GYRO_X 29
#define IDX_GYRO_Y 31
#define IDX_GYRO_Z 33
#define SINPUT_REPORT_IMU_LEN 35  // Up to and including gyro Z.

// buttons[0]
// Face buttons in A, B, X, Y order (bit 0 = south/A). SDL's driver names bit 0 "east" and bit 1 "south", but its
// mapping string is a:b0,b:b1,x:b2,y:b3, i.e. bit 0 is A. Confirmed on hardware against ESP32-BLE-Gamepad's SInput
// mode.
#define B0_SOUTH 0x01
#define B0_EAST 0x02
#define B0_WEST 0x04
#define B0_NORTH 0x08
#define B0_DPAD_UP 0x10
#define B0_DPAD_DOWN 0x20
#define B0_DPAD_LEFT 0x40
#define B0_DPAD_RIGHT 0x80
// buttons[1]
#define B1_LEFT_STICK 0x01
#define B1_RIGHT_STICK 0x02
#define B1_LEFT_BUMPER 0x04
#define B1_RIGHT_BUMPER 0x08
#define B1_LEFT_TRIGGER 0x10
#define B1_RIGHT_TRIGGER 0x20
// buttons[2]
#define B2_START 0x01
#define B2_BACK 0x02
#define B2_GUIDE 0x04
#define B2_CAPTURE 0x08

// Plug status values.
#define PLUG_NO_BATTERY 1
#define PLUG_CHARGING 2
#define PLUG_CHARGED 3
#define PLUG_ON_BATTERY 4

#define SINPUT_REPORT_ID_COMMAND_IN 0x02
#define SINPUT_REPORT_ID_COMMAND_OUT 0x03
#define SINPUT_CMD_FEATURES 0x02
#define SINPUT_CMD_HAPTIC 0x01
// Haptic type 2, "ERM simulation": left amplitude, left brake, right amplitude, right brake. Like SDL, left is the
// low-frequency (strong) motor and right the high-frequency (weak) one.
#define SINPUT_HAPTIC_TYPE_ERM 0x02
#define SINPUT_CMD_PLAYER_LED 0x03
#define SINPUT_CMD_RGB 0x04
// Output report 0x03 is 48 bytes on the wire; the Report ID is passed separately, so the payload is 47.
#define SINPUT_CMD_PAYLOAD_LEN 47
// Feature response: report[0]=0x02, report[1]=0x02, report[2..3]=protocol version, report[4]=caps0, report[5]=caps1.
#define SINPUT_FEATURES_MIN_LEN 6
// report[8..9]=polling interval (us), report[10..11]=accel range (+/- g), report[12..13]=gyro range (+/- deg/s).
#define SINPUT_FEATURES_IMU_LEN 14
// Used when the device sets an IMU capability bit but reports a range of 0 (SDL's SINPUT_DEFAULT_*_SENS).
#define SINPUT_DEFAULT_ACCEL_RANGE_G 8
#define SINPUT_DEFAULT_GYRO_RANGE_DPS 2000
#define SINPUT_FEATURES_TIMEOUT_MS 2000

// BLE only allows one GATT write in flight, so commands go through a small paced queue.
#define SINPUT_CMD_QUEUE_LEN 4
#define SINPUT_CMD_MAX_ARGS 5
#define SINPUT_CMD_GAP_MS 30
#define SINPUT_CMD_RETRY_MS 50
#define SINPUT_CMD_MAX_RETRIES 20

typedef struct {
    uint8_t len;
    uint8_t data[1 + SINPUT_CMD_MAX_ARGS];  // command byte + arguments
} sinput_cmd_t;

typedef struct sinput_instance_s {
    btstack_timer_source_t pump_timer;
    btstack_timer_source_t features_timer;
    sinput_cmd_t queue[SINPUT_CMD_QUEUE_LEN];
    uint8_t head;
    uint8_t count;
    uint8_t retries;
    bool pump_scheduled;
    bool ready_done;
    bool features_valid;
    uint16_t protocol_version;
    uint8_t caps0;
    uint8_t caps1;
    uint16_t poll_us;
    uint16_t accel_range_g;
    uint16_t gyro_range_dps;
    // Raw int16 count -> m/s^2 and rad/s. 0 when the device has no such sensor (or no feature response yet).
    float accel_scale;
    float gyro_scale;
} sinput_instance_t;
_Static_assert(sizeof(sinput_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "SInput instance too big");

static sinput_instance_t* get_sinput_instance(uni_hid_device_t* d) {
    return (sinput_instance_t*)&d->parser_data[0];
}

static void schedule_pump(uni_hid_device_t* d, uint32_t delay_ms);

static void on_pump(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    sinput_instance_t* ins = get_sinput_instance(d);
    ins->pump_scheduled = false;
    if (ins->count == 0)
        return;

    const sinput_cmd_t* c = &ins->queue[ins->head];
    uint8_t payload[SINPUT_CMD_PAYLOAD_LEN] = {0};
    memcpy(payload, c->data, c->len);

    uint8_t status = hids_host_send_write_report(d->hids_cid, SINPUT_REPORT_ID_COMMAND_OUT, HID_REPORT_TYPE_OUTPUT,
                                                 payload, sizeof(payload));
    if (status == ERROR_CODE_COMMAND_DISALLOWED && ins->retries < SINPUT_CMD_MAX_RETRIES) {
        // A previous write is still in flight.
        ins->retries++;
        schedule_pump(d, SINPUT_CMD_RETRY_MS);
        return;
    }
    if (status != ERROR_CODE_SUCCESS)
        logi("SInput: command 0x%02x failed, status=%#x\n", c->data[0], status);

    ins->retries = 0;
    ins->head = (ins->head + 1) % SINPUT_CMD_QUEUE_LEN;
    ins->count--;
    if (ins->count > 0)
        schedule_pump(d, SINPUT_CMD_GAP_MS);
}

static void schedule_pump(uni_hid_device_t* d, uint32_t delay_ms) {
    sinput_instance_t* ins = get_sinput_instance(d);
    if (ins->pump_scheduled)
        return;
    ins->pump_scheduled = true;
    ins->pump_timer.process = &on_pump;
    ins->pump_timer.context = d;
    btstack_run_loop_set_timer(&ins->pump_timer, delay_ms);
    btstack_run_loop_add_timer(&ins->pump_timer);
}

// Returns false if the command was not queued (bad length, or the queue is full).
static bool enqueue_command(uni_hid_device_t* d, const uint8_t* data, uint8_t len) {
    sinput_instance_t* ins = get_sinput_instance(d);
    if (len == 0 || len > sizeof(ins->queue[0].data))
        return false;
    if (ins->count == SINPUT_CMD_QUEUE_LEN) {
        loge("SInput: command queue full, dropping command 0x%02x\n", data[0]);
        return false;
    }
    sinput_cmd_t* c = &ins->queue[(ins->head + ins->count) % SINPUT_CMD_QUEUE_LEN];
    c->len = len;
    memcpy(c->data, data, len);
    ins->count++;
    schedule_pump(d, 0);
    return true;
}

static void finish_setup(uni_hid_device_t* d) {
    sinput_instance_t* ins = get_sinput_instance(d);
    if (ins->ready_done)
        return;
    ins->ready_done = true;
    btstack_run_loop_remove_timer(&ins->features_timer);
    uni_hid_device_set_ready_complete(d);
}

static void on_features_timeout(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    logi("SInput: no feature response, continuing without it\n");
    finish_setup(d);
}

static inline int16_t read_s16(const uint8_t* data, int idx) {
    return (int16_t)(data[idx] | (data[idx + 1] << 8));
}

static inline uint16_t read_u16(const uint8_t* data, int idx) {
    return (uint16_t)(data[idx] | (data[idx + 1] << 8));
}

static void parse_features_response(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (len < SINPUT_FEATURES_MIN_LEN) {
        loge("SInput: feature response too short; got %d, want >= %d\n", len, SINPUT_FEATURES_MIN_LEN);
        return;
    }
    sinput_instance_t* ins = get_sinput_instance(d);
    ins->protocol_version = (uint16_t)(report[2] | (report[3] << 8));
    ins->caps0 = report[4];
    ins->caps1 = report[5];
    ins->features_valid = true;
    logi("SInput: protocol=%u caps0=0x%02x caps1=0x%02x\n", ins->protocol_version, ins->caps0, ins->caps1);

    if (len >= SINPUT_FEATURES_IMU_LEN) {
        ins->poll_us = read_u16(report, 8);
        ins->accel_range_g = read_u16(report, 10);
        ins->gyro_range_dps = read_u16(report, 12);
    }
    if (ins->caps0 & UNI_SINPUT_CAPS0_ACCEL) {
        if (ins->accel_range_g == 0)
            ins->accel_range_g = SINPUT_DEFAULT_ACCEL_RANGE_G;
        ins->accel_scale = UNI_STANDARD_GRAVITY * (float)ins->accel_range_g / 32768.0f;
    }
    if (ins->caps0 & UNI_SINPUT_CAPS0_GYRO) {
        if (ins->gyro_range_dps == 0)
            ins->gyro_range_dps = SINPUT_DEFAULT_GYRO_RANGE_DPS;
        ins->gyro_scale = UNI_DEG_TO_RAD * (float)ins->gyro_range_dps / 32768.0f;
    }
    if (ins->accel_scale != 0.0f || ins->gyro_scale != 0.0f)
        logi("SInput: IMU poll=%uus accel=+/-%ug gyro=+/-%udps\n", ins->poll_us, ins->accel_range_g,
             ins->gyro_range_dps);
    finish_setup(d);
}

// int16 stick -> Bluepad32 -512..511.
static inline int32_t stick_to_axis(int16_t v) {
    return v >> 6;
}

// int16 trigger -> Bluepad32 0..1023.
// ESP32-BLE-Gamepad's SInput mode sends 0 (released) .. 32767 (fully pressed), and this is what we assume. Negative
// values are clamped to released, so a device that idles at INT16_MIN (SDL's default for absent triggers) also reads
// as released. TODO: confirm the range against the spec / a real SInput device.
static inline int32_t trigger_to_pedal(int16_t v) {
    if (v < 0)
        return 0;
    return v >> 5;
}

static uni_rumble_result_t send_haptic(uni_hid_device_t* d, uint8_t weak_magnitude, uint8_t strong_magnitude) {
    sinput_instance_t* ins = get_sinput_instance(d);
    if (ins->features_valid && !(ins->caps0 & UNI_SINPUT_CAPS0_RUMBLE)) {
        logd("SInput: device has no rumble\n");
        return UNI_RUMBLE_ERR;
    }
    const uint8_t cmd[] = {SINPUT_CMD_HAPTIC, SINPUT_HAPTIC_TYPE_ERM, strong_magnitude, 0, weak_magnitude, 0};
    // The queue paces the BLE writes itself. If it is full, let the rumble helper try again shortly.
    return enqueue_command(d, cmd, sizeof(cmd)) ? UNI_RUMBLE_OK : UNI_RUMBLE_RETRY_BLE;
}

static uni_rumble_result_t start_rumble_now(uni_hid_device_t* d,
                                            uint8_t weak_magnitude,
                                            uint8_t strong_magnitude,
                                            uint8_t trigger_left,
                                            uint8_t trigger_right) {
    ARG_UNUSED(trigger_left);
    ARG_UNUSED(trigger_right);
    return send_haptic(d, weak_magnitude, strong_magnitude);
}

static uni_rumble_result_t stop_rumble_now(uni_hid_device_t* d) {
    return send_haptic(d, 0, 0);
}

void uni_hid_parser_sinput_setup(struct uni_hid_device_s* d) {
    sinput_instance_t* ins = get_sinput_instance(d);
    // In case setup runs again on a live instance: unlink its timers before zeroing them.
    uni_hid_parser_sinput_deinit(d);
    memset(ins, 0, sizeof(*ins));
    uni_hid_parser_rumble_init(d, start_rumble_now, stop_rumble_now);

    // Ask for the device's features. The device is "ready" once it answers, or after a timeout for devices that don't.
    const uint8_t cmd = SINPUT_CMD_FEATURES;
    enqueue_command(d, &cmd, 1);

    ins->features_timer.process = &on_features_timeout;
    ins->features_timer.context = d;
    btstack_run_loop_set_timer(&ins->features_timer, SINPUT_FEATURES_TIMEOUT_MS);
    btstack_run_loop_add_timer(&ins->features_timer);
}

void uni_hid_parser_sinput_deinit(struct uni_hid_device_s* d) {
    if (!d)
        return;
    // Remove the command pump and feature timeout timers from BTstack's run-loop timer list before
    // uni_hid_device_delete() or uni_hid_device_setup() zeroes d->parser_data with memset.
    sinput_instance_t* ins = get_sinput_instance(d);
    btstack_run_loop_remove_timer(&ins->pump_timer);
    btstack_run_loop_remove_timer(&ins->features_timer);
    ins->pump_scheduled = false;
}

void uni_hid_parser_sinput_init_report(struct uni_hid_device_s* d) {
    // Called before every report, including command responses, so it must not wipe the gamepad state.
    // State reports overwrite every field they own.
    d->controller.klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_sinput_set_player_leds(struct uni_hid_device_s* d, uint8_t leds) {
    sinput_instance_t* ins = get_sinput_instance(d);
    if (ins->features_valid && !(ins->caps0 & UNI_SINPUT_CAPS0_PLAYER_LEDS)) {
        logd("SInput: device has no player LEDs\n");
        return;
    }
    uint8_t idx = 0;
    for (int i = 0; i < 8; i++) {
        if (leds & (1 << i)) {
            idx = (uint8_t)(i + 1);
            break;
        }
    }
    const uint8_t cmd[] = {SINPUT_CMD_PLAYER_LED, idx};
    enqueue_command(d, cmd, sizeof(cmd));
}

void uni_hid_parser_sinput_play_dual_rumble(struct uni_hid_device_s* d,
                                            uint16_t start_delay_ms,
                                            uint16_t duration_ms,
                                            uint8_t weak_magnitude,
                                            uint8_t strong_magnitude) {
    uni_hid_parser_rumble_play_dual(d, start_delay_ms, duration_ms, weak_magnitude, strong_magnitude, start_rumble_now,
                                    stop_rumble_now);
}

void uni_hid_parser_sinput_set_lightbar_color(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b) {
    sinput_instance_t* ins = get_sinput_instance(d);
    if (ins->features_valid && !(ins->caps1 & UNI_SINPUT_CAPS1_RGB)) {
        logd("SInput: device has no RGB\n");
        return;
    }
    const uint8_t cmd[] = {SINPUT_CMD_RGB, r, g, b};
    enqueue_command(d, cmd, sizeof(cmd));
}

bool uni_hid_parser_sinput_get_imu_config(struct uni_hid_device_s* d,
                                          uint16_t* poll_us,
                                          uint16_t* accel_range_g,
                                          uint16_t* gyro_range_dps) {
    sinput_instance_t* ins = get_sinput_instance(d);
    if (!ins->features_valid)
        return false;
    *poll_us = ins->poll_us;
    *accel_range_g = ins->accel_scale != 0.0f ? ins->accel_range_g : 0;
    *gyro_range_dps = ins->gyro_scale != 0.0f ? ins->gyro_range_dps : 0;
    return true;
}

bool uni_hid_parser_sinput_get_features(struct uni_hid_device_s* d,
                                        uint16_t* protocol_version,
                                        uint8_t* caps0,
                                        uint8_t* caps1) {
    sinput_instance_t* ins = get_sinput_instance(d);
    if (!ins->features_valid)
        return false;
    *protocol_version = ins->protocol_version;
    *caps0 = ins->caps0;
    *caps1 = ins->caps1;
    return true;
}

void uni_hid_parser_sinput_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    if (len >= 2 && report[0] == SINPUT_REPORT_ID_COMMAND_IN) {
        if (report[1] == SINPUT_CMD_FEATURES)
            parse_features_response(d, report, len);
        else
            logd("SInput: ignoring command response 0x%02x\n", report[1]);
        return;
    }
    if (len < 1 || report[0] != SINPUT_REPORT_ID_INPUT) {
        logd("SInput: ignoring report id=0x%02x len=%d\n", len ? report[0] : 0, len);
        return;
    }
    if (len < SINPUT_REPORT_MIN_LEN) {
        loge("SInput: report too short; got %d, want >= %d\n", len, SINPUT_REPORT_MIN_LEN);
        return;
    }

    uni_gamepad_t* gp = &d->controller.gamepad;

    const uint8_t b0 = report[IDX_BUTTONS_0];
    const uint8_t b1 = report[IDX_BUTTONS_1];
    const uint8_t b2 = report[IDX_BUTTONS_2];

    // SInput names buttons by position (south/east/west/north). Bluepad32 uses the Xbox layout:
    // A=south, B=east, X=west, Y=north.
    gp->buttons = 0;
    gp->buttons |= (b0 & B0_SOUTH) ? BUTTON_A : 0;
    gp->buttons |= (b0 & B0_EAST) ? BUTTON_B : 0;
    gp->buttons |= (b0 & B0_WEST) ? BUTTON_X : 0;
    gp->buttons |= (b0 & B0_NORTH) ? BUTTON_Y : 0;
    gp->buttons |= (b1 & B1_LEFT_BUMPER) ? BUTTON_SHOULDER_L : 0;
    gp->buttons |= (b1 & B1_RIGHT_BUMPER) ? BUTTON_SHOULDER_R : 0;
    gp->buttons |= (b1 & B1_LEFT_TRIGGER) ? BUTTON_TRIGGER_L : 0;
    gp->buttons |= (b1 & B1_RIGHT_TRIGGER) ? BUTTON_TRIGGER_R : 0;
    gp->buttons |= (b1 & B1_LEFT_STICK) ? BUTTON_THUMB_L : 0;
    gp->buttons |= (b1 & B1_RIGHT_STICK) ? BUTTON_THUMB_R : 0;

    gp->misc_buttons = 0;
    gp->misc_buttons |= (b2 & B2_START) ? MISC_BUTTON_START : 0;
    gp->misc_buttons |= (b2 & B2_BACK) ? MISC_BUTTON_SELECT : 0;
    gp->misc_buttons |= (b2 & B2_GUIDE) ? MISC_BUTTON_SYSTEM : 0;
    gp->misc_buttons |= (b2 & B2_CAPTURE) ? MISC_BUTTON_CAPTURE : 0;

    gp->dpad = 0;
    gp->dpad |= (b0 & B0_DPAD_UP) ? DPAD_UP : 0;
    gp->dpad |= (b0 & B0_DPAD_DOWN) ? DPAD_DOWN : 0;
    gp->dpad |= (b0 & B0_DPAD_LEFT) ? DPAD_LEFT : 0;
    gp->dpad |= (b0 & B0_DPAD_RIGHT) ? DPAD_RIGHT : 0;

    gp->axis_x = stick_to_axis(read_s16(report, IDX_LEFT_X));
    gp->axis_y = stick_to_axis(read_s16(report, IDX_LEFT_Y));
    gp->axis_rx = stick_to_axis(read_s16(report, IDX_RIGHT_X));
    gp->axis_ry = stick_to_axis(read_s16(report, IDX_RIGHT_Y));
    gp->brake = trigger_to_pedal(read_s16(report, IDX_LEFT_TRIGGER));
    gp->throttle = trigger_to_pedal(read_s16(report, IDX_RIGHT_TRIGGER));

    // Battery: 0..100 % -> 0..254. "No battery" (wired) leaves it as "not available".
    d->controller.klass = UNI_CONTROLLER_CLASS_GAMEPAD;
    d->controller.battery = 255;
    const uint8_t plug = report[IDX_PLUG_STATUS];
    if (plug == PLUG_NO_BATTERY) {
        d->controller.battery = 255;
    } else if (plug == PLUG_CHARGED) {
        d->controller.battery = 254;
    } else if (plug == PLUG_CHARGING || plug == PLUG_ON_BATTERY) {
        uint8_t pct = report[IDX_CHARGE_LEVEL];
        if (pct > 100)
            pct = 100;
        d->controller.battery = (uint8_t)(pct * 254 / 100);
    }

    // IMU, in Bluepad32's frame (X right, Y up, Z toward the player), which is SDL's sensor frame too: like SDL's
    // driver, canonical = (-x, +z, -y) for both sensors.
    memset(gp->accel, 0, sizeof(gp->accel));
    memset(gp->gyro, 0, sizeof(gp->gyro));
    if (len < SINPUT_REPORT_IMU_LEN)
        return;
    const sinput_instance_t* ins = get_sinput_instance(d);
    const float as = ins->accel_scale;
    const float gs = ins->gyro_scale;
    gp->accel[0] = -(float)read_s16(report, IDX_ACCEL_X) * as;
    gp->accel[1] = (float)read_s16(report, IDX_ACCEL_Z) * as;
    gp->accel[2] = -(float)read_s16(report, IDX_ACCEL_Y) * as;
    gp->gyro[0] = -(float)read_s16(report, IDX_GYRO_X) * gs;
    gp->gyro[1] = (float)read_s16(report, IDX_GYRO_Z) * gs;
    gp->gyro[2] = -(float)read_s16(report, IDX_GYRO_Y) * gs;
}

// Formats SInput protocol version, capability bitmasks, polling interval, and IMU sensor
// ranges once the asynchronous feature response (`0x02`) has populated `ins->features_valid`.
// Returns `0` with `buf[0] = '\0'` prior to receiving the feature response.
int uni_hid_parser_sinput_device_extra_info(const struct uni_hid_device_s* d, char* buf, size_t len) {
    if (!d || !buf || len == 0)
        return -1;
    const sinput_instance_t* ins = (const sinput_instance_t*)&d->parser_data[0];
    if (!ins->features_valid) {
        buf[0] = '\0';
        return 0;
    }
    return snprintf(buf, len, "protocol=%u, caps0=0x%02x, caps1=0x%02x, poll=%uus, accel=+/-%ug, gyro=+/-%udps",
                    ins->protocol_version, ins->caps0, ins->caps1, ins->poll_us, ins->accel_range_g,
                    ins->gyro_range_dps);
}
