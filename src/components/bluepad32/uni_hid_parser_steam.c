/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2023 Ricardo Quesada

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

#include "uni_hid_parser_steam.h"

#include "hid_usage.h"
#include "uni_common.h"
#include "uni_controller.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_log.h"

typedef enum {
    STATE_QUERY_SERVICE,
    STATE_QUERY_CHARACTERISTIC_REPORT,
    STATE_QUERY_SET_VALVE_MODE1,
    STATE_QUERY_SET_VALVE_MODE2,
    STATE_QUERY_SET_VALVE_MODE3,
    STATE_QUERY_SET_VALVE_MODE4,
    STATE_QUERY_SET_VALVE_MODE5,
    STATE_QUERY_SET_VALVE_MODE6,
    STATE_QUERY_SET_VALVE_MODE7,
    STATE_QUERY_SET_VALVE_MODE8,
    STATE_QUERY_SET_VALVE_MODE9,
    STATE_QUERY_SET_VALVE_MODE10,
    STATE_QUERY_SET_VALVE_MODE11,
    STATE_QUERY_SET_VALVE_MODE12,
    STATE_QUERY_END,
} steam_query_state_t;

// "100F6C32-1735-4313-B402-38567131E5F3"
static uint8_t le_steam_service_uuid[16] = {0x10, 0x0f, 0x6c, 0x32, 0x17, 0x35, 0x43, 0x13,
                                            0xb4, 0x02, 0x38, 0x56, 0x71, 0x31, 0xe5, 0xf3};

// "100F6C34-1735-4313-B402-38567131E5F3"
static uint8_t le_steam_characteristic_report_uuid[16] = {0x10, 0x0f, 0x6c, 0x34, 0x17, 0x35, 0x43, 0x13,
                                                          0xb4, 0x02, 0x38, 0x56, 0x71, 0x31, 0xe5, 0xf3};

// Steam mode
static uint8_t enter_valve_mode1[] = {0xc0, 0x87, 0x03, 0x08, 0x07, 0x00, 0x00, 0x00,   // 0-7
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-15
                                      0x00, 0x00, 0x00};                                // 16-18
static uint8_t enter_valve_mode2[] = {0xc0, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0 -7
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-15
                                      0x00, 0x00, 0x00};                                // 16-18
static uint8_t enter_valve_mode3[] = {0xc0, 0x87, 0x03, 0x32, 0x84, 0x03, 0x00, 0x00,   // 0 -7
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-15
                                      0x00, 0x00, 0x00};                                // 16-18
static uint8_t enter_valve_mode4[] = {0xc0, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0 -7
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-15
                                      0x00, 0x00, 0x00};                                // 16-18
static uint8_t enter_valve_mode5[] = {0x80, 0xae, 0x15, 0x01, 0x00, 0x00, 0x00, 0x00,   // 0 -7
                                      0x01, 0x06, 0x11, 0x00, 0x00, 0x02, 0x03, 0x00,   // 8-15
                                      0x00, 0x00, 0x0a};                                // 16-18
static uint8_t enter_valve_mode6[] = {0xc1, 0x6d, 0x92, 0xd2, 0x55, 0x04, 0x00, 0x00,   // 0 -7
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-15
                                      0x00, 0x00, 0x00};                                // 16-18
static uint8_t enter_valve_mode7[] = {0xc0, 0xba, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0 -7
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-15
                                      0x00, 0x00, 0x00};                                // 16-18
static uint8_t enter_valve_mode8[] = {0xc0, 0x87, 0x03, 0x2d, 0x64, 0x00, 0x00, 0x00,   // 0 -7
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-15
                                      0x00, 0x00, 0x00};                                // 16-18
static uint8_t enter_valve_mode9[] = {0xc0, 0x8f, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,   // 0 -7
                                      0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,   // 8-15
                                      0x00, 0x00, 0x00};                                // 16-18
static uint8_t enter_valve_mode10[] = {0xc0, 0x8f, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00,  // 0 -7
                                       0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,  // 8-15
                                       0x00, 0x00, 0x00};                               // 16-18
// 1337
static uint8_t enter_valve_mode11[] = {0xc0, 0x8f, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00,  // 0 -7
                                       0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,  // 8-15
                                       0x00, 0x00, 0x00};                               // 16-18

// check 1560

