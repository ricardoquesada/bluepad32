// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

/**
 * @file uni_hid_parser_steam.c
 * @brief BLE HID parser and GATT setup state machine for the Valve Steam
 * Controller (1st Gen / "Chell", 2015, VID `0x28de`, PID `0x1106`).
 *
 * Architectural Overview:
 * Unlike the 2nd-generation Steam Controller 2026 ("Triton"), the original 2015
 * Steam Controller flashed with BLE firmware exposes a standard BLE HOGP (HID
 * over GATT Profile, `0x1812`) service that streams input reports (`0x03`),
 * paired alongside Valve's custom GATT configuration service
 * (`100F6C32-1735-4313-B402-38567131E5F3`).
 *
 * When `uni_hid_parser_steam_setup()` runs, it discovers the custom report
 * characteristic (`100F6C34-1735-4313-B402-38567131E5F3`) and writes two
 * configuration commands:
 * 1. `cmd_clear_mappings` (`0x81`): Clears onboard button-to-keyboard/mouse
 *    bindings.
 * 2. `cmd_disable_lizard` (`0x87` write register): Disables "lizard mode"
 *    (cursor/mouse emulation on the trackpads), sets LED brightness, and
 *    writes `STEAM_GYRO_MODE_RAW_IMU` (`0x0018` = `SEND_RAW_ACCEL |
 *    SEND_RAW_GYRO`) to `STEAM_REG_GYRO_MODE` (`0x30`) so BLE input reports
 *    append raw 6-axis IMU sections.
 *
 * BLE Input Report Framing (Report ID `0x03`):
 * Bytes 0..3 form a 4-byte header (`[0]=0x03`, `[1]=0xc0`, `[2..3]` = 4-bit
 * report type `0x04` + 12-bit section presence bitmask `report_flags`).
 * Optional payload sections are packed sequentially in ascending bitmask order
 * starting at byte offset 4:
 *   - `0x0010` (`FLAG_BUTTONS`):     3 bytes (24-bit button/D-pad bitmask)
 *   - `0x0020` (`FLAG_TRIGGERS`):    2 bytes (uint8 L/R triggers)
 *   - `0x0040` (`FLAG_BUTTONS_EXT`): 3 bytes (extended buttons, skipped)
 *   - `0x0080` (`FLAG_THUMBSTICK`):  4 bytes (int16 LE X, Y)
 *   - `0x0100` (`FLAG_LEFT_PAD`):    4 bytes (int16 LE X, Y, skipped)
 *   - `0x0200` (`FLAG_RIGHT_PAD`):   4 bytes (int16 LE X, Y)
 *   - `0x0400` (`FLAG_IMU_ACCEL`):   6 bytes (int16 LE ax, ay, az; +/-2g)
 *   - `0x0800` (`FLAG_IMU_GYRO`):    6 bytes (int16 LE gx, gy, gz; +/-2000 dps)
 *   - `0x1000` (`FLAG_IMU_QUAT`):    8 bytes (orientation quaternion, skipped)
 *
 * Haptic Rumble Emulation (`STEAM_CMD_FORCEFEEDBAK` = `0x8f`):
 * The 2015 Steam Controller does not have spinning ERM rumble motors, and
 * `STEAM_CMD_HAPTIC_RUMBLE` (`0xeb`) is only supported on the Steam Deck.
 * Instead, it has dual voice-coil Linear Resonant Actuators (LRAs) beneath the
 * Left and Right trackpads driven by command `0x8f` (`ID_TRIGGER_HAPTIC_PULSE`).
 * Each 11-byte BLE feature report addresses one actuator (`0x01` = Left,
 * `0x00` = Right; swapped on wire for legacy reasons) with pulse ON duration
 * (`on_us`), OFF interval (`off_us`), repeat `count`, and `0 dB` gain.
 *
 * References:
 * - https://github.com/rodrigorc/steamctrl/blob/master/src/steamctrl.c
 * - https://elixir.bootlin.com/linux/latest/source/drivers/hid/hid-steam.c
 * - https://github.com/cvuchener/steamcontroller-linux-kernel/blob/master/hid-valve-sc.c
 * -
 * https://github.com/haxpor/sdl2-samples/blob/master/android-project/app/src/main/java/org/libsdl/app/HIDDeviceBLESteamController.java
 * - https://github.com/g3gg0/LegoRemote
 */

