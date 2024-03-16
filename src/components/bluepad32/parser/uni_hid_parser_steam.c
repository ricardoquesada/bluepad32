// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

// Info from:
// https://github.com/rodrigorc/steamctrl/blob/master/src/steamctrl.c
// https://elixir.bootlin.com/linux/latest/source/drivers/hid/hid-steam.c
// https://github.com/haxpor/sdl2-samples/blob/master/android-project/app/src/main/java/org/libsdl/app/HIDDeviceBLESteamController.java
// https://github.com/g3gg0/LegoRemote

#include "parser/uni_hid_parser_steam.h"

#include "controller/uni_controller.h"
#include "hid_usage.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

// clang-format off
#define STEAM_CONTROLLER_FLAG_BUTTONS       0x0010
#define STEAM_CONTROLLER_FLAG_TRIGGERS 		0x0020
#define STEAM_CONTROLLER_FLAG_THUMBSTICK  	0x0080
#define STEAM_CONTROLLER_FLAG_LEFT_PAD      0x0100
#define STEAM_CONTROLLER_FLAG_RIGHT_PAD     0x0200
// clang-format on

typedef enum {
    STATE_QUERY_SERVICE,
    STATE_QUERY_CHARACTERISTIC_REPORT,
    STATE_QUERY_CLEAR_MAPPINGS,
    STATE_QUERY_DISABLE_LIZARD,
    STATE_QUERY_FORCEFEEDBACK,
    STATE_QUERY_END,
} steam_query_state_t;

// TODO: Can this be refactored to use `hids_client_send_write_report()`

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
	STEAM_REG_GYRO_MODE,   0x00, 0x00, // Disable gyro/accel
	STEAM_REG_LPAD_MODE,   0x07, 0x00, // Disable cursor
	STEAM_REG_RPAD_MODE,   0x07, 0x00, // Disable mouse
	STEAM_REG_RPAD_MARGIN, 0x00, 0x00, // No margin
	STEAM_REG_LED,         0x64, 0x00  // LED bright, max value
};
// clang-format on

static gatt_client_service_t le_steam_service;
static gatt_client_characteristic_t le_steam_characteristic_report;
static hci_con_handle_t connection_handle;
static uni_hid_device_t* device;
static steam_query_state_t query_state;

static void parse_buttons(struct uni_hid_device_s* d, const uint8_t* data);
static void parse_triggers(struct uni_hid_device_s* d, const uint8_t* data);
static void parse_thumbstick(struct uni_hid_device_s* d, const uint8_t* data);
static void parse_right_pad(struct uni_hid_device_s* d, const uint8_t* data);

// TODO: Make it easier for "parsers" to write/read/get notified from characteristics
static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    uint8_t att_status;

    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;

    uint8_t event = hci_event_packet_get_type(packet);
    switch (query_state) {
        case STATE_QUERY_SERVICE:
            switch (event) {
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    // store service (we expect only one)
                    gatt_event_service_query_result_get_service(packet, &le_steam_service);
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
                    query_state = STATE_QUERY_CHARACTERISTIC_REPORT;
                    gatt_client_discover_characteristics_for_service_by_uuid128(handle_gatt_client_event,
                                                                                connection_handle, &le_steam_service,
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
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle,
                                                              sizeof(cmd_clear_mappings), cmd_clear_mappings);
                    query_state = STATE_QUERY_CLEAR_MAPPINGS;
                    break;
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
                    gatt_event_characteristic_query_result_get_characteristic(packet, &le_steam_characteristic_report);
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
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle,
                                                              sizeof(cmd_disable_lizard), cmd_disable_lizard);
                    query_state = STATE_QUERY_DISABLE_LIZARD;
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
                    query_state = STATE_QUERY_END;
                    break;
                default:
                    loge("Steam: Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_END:
            // pass-through
        default:
            loge("Steam: Unknown query state: %#x\n", query_state);
            break;
    }
}

void uni_hid_parser_steam_setup(struct uni_hid_device_s* d) {
    device = d;
    connection_handle = d->conn.handle;
    query_state = STATE_QUERY_SERVICE;
    gatt_client_discover_primary_services_by_uuid128(handle_gatt_client_event, d->conn.handle, le_steam_service_uuid);

    // Set the type of controller class once.
    uni_controller_t* ctl = &d->controller;
    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_steam_init_report(uni_hid_device_t* d) {
    ARG_UNUSED(d);
    // Don't reset old state. Each report contains a full-state.
    // memset(ctl, 0, sizeof(*ctl));
}

void uni_hid_parser_steam_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    int idx;

    // Sanity checks
    if (len != 20) {
        logi("Steam: Inport report with unsupported length: %d\n", len);
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

    uint16_t report_flags = (report[2] & 0xf0) + (report[3] << 8);

    idx = 4;
    if (report_flags & STEAM_CONTROLLER_FLAG_BUTTONS) {
        parse_buttons(d, &report[idx]);
    }

    if (report_flags & STEAM_CONTROLLER_FLAG_TRIGGERS) {
        parse_triggers(d, &report[idx]);
    }

    if (report_flags & STEAM_CONTROLLER_FLAG_THUMBSTICK) {
        parse_thumbstick(d, &report[idx]);
    }

    if (report_flags & STEAM_CONTROLLER_FLAG_LEFT_PAD) {
        idx += 4;
    }

    if (report_flags & STEAM_CONTROLLER_FLAG_RIGHT_PAD) {
        parse_right_pad(d, &report[idx]);
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

    int16_t x = (data[0] | data[1] << 8);
    int16_t y = (data[2] | data[3] << 8);
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

    int16_t x = (data[0] | data[1] << 8);
    int16_t y = (data[2] | data[3] << 8);
    y = -y;

    ctl->gamepad.axis_rx = (x >> 6);
    ctl->gamepad.axis_ry = (y >> 6);
}