static gatt_client_service_t le_steam_service;
static gatt_client_characteristic_t le_steam_characteristic_report;
static hci_con_handle_t connection_handle;
static uni_hid_device_t* device;
static steam_query_state_t query_state;

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    uint8_t att_status;

    if (packet_type != HCI_EVENT_PACKET)
        return;

    printf_hexdump(packet, size);

    uint16_t conn_interval;
    uint8_t event = hci_event_packet_get_type(packet);
    switch (query_state) {
        case STATE_QUERY_SERVICE:
            switch (event) {
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    logi("gatt_event_service_query_result\n");
                    // store service (we expect only one)
                    gatt_event_service_query_result_get_service(packet, &le_steam_service);
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    logi("gatt_event_query_complete\n");
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    // service query complete, look for characteristic
                    logi("Search for LE Steam characteristic.\n");
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
                    logi("gatt_event_query_complete\n");
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle, 19,
                                                              enter_valve_mode1);
                    query_state = STATE_QUERY_SET_VALVE_MODE1;
                    break;
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
                    logi("gatt_event_characteristic_query_result\n");
                    gatt_event_characteristic_query_result_get_characteristic(packet, &le_steam_characteristic_report);
                    break;
                default:
                    loge("Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_SET_VALVE_MODE1:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    logi("gatt_event_query_complete\n");
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle, 19,
                                                              enter_valve_mode2);
                    query_state = STATE_QUERY_SET_VALVE_MODE2;
                    break;
                default:
                    loge("Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_SET_VALVE_MODE2:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    logi("gatt_event_query_complete\n");
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle, 19,
                                                              enter_valve_mode3);
                    query_state = STATE_QUERY_SET_VALVE_MODE3;
                    break;
                default:
                    loge("Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_SET_VALVE_MODE3:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    logi("gatt_event_query_complete\n");
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle, 19,
                                                              enter_valve_mode4);
                    query_state = STATE_QUERY_SET_VALVE_MODE4;
                    break;
                default:
                    loge("Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_SET_VALVE_MODE4:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    logi("gatt_event_query_complete\n");
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle, 19,
                                                              enter_valve_mode5);
                    query_state = STATE_QUERY_SET_VALVE_MODE5;
                    break;
                default:
                    loge("Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_SET_VALVE_MODE5:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    logi("gatt_event_query_complete\n");
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle, 19,
                                                              enter_valve_mode6);
                    query_state = STATE_QUERY_SET_VALVE_MODE6;
                    break;
                default:
                    loge("Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_SET_VALVE_MODE6:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    logi("gatt_event_query_complete\n");
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle, 19,
                                                              enter_valve_mode7);
                    query_state = STATE_QUERY_SET_VALVE_MODE7;
                    break;
                default:
                    loge("Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_SET_VALVE_MODE7:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    logi("gatt_event_query_complete\n");
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle, 19,
                                                              enter_valve_mode8);
                    query_state = STATE_QUERY_SET_VALVE_MODE8;
                    break;
                default:
                    loge("Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_SET_VALVE_MODE8:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    logi("gatt_event_query_complete\n");
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle, 19,
                                                              enter_valve_mode9);
                    query_state = STATE_QUERY_SET_VALVE_MODE9;
                    break;
                default:
                    loge("Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_SET_VALVE_MODE9:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    logi("gatt_event_query_complete\n");
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle, 19,
                                                              enter_valve_mode10);
                    query_state = STATE_QUERY_SET_VALVE_MODE10;
                    break;
                default:
                    loge("Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_SET_VALVE_MODE10:
            switch (event) {
                case GATT_EVENT_QUERY_COMPLETE:
                    logi("gatt_event_query_complete\n");
                    att_status = gatt_event_query_complete_get_att_status(packet);
                    if (att_status != ATT_ERROR_SUCCESS) {
                        loge("SERVICE_QUERY_RESULT - Error status %x.\n", att_status);
                        // Should disconnect (?)
                        // gap_disconnect(connection_handle);
                        break;
                    }
                    // Reuse "mode2"
                    gatt_client_write_value_of_characteristic(handle_gatt_client_event, connection_handle,
                                                              le_steam_characteristic_report.value_handle, 19,
                                                              enter_valve_mode2);
                    query_state = STATE_QUERY_SET_VALVE_MODE11;
                    break;
                default:
                    loge("Unknown event: %#x\n", event);
            }
            break;
        case STATE_QUERY_SET_VALVE_MODE11:
            uni_hid_device_set_ready_complete(device);
            break;
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
}

void uni_hid_parser_steam_init_report(uni_hid_device_t* d) {
    // Reset old state. Each report contains a full-state.
    uni_controller_t* ctl = &d->controller;
    memset(ctl, 0, sizeof(*ctl));
    ctl->klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_steam_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    printf_hexdump(report, len);
}