#include "parser/uni_hid_parser_steam.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "controller/uni_controller.h"
#include "hid_usage.h"
#include "parser/uni_hid_parser_rumble.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

// clang-format off
#define STEAM_CONTROLLER_FLAG_BUTTONS       0x0010
#define STEAM_CONTROLLER_FLAG_TRIGGERS      0x0020
#define STEAM_CONTROLLER_FLAG_BUTTONS_EXT   0x0040
#define STEAM_CONTROLLER_FLAG_THUMBSTICK    0x0080
#define STEAM_CONTROLLER_FLAG_LEFT_PAD      0x0100
#define STEAM_CONTROLLER_FLAG_RIGHT_PAD     0x0200
#define STEAM_CONTROLLER_FLAG_IMU_ACCEL     0x0400
#define STEAM_CONTROLLER_FLAG_IMU_GYRO      0x0800
#define STEAM_CONTROLLER_FLAG_IMU_QUAT      0x1000
// clang-format on

// Gyro mode bitmask for STEAM_REG_GYRO_MODE (0x30):
// 0x0008 = SEND_RAW_ACCEL, 0x0010 = SEND_RAW_GYRO
#define STEAM_GYRO_MODE_RAW_IMU 0x0018

// Steam Controller 2015 IMU scales:
// Accelerometer full-scale is +/-2g over int16_t (+/-32768 LSB -> 16384 LSB/g).
// Gyroscope full-scale is +/-2000 deg/s over int16_t (+/-32768 LSB -> 16.384 LSB/(deg/s)).
#define STEAM_ACCEL_SCALE ((2.0f * UNI_STANDARD_GRAVITY) / 32768.0f)
#define STEAM_GYRO_SCALE ((2000.0f * UNI_DEG_TO_RAD) / 32768.0f)

// 2015 Steam Controller LRA haptic actuator wire indices for STEAM_CMD_FORCEFEEDBAK (0x8f).
// Per Linux drivers/hid/hid-steam.c ("Left and right are swapped on this report for legacy reasons"):
// wire 0x00 addresses the Right trackpad LRA, and wire 0x01 addresses the Left trackpad LRA.
#define STEAM_HAPTIC_ACTUATOR_RIGHT 0x00
#define STEAM_HAPTIC_ACTUATOR_LEFT 0x01

// Carrier periods in microseconds for LRA square-wave rumble synthesis:
// - Left trackpad LRA ("strong" low-frequency motor): 10000 us (100 Hz, matching SC_RUMBLE_PERIOD)
// - Right trackpad LRA ("weak" high-frequency motor): 6250 us (160 Hz)
#define STEAM_RUMBLE_LEFT_PERIOD_US 10000u
#define STEAM_RUMBLE_RIGHT_PERIOD_US 6250u
#define STEAM_HAPTIC_CMD_LEN 11u

/**
 * @brief GATT configuration state machine for Steam Controller (2015) setup.
 */
typedef enum {
    STATE_QUERY_SERVICE,
    STATE_QUERY_CHARACTERISTIC_REPORT,
    STATE_QUERY_CLEAR_MAPPINGS,
    STATE_QUERY_DISABLE_LIZARD,
    STATE_QUERY_FORCEFEEDBACK,
    STATE_QUERY_END,
} steam_query_state_t;

// TODO: Can this be refactored to use `hids_host_send_write_report()`

// "100F6C32-1735-4313-B402-38567131E5F3"
static uint8_t le_steam_service_uuid[16] = {0x10, 0x0f, 0x6c, 0x32, 0x17, 0x35, 0x43, 0x13,
                                            0xb4, 0x02, 0x38, 0x56, 0x71, 0x31, 0xe5, 0xf3};

// "100F6C34-1735-4313-B402-38567131E5F3"
static uint8_t le_steam_characteristic_report_uuid[16] = {0x10, 0x0f, 0x6c, 0x34, 0x17, 0x35, 0x43, 0x13,
                                                          0xb4, 0x02, 0x38, 0x56, 0x71, 0x31, 0xe5, 0xf3};

// Commands that can be sent in a feature report.
#define STEAM_CMD_SET_MAPPINGS 0x80
#define STEAM_CMD_CLEAR_MAPPINGS 0x81
#define STEAM_CMD_GET_MAPPINGS 0x82
#define STEAM_CMD_GET_ATTRIB 0x83
#define STEAM_CMD_GET_ATTRIB_LABEL 0x84
#define STEAM_CMD_DEFAULT_MAPPINGS 0x85
#define STEAM_CMD_FACTORY_RESET 0x86
#define STEAM_CMD_WRITE_REGISTER 0x87
#define STEAM_CMD_CLEAR_REGISTER 0x88
#define STEAM_CMD_READ_REGISTER 0x89
#define STEAM_CMD_GET_REGISTER_LABEL 0x8a
#define STEAM_CMD_GET_REGISTER_MAX 0x8b
#define STEAM_CMD_GET_REGISTER_DEFAULT 0x8c
#define STEAM_CMD_SET_MODE 0x8d
#define STEAM_CMD_DEFAULT_MOUSE 0x8e
#define STEAM_CMD_FORCEFEEDBAK 0x8f
#define STEAM_CMD_REQUEST_COMM_STATUS 0xb4
#define STEAM_CMD_GET_SERIAL 0xae
#define STEAM_CMD_HAPTIC_RUMBLE 0xeb

// Some useful register ids
#define STEAM_REG_LPAD_MODE 0x07
#define STEAM_REG_RPAD_MODE 0x08
#define STEAM_REG_RPAD_MARGIN 0x18
#define STEAM_REG_LED 0x2d
#define STEAM_REG_GYRO_MODE 0x30
#define STEAM_REG_LPAD_CLICK_PRESSURE 0x34
#define STEAM_REG_RPAD_CLICK_PRESSURE 0x35

static uint8_t cmd_clear_mappings[] = {
    0xc0, STEAM_CMD_CLEAR_MAPPINGS,  // Command
    0x01                             // Command Len
};

// clang-format off
static uint8_t cmd_disable_lizard[] = {
	0xc0, STEAM_CMD_WRITE_REGISTER,    // Command
	0x0f,                              // Command Len
	STEAM_REG_GYRO_MODE,   (uint8_t)(STEAM_GYRO_MODE_RAW_IMU & 0xff), (uint8_t)(STEAM_GYRO_MODE_RAW_IMU >> 8), // Enable raw accel + gyro
	STEAM_REG_LPAD_MODE,   0x07, 0x00, // Disable cursor
	STEAM_REG_RPAD_MODE,   0x07, 0x00, // Disable mouse
	STEAM_REG_RPAD_MARGIN, 0x00, 0x00, // No margin
	STEAM_REG_LED,         0x64, 0x00  // LED bright, max value
};
// clang-format on

typedef struct {
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic_report;
    steam_query_state_t query_state;
    uint8_t rumble_cmd[STEAM_HAPTIC_CMD_LEN];
    uint8_t pending_right_cmd[STEAM_HAPTIC_CMD_LEN];
    bool pending_right_valid;
    bool write_in_flight;
} steam_instance_t;
_Static_assert(sizeof(steam_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "Steam instance too big");

static void parse_buttons(struct uni_hid_device_s* d, const uint8_t* data);
static void parse_triggers(struct uni_hid_device_s* d, const uint8_t* data);
static void parse_thumbstick(struct uni_hid_device_s* d, const uint8_t* data);
static void parse_right_pad(struct uni_hid_device_s* d, const uint8_t* data);
static void parse_imu_accel(struct uni_hid_device_s* d, const uint8_t* data);
static void parse_imu_gyro(struct uni_hid_device_s* d, const uint8_t* data);
static uni_rumble_result_t steam_stop_rumble_now(struct uni_hid_device_s* d);
static uni_rumble_result_t steam_start_rumble_now(struct uni_hid_device_s* d,
                                                  uint8_t weak_magnitude,
                                                  uint8_t strong_magnitude,
                                                  uint8_t trigger_left,
                                                  uint8_t trigger_right);

static steam_instance_t* get_steam_instance(uni_hid_device_t* d) {
    return (steam_instance_t*)&d->parser_data[0];
}

// TODO: Make it easier for "parsers" to write/read/get notified from characteristics
static void uni_steam_handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    uint8_t att_status;
    uni_hid_device_t* device;
    steam_instance_t* ins;
    hci_con_handle_t con_handle;

    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;

    uint8_t event = hci_event_packet_get_type(packet);
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
        default:
            loge("Steam: Unknown GATT event: %#x\n", event);
            return;
    }

    device = uni_hid_device_get_instance_for_connection_handle(con_handle);
    if (!device) {
        loge("Steam: Invalid device for connection handle: %#x\n", con_handle);
        return;
    }
    ins = get_steam_instance(device);

    switch (ins->query_state) {
        case STATE_QUERY_SERVICE:
            switch (event) {
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    // store service (we expect only one)
                    gatt_event_service_query_result_get_service(packet, &ins->service);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("Steam: SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    // service query complete, look for characteristic report
                    ins->query_state = STATE_QUERY_CHARACTERISTIC_REPORT;
                    gatt_client_discover_characteristics_for_service_by_uuid128(uni_steam_handle_gatt_client_event,
                                                                                con_handle, &ins->service,
                                                                                le_steam_characteristic_report_uuid);
                    break;
                default:
                    loge("Steam: Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_CHARACTERISTIC_REPORT:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("Steam: SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    gatt_client_write_value_of_characteristic(uni_steam_handle_gatt_client_event, con_handle,
                                                              ins->characteristic_report.value_handle,
                                                              sizeof(cmd_clear_mappings), cmd_clear_mappings);
                    ins->query_state = STATE_QUERY_CLEAR_MAPPINGS;
                    break;
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
                    gatt_event_characteristic_query_result_get_characteristic(packet, &ins->characteristic_report);
                    break;
                default:
                    loge("Steam: Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_CLEAR_MAPPINGS:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("Steam: SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    gatt_client_write_value_of_characteristic(uni_steam_handle_gatt_client_event, con_handle,
                                                              ins->characteristic_report.value_handle,
                                                              sizeof(cmd_disable_lizard), cmd_disable_lizard);
                    ins->query_state = STATE_QUERY_DISABLE_LIZARD;
                    break;
                default:
                    loge("Steam: Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_DISABLE_LIZARD:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("Steam: SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    uni_hid_device_set_ready_complete(device);
                    ins->query_state = STATE_QUERY_END;
                    break;
                default:
                    loge("Steam: Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_END:
            if (event == GATT_EVENT_QUERY_COMPLETE) {
                ins->write_in_flight = false;
                att_status = gatt_event_query_complete_get_att_status(packet);
                if (att_status != ATT_ERROR_SUCCESS) {
                    logd("Steam: Rumble write complete with ATT status %#x\n", att_status);
                    ins->pending_right_valid = false;
                    break;
                }
                if (ins->pending_right_valid) {
                    memcpy(ins->rumble_cmd, ins->pending_right_cmd, sizeof(ins->rumble_cmd));
                    ins->pending_right_valid = false;
                    uint8_t status = gatt_client_write_value_of_characteristic(
                        uni_steam_handle_gatt_client_event, con_handle, ins->characteristic_report.value_handle,
                        sizeof(ins->rumble_cmd), ins->rumble_cmd);
                    if (status == ERROR_CODE_SUCCESS) {
                        ins->write_in_flight = true;
                    }
                }
            }
            break;
        default:
            loge("Steam: Unknown query state: %#x\n", ins->query_state);
            break;
    }
}

void uni_hid_parser_steam_setup(struct uni_hid_device_s* d) {
    if (d == NULL) {
        loge("Steam: Invalid device\n");
        return;
    }

    steam_instance_t* ins = get_steam_instance(d);
    memset(ins, 0, sizeof(*ins));

    uni_hid_parser_rumble_init(d, steam_start_rumble_now, steam_stop_rumble_now);

    ins->query_state = STATE_QUERY_SERVICE;
    gatt_client_discover_primary_services_by_uuid128(uni_steam_handle_gatt_client_event, d->conn.handle,
                                                     le_steam_service_uuid);

    // Set the type of controller class once.
    uni_controller_t* ctl = &d->controller;
    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_steam_init_report(uni_hid_device_t* d) {
    ARG_UNUSED(d);
    // Don't reset old state. Each report contains a full-state.
    // memset(ctl, 0, sizeof(*ctl));
}

void uni_hid_parser_steam_play_dual_rumble(struct uni_hid_device_s* d,
                                           uint16_t start_delay_ms,
                                           uint16_t duration_ms,
                                           uint8_t weak_magnitude,
                                           uint8_t strong_magnitude) {
    if (d == NULL) {
        loge("Steam: Invalid device\n");
        return;
    }

    uni_hid_parser_rumble_play_dual(d, start_delay_ms, duration_ms, weak_magnitude, strong_magnitude,
                                    steam_start_rumble_now, steam_stop_rumble_now);
}

void uni_hid_parser_steam_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    int idx;

    // Sanity checks: require at least the 4-byte BLE report header.
    // Reports with IMU sections enabled grow beyond 20 bytes.
    if (len < 4) {
        logi("Steam: Input report with unsupported length: %d\n", len);
        return;
    }

    // Report Id
    if (report[0] != 0x03)
        // Unsupported Report Id.
        return;

    if (report[1] != 0xc0)
        // Not sure what 0xc0 is
        return;

    // Only care about input reports.
    // Misc info comes in 0x05
    if ((report[2] & 0x0f) != 0x04) {
        // TODO: parse misc info
        return;
    }

    // printf_hexdump(report, len);

    uint16_t report_flags = (uint16_t)((report[2] & 0xf0) | (report[3] << 8));

    // Each flagged section is packed sequentially starting at byte offset 4.
    // Guard every section read with `idx + N <= len` and always advance `idx` by the
    // section's wire size so subsequent sections read from the correct offset without
    // reading past `report + len`.
    idx = 4;
    if (report_flags & STEAM_CONTROLLER_FLAG_BUTTONS) {
        if (idx + 3 <= len) {
            parse_buttons(d, &report[idx]);
        }
        idx += 3;
    }

    if (report_flags & STEAM_CONTROLLER_FLAG_TRIGGERS) {
        if (idx + 2 <= len) {
            parse_triggers(d, &report[idx]);
        }
        idx += 2;
    }

    if (report_flags & STEAM_CONTROLLER_FLAG_BUTTONS_EXT) {
        // Extra 3-byte buttons chunk (unmapped, but advances stream offset).
        idx += 3;
    }

    if (report_flags & STEAM_CONTROLLER_FLAG_THUMBSTICK) {
        if (idx + 4 <= len) {
            parse_thumbstick(d, &report[idx]);
        }
        idx += 4;
    }

    if (report_flags & STEAM_CONTROLLER_FLAG_LEFT_PAD) {
        // Not mapped for the moment, but still occupies 4 bytes in the report stream.
        idx += 4;
    }

    if (report_flags & STEAM_CONTROLLER_FLAG_RIGHT_PAD) {
        if (idx + 4 <= len) {
            parse_right_pad(d, &report[idx]);
        }
        idx += 4;
    }

    if (report_flags & STEAM_CONTROLLER_FLAG_IMU_ACCEL) {
        if (idx + 6 <= len) {
            parse_imu_accel(d, &report[idx]);
        }
        idx += 6;
    }

    if (report_flags & STEAM_CONTROLLER_FLAG_IMU_GYRO) {
        if (idx + 6 <= len) {
            parse_imu_gyro(d, &report[idx]);
        }
        idx += 6;
    }

    if (report_flags & STEAM_CONTROLLER_FLAG_IMU_QUAT) {
        // Quaternion occupies 8 bytes in the report stream.
        idx += 8;
    }
}

static void parse_buttons(struct uni_hid_device_s* d, const uint8_t* data) {
    uni_controller_t* ctl = &d->controller;

    // Not all reports have the "buttons" section, but when they do, all buttons are present.
    // Clear them now.
    ctl->gamepad.dpad = 0;
    ctl->gamepad.buttons = 0;
    ctl->gamepad.misc_buttons = 0;

    uint32_t buttons = data[0] | (data[1] << 8) | (data[2] << 16);

    ctl->gamepad.buttons |= (buttons & 0x01) ? BUTTON_TRIGGER_R : 0;
    ctl->gamepad.buttons |= (buttons & 0x02) ? BUTTON_TRIGGER_L : 0;
    ctl->gamepad.buttons |= (buttons & 0x04) ? BUTTON_SHOULDER_R : 0;
    ctl->gamepad.buttons |= (buttons & 0x08) ? BUTTON_SHOULDER_L : 0;

    ctl->gamepad.buttons |= (buttons & 0x10) ? BUTTON_Y : 0;
    ctl->gamepad.buttons |= (buttons & 0x20) ? BUTTON_B : 0;
    ctl->gamepad.buttons |= (buttons & 0x40) ? BUTTON_X : 0;
    ctl->gamepad.buttons |= (buttons & 0x80) ? BUTTON_A : 0;

    ctl->gamepad.dpad |= (buttons & 0x0100) ? DPAD_UP : 0;
    ctl->gamepad.dpad |= (buttons & 0x0200) ? DPAD_RIGHT : 0;
    ctl->gamepad.dpad |= (buttons & 0x0400) ? DPAD_LEFT : 0;
    ctl->gamepad.dpad |= (buttons & 0x0800) ? DPAD_DOWN : 0;

    ctl->gamepad.misc_buttons |= (buttons & 0x1000) ? MISC_BUTTON_SELECT : 0;
    ctl->gamepad.misc_buttons |= (buttons & 0x2000) ? MISC_BUTTON_SYSTEM : 0;
    ctl->gamepad.misc_buttons |= (buttons & 0x4000) ? MISC_BUTTON_START : 0;

    // Emulates the behavior of Steam Controller under Steam games.
    ctl->gamepad.buttons |= (buttons & 0x008000) ? BUTTON_A : 0;  // Left-inner button.
    ctl->gamepad.buttons |= (buttons & 0x010000) ? BUTTON_X : 0;  // right-inner button.

#if 0
	// Do nothing.
    ctl->gamepad.buttons |= (buttons & 0x080000) ? BUTTON_X : 0;  // Touch left pad
    ctl->gamepad.buttons |= (buttons & 0x100000) ? BUTTON_Y : 0;  // Touch right pad
#endif

    ctl->gamepad.buttons |= (buttons & 0x400000) ? BUTTON_THUMB_L : 0;
}

static void parse_thumbstick(struct uni_hid_device_s* d, const uint8_t* data) {
    uni_controller_t* ctl = &d->controller;

    // Widen to 32-bit signed integers after 16-bit sign-extension so negating INT16_MIN
    // (-32768 / 0x8000) produces +32768 (+512 after >> 6) instead of signed 16-bit overflow UB.
    int32_t x = (int16_t)(data[0] | (data[1] << 8));
    int32_t y = (int16_t)(data[2] | (data[3] << 8));
    y = -y;

    ctl->gamepad.axis_x = (x >> 6);
    ctl->gamepad.axis_y = (y >> 6);
}

static void parse_triggers(struct uni_hid_device_s* d, const uint8_t* data) {
    uni_controller_t* ctl = &d->controller;

    ctl->gamepad.brake = data[0] << 2;
    ctl->gamepad.throttle = data[1] << 2;
}

static void parse_right_pad(struct uni_hid_device_s* d, const uint8_t* data) {
    uni_controller_t* ctl = &d->controller;

    // Widen to 32-bit signed integers after 16-bit sign-extension so negating INT16_MIN
    // (-32768 / 0x8000) produces +32768 (+512 after >> 6) instead of signed 16-bit overflow UB.
    int32_t x = (int16_t)(data[0] | (data[1] << 8));
    int32_t y = (int16_t)(data[2] | (data[3] << 8));
    y = -y;

    ctl->gamepad.axis_rx = (x >> 6);
    ctl->gamepad.axis_ry = (y >> 6);
}

static void parse_imu_accel(struct uni_hid_device_s* d, const uint8_t* data) {
    uni_controller_t* ctl = &d->controller;
    int16_t ax = (int16_t)(data[0] | (data[1] << 8));
    int16_t ay = (int16_t)(data[2] | (data[3] << 8));
    int16_t az = (int16_t)(data[4] | (data[5] << 8));

    // Native sensor frame (+X right, +Y forward, +Z up) -> canonical Bluepad32 right-handed Y-up:
    // [0] = +X (right), [1] = +Y (up = +az), [2] = +Z (toward player = -ay).
    ctl->gamepad.accel[0] = (float)ax * STEAM_ACCEL_SCALE;
    ctl->gamepad.accel[1] = (float)az * STEAM_ACCEL_SCALE;
    ctl->gamepad.accel[2] = -(float)ay * STEAM_ACCEL_SCALE;
}

static void parse_imu_gyro(struct uni_hid_device_s* d, const uint8_t* data) {
    uni_controller_t* ctl = &d->controller;
    int16_t gx = (int16_t)(data[0] | (data[1] << 8));
    int16_t gy = (int16_t)(data[2] | (data[3] << 8));
    int16_t gz = (int16_t)(data[4] | (data[5] << 8));

    // Native sensor frame (+X pitch, +Y roll, +Z yaw) -> canonical Bluepad32 right-handed Y-up:
    // [0] = +X (pitch = +gx), [1] = +Y (yaw = +gz), [2] = +Z (roll = -gy).
    ctl->gamepad.gyro[0] = (float)gx * STEAM_GYRO_SCALE;
    ctl->gamepad.gyro[1] = (float)gz * STEAM_GYRO_SCALE;
    ctl->gamepad.gyro[2] = -(float)gy * STEAM_GYRO_SCALE;
}

static void steam_build_haptic_pulse_cmd(uint8_t out_cmd[STEAM_HAPTIC_CMD_LEN],
                                         uint8_t actuator,
                                         uint16_t duration_ms,
                                         uint8_t magnitude,
                                         uint16_t period_us) {
    uint16_t on_us = 0;
    uint16_t off_us = 0;
    uint16_t count = 0;

    if (magnitude > 0 && duration_ms > 0 && period_us > 0) {
        // Voice-coil LRAs reach peak AC oscillation amplitude at 50% duty cycle
        // (on_us == off_us == period_us / 2). Scale on_us linearly from 1..(period_us / 2)
        // and set off_us = period_us - on_us so (on_us + off_us) == period_us is invariant.
        uint16_t half_period_us = (uint16_t)(period_us / 2u);
        on_us = (uint16_t)(((uint32_t)magnitude * (uint32_t)half_period_us) / 255u);
        if (on_us == 0) {
            on_us = 1;
        }
        off_us = (uint16_t)(period_us - on_us);

        uint32_t total_us = (uint32_t)duration_ms * 1000u;
        uint32_t raw_count = total_us / (uint32_t)period_us;
        if (raw_count == 0) {
            count = 1;
        } else if (raw_count > 0xffffu) {
            count = 0xffffu;
        } else {
            count = (uint16_t)raw_count;
        }
    }

    out_cmd[0] = 0xc0;                    // BLE single-segment header (0x80 data | 0x40 last)
    out_cmd[1] = STEAM_CMD_FORCEFEEDBAK;  // 0x8f (ID_TRIGGER_HAPTIC_PULSE)
    out_cmd[2] = 0x08;                    // Payload length (8 bytes)
    out_cmd[3] = actuator;                // 0x00 = Right, 0x01 = Left (swapped on wire)
    out_cmd[4] = (uint8_t)(on_us & 0xffu);
    out_cmd[5] = (uint8_t)(on_us >> 8);
    out_cmd[6] = (uint8_t)(off_us & 0xffu);
    out_cmd[7] = (uint8_t)(off_us >> 8);
    out_cmd[8] = (uint8_t)(count & 0xffu);
    out_cmd[9] = (uint8_t)(count >> 8);
    out_cmd[10] = 0x00;  // 0 dB gain
}

static uni_rumble_result_t steam_send_rumble_dual(struct uni_hid_device_s* d,
                                                  uint16_t duration_ms,
                                                  uint8_t weak_magnitude,
                                                  uint8_t strong_magnitude) {
    steam_instance_t* ins = get_steam_instance(d);

    uint8_t left_cmd[STEAM_HAPTIC_CMD_LEN];
    uint8_t right_cmd[STEAM_HAPTIC_CMD_LEN];
    steam_build_haptic_pulse_cmd(left_cmd, STEAM_HAPTIC_ACTUATOR_LEFT, duration_ms, strong_magnitude,
                                 STEAM_RUMBLE_LEFT_PERIOD_US);
    steam_build_haptic_pulse_cmd(right_cmd, STEAM_HAPTIC_ACTUATOR_RIGHT, duration_ms, weak_magnitude,
                                 STEAM_RUMBLE_RIGHT_PERIOD_US);

    // Synthetic test devices (conn.handle == UNI_BT_CONN_HANDLE_INVALID) have no active BLE GATT session;
    // stage the primary/secondary packets in parser_data for inspection and return UNI_RUMBLE_OK.
    if (d->conn.handle == UNI_BT_CONN_HANDLE_INVALID) {
        if (strong_magnitude > 0 || weak_magnitude == 0) {
            memcpy(ins->rumble_cmd, left_cmd, sizeof(ins->rumble_cmd));
            memcpy(ins->pending_right_cmd, right_cmd, sizeof(ins->pending_right_cmd));
        } else {
            memcpy(ins->rumble_cmd, right_cmd, sizeof(ins->rumble_cmd));
            memcpy(ins->pending_right_cmd, left_cmd, sizeof(ins->pending_right_cmd));
        }
        ins->pending_right_valid = true;
        return UNI_RUMBLE_OK;
    }

    // On a live BLE connection, defer rumble if GATT setup has not finished or an ATT Write Request
    // is already in flight (avoiding mutating ins->rumble_cmd while gatt_client references it).
    if (ins->query_state != STATE_QUERY_END || ins->characteristic_report.value_handle == 0 || ins->write_in_flight) {
        ins->pending_right_valid = false;
        return UNI_RUMBLE_RETRY_BLE;
    }

    if (strong_magnitude > 0 || weak_magnitude == 0) {
        memcpy(ins->rumble_cmd, left_cmd, sizeof(ins->rumble_cmd));
        memcpy(ins->pending_right_cmd, right_cmd, sizeof(ins->pending_right_cmd));
    } else {
        memcpy(ins->rumble_cmd, right_cmd, sizeof(ins->rumble_cmd));
        memcpy(ins->pending_right_cmd, left_cmd, sizeof(ins->pending_right_cmd));
    }
    ins->pending_right_valid = true;

    uint8_t status = gatt_client_write_value_of_characteristic(uni_steam_handle_gatt_client_event, d->conn.handle,
                                                               ins->characteristic_report.value_handle,
                                                               sizeof(ins->rumble_cmd), ins->rumble_cmd);
    if (status == ERROR_CODE_SUCCESS) {
        ins->write_in_flight = true;
        return UNI_RUMBLE_OK;
    }

    ins->pending_right_valid = false;
    if (status == GATT_CLIENT_IN_WRONG_STATE || status == GATT_CLIENT_BUSY || status == ERROR_CODE_COMMAND_DISALLOWED) {
        logd("Steam: GATT busy sending rumble (status=%#x), retrying...\n", status);
        return UNI_RUMBLE_RETRY_BLE;
    }

    logi("Steam: Failed to send rumble report, status=%#x\n", status);
    return UNI_RUMBLE_ERR;
}

static uni_rumble_result_t steam_stop_rumble_now(struct uni_hid_device_s* d) {
    return steam_send_rumble_dual(d, 0, 0, 0);
}

static uni_rumble_result_t steam_start_rumble_now(struct uni_hid_device_s* d,
                                                  uint8_t weak_magnitude,
                                                  uint8_t strong_magnitude,
                                                  uint8_t trigger_left,
                                                  uint8_t trigger_right) {
    ARG_UNUSED(trigger_left);
    ARG_UNUSED(trigger_right);
    return steam_send_rumble_dual(d, d->rumble.duration_ms, weak_magnitude, strong_magnitude);
}