/*
 * Copyright (C) 2017 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

/*
 * Copyright (C) 2019 Ricardo Quesada
 * Unijoysticle additions based on the following BlueKitchen's test/example
 * files:
 *   - hid_host_test.c
 *   - hid_device.c
 *   - gap_inquire.c
 *   - hid_device_test.c
 *   - gap_link_keys.c
 */

/* This file controls everything related to Bluetooth, like connections, state,
 * queries, etc. No Bluetooth logic should be placed outside this file. That
 * way, in theory, it should be possible to support USB devices by replacing
 * this file.
 */

#include "bt/uni_bt.h"

#include <btstack.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "sdkconfig.h"

#include "bt/uni_bt_bredr.h"
#include "bt/uni_bt_hci_cmd.h"
#include "bt/uni_bt_le.h"
#include "bt/uni_bt_service.h"
#include "bt/uni_bt_setup.h"
#include "platform/uni_platform.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_hid_device.h"
#include "uni_log.h"
#include "uni_property.h"

// globals
bd_addr_t uni_local_bd_addr;

// Used to implement connection timeout and reconnect timer
static btstack_context_callback_registration_t cmd_callback_registration;

static bool bt_scanning_enabled;

static void start_scan(void);
static void stop_scan(void);

enum {
    CMD_BT_DEL_KEYS,
    CMD_BT_LIST_KEYS,
    CMD_BT_ENABLE,
    CMD_BT_DISABLE,
    CMD_DUMP_DEVICES,
    CMD_DISCONNECT_DEVICE,
    CMD_BLE_SERVICE_ENABLE,
    CMD_BLE_SERVICE_DISABLE,
};

static void bluetooth_del_keys(void) {
    if (IS_ENABLED(UNI_ENABLE_BREDR))
        uni_bt_bredr_delete_bonded_keys();
    if (IS_ENABLED(UNI_ENABLE_BLE))
        uni_bt_le_delete_bonded_keys();
}

static void bluetooth_list_keys(void) {
    if (IS_ENABLED(UNI_ENABLE_BREDR))
        uni_bt_bredr_list_bonded_keys();
    if (IS_ENABLED(UNI_ENABLE_BLE))
        uni_bt_le_list_bonded_keys();
}

static void start_scan(void) {
    logd("--> Scanning for new controllers\n");

    if (IS_ENABLED(UNI_ENABLE_BREDR))
        uni_bt_bredr_scan_start();
    if (IS_ENABLED(UNI_ENABLE_BLE))
        uni_bt_le_scan_start();
}

static void stop_scan(void) {
    logd("--> Stop scanning for new controllers\n");

    if (IS_ENABLED(UNI_ENABLE_BREDR))
        uni_bt_bredr_scan_stop();
    if (IS_ENABLED(UNI_ENABLE_BLE))
        uni_bt_le_scan_stop();
}

static void enable_new_connections(bool enabled) {
    if (bt_scanning_enabled != enabled) {
        bt_scanning_enabled = enabled;

        if (enabled)
            start_scan();
        else
            stop_scan();
    }

    uni_get_platform()->on_oob_event(UNI_PLATFORM_OOB_BLUETOOTH_ENABLED, (void*)enabled);
}

static void on_hci_disconnection_complete(uint16_t channel, const uint8_t* packet, uint16_t size) {
    uint16_t handle;
    uni_hid_device_t* d;
    gap_connection_type_t type;
    uint8_t status, reason;

    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    handle = hci_event_disconnection_complete_get_connection_handle(packet);
    reason = hci_event_disconnection_complete_get_reason(packet);
    status = hci_event_disconnection_complete_get_status(packet);

    // Xbox Wireless Controller starts an incoming connection when told to enter in "discovery mode". If the connection
    // fails (HCI_EVENT_DISCONNECTION_COMPLETE is generated) then it starts the discovery. So, delete the
    // possible-previous created entry. This highly increases the reliability of Xbox Wireless controllers.
    d = uni_hid_device_get_instance_for_connection_handle(handle);
    if (d) {
        // Get type before it gets destroyed.
        type = gap_get_connection_type(d->conn.handle);

        logi("Device %s disconnected, deleting it. Reason=%#x, status=%d\n", bd_addr_to_str(d->conn.btaddr), reason,
             status);
        uni_hid_device_disconnect(d);
        uni_hid_device_delete(d);
        // Device cannot be used after delete.
        d = NULL;

        if (IS_ENABLED(UNI_ENABLE_BLE) && type == GAP_CONNECTION_LE)
            uni_bt_le_on_hci_disconnection_complete(channel, packet, size);
        else if (IS_ENABLED(UNI_ENABLE_BREDR) && type == GAP_CONNECTION_ACL)
            uni_bt_bredr_on_hci_disconnection_complete(channel, packet, size);
        else
            loge("on_hci_disconnection_complete: Unknown GAP connection type: %d\n", type);
    }
}

static void cmd_callback(void* context) {
    uni_hid_device_t* d;
    unsigned long ctx = (unsigned long)context;
    uint16_t cmd = ctx & 0xffff;
    uint16_t args = (ctx >> 16) & 0xffff;

    switch (cmd) {
        case CMD_BT_DEL_KEYS:
            bluetooth_del_keys();
            break;
        case CMD_BT_LIST_KEYS:
            bluetooth_list_keys();
            break;
        case CMD_BT_ENABLE:
            enable_new_connections(true);
            break;
        case CMD_BT_DISABLE:
            enable_new_connections(false);
            break;
        case CMD_DUMP_DEVICES:
            uni_hid_device_dump_all();
            break;
        case CMD_DISCONNECT_DEVICE:
            d = uni_hid_device_get_instance_for_idx(args);
            if (!d) {
                loge("cmd_callback: Invalid device index: %d\n", args);
                return;
            }
            uni_hid_device_disconnect(d);
            uni_hid_device_delete(d);
            break;
        case CMD_BLE_SERVICE_ENABLE:
            uni_bt_service_set_enabled(true);
            break;
        case CMD_BLE_SERVICE_DISABLE:
            uni_bt_service_set_enabled(false);
            break;
        default:
            loge("Unknown command: %#x\n", cmd);
            break;
    }
}

//
// Public functions
//

void uni_bt_del_keys_safe(void) {
    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)CMD_BT_DEL_KEYS;
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

void uni_bt_del_keys_unsafe(void) {
    bluetooth_del_keys();
}

void uni_bt_list_keys_safe(void) {
    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)CMD_BT_LIST_KEYS;
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

void uni_bt_list_keys_unsafe(void) {
    bluetooth_list_keys();
}

void uni_bt_enable_new_connections_safe(bool enabled) {
    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)(enabled ? (intptr_t)CMD_BT_ENABLE : (intptr_t)CMD_BT_DISABLE);
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

void uni_bt_enable_new_connections_unsafe(bool enabled) {
    enable_new_connections(enabled);
}

bool uni_bt_enable_new_connections_is_enabled(void) {
    return bt_scanning_enabled;
}

void uni_bt_dump_devices_safe(void) {
    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)CMD_DUMP_DEVICES;
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

void uni_bt_disconnect_device_safe(int device_idx) {
    unsigned long idx = (unsigned long)device_idx;
    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)(CMD_DISCONNECT_DEVICE | (idx << 16));
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

void uni_bt_enable_service_safe(bool enabled) {
    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context =
        (void*)(enabled ? (intptr_t)CMD_BLE_SERVICE_ENABLE : (intptr_t)CMD_BLE_SERVICE_DISABLE);
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

void uni_bt_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    uint8_t event;
    uni_hid_device_t* device;
    uint8_t status;
    uint16_t handle;

    if (!uni_bt_setup_is_ready()) {
        uni_bt_setup_packet_handler(packet_type, channel, packet, size);
        return;
    }

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);
            switch (event) {
                // HCI EVENTS
                case HCI_EVENT_LE_META:
                    if (IS_ENABLED(UNI_ENABLE_BLE))
                        uni_bt_le_on_hci_event_le_meta(packet, size);
                    break;
                case HCI_EVENT_ENCRYPTION_CHANGE:
                    if (IS_ENABLED(UNI_ENABLE_BLE))
                        uni_bt_le_on_hci_event_encryption_change(packet, size);
                    break;
                case HCI_EVENT_COMMAND_COMPLETE: {
                    uint16_t opcode = hci_event_command_complete_get_command_opcode(packet);
                    const uint8_t* param = hci_event_command_complete_get_return_parameters(packet);
                    status = param[0];
                    if (status)
                        logi("Failed command: HCI_EVENT_COMMAND_COMPLETE: opcode = 0x%04x - status=%d\n", opcode,
                             status);
                    break;
                }
                case HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT: {
                    status = hci_event_authentication_complete_get_status(packet);
                    handle = hci_event_authentication_complete_get_connection_handle(packet);
                    logi("--> HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT: status=%d, handle=0x%04x\n", status, handle);
                    break;
                }
                case HCI_EVENT_PIN_CODE_REQUEST: {
                    if (IS_ENABLED(UNI_ENABLE_BREDR))
                        uni_bt_bredr_on_hci_pin_code_request(channel, packet, size);
                    break;
                }
                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    logi("SSP User Confirmation Request with numeric value '%" PRIu32 "'\n",
                         little_endian_read_32(packet, 8));
                    logi("SSP User Confirmation Auto accept\n");
                    break;
                case HCI_EVENT_HID_META: {
                    uint8_t code = hci_event_hid_meta_get_subevent_code(packet);
                    logi("HCI HID META SUBEVENT: 0x%02x\n", code);
                    switch (code) {
                        case HID_SUBEVENT_INCOMING_CONNECTION:
                            // There is an incoming connection: we can accept it or decline it.
                            // The hid_host_report_mode in the hid_host_accept_connection function
                            // allows the application to request a protocol mode.
                            // For available protocol modes, see hid_protocol_mode_t in btstack_hid.h file.
                            logi("UNSUPPORTED ---> HCI_EVENT_HID_META : HID_SUBEVENT_INCOMING_CONNECTION <---\n");
                            break;

                        case HID_SUBEVENT_CONNECTION_OPENED:
                            // The status field of this event indicates if the control and interrupt
                            // connections were opened successfully.
                            logi("UNSUPPORTED ---> HCI_EVENT_HID_META : HID_SUBEVENT_CONNECTION_OPENED <---\n");
                            break;

                        case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
                            // This event will follows HID_SUBEVENT_CONNECTION_OPENED event.
                            // For incoming connections, i.e. HID Device initiating the connection,
                            // the HID_SUBEVENT_DESCRIPTOR_AVAILABLE is delayed, and some HID
                            // reports may be received via HID_SUBEVENT_REPORT event. It is up to
                            // the application if these reports should be buffered or ignored until
                            // the HID descriptor is available.
                            logi("UNSUPPORTED ---> HCI_EVENT_HID_META : HID_SUBEVENT_DESCRIPTOR_AVAILABLE <---\n");
                            break;

                        case HID_SUBEVENT_REPORT:
                            // Handle input report.
                            logi("UNSUPPORTED ---> HCI_EVENT_HID_META : HID_SUBEVENT_REPORT <---\n");
                            break;

                        case HID_SUBEVENT_SET_PROTOCOL_RESPONSE:
                            // For incoming connections, the library will set the protocol mode of the
                            // HID Device as requested in the call to hid_host_accept_connection. The event
                            // reports the result. For connections initiated by calling hid_host_connect,
                            // this event will occur only if the established report mode is boot mode.
                            logi("UNSUPPORTED ---> HCI_EVENT_HID_META : HID_SUBEVENT_SET_PROTOCOL_RESPONSE <---\n");
                            break;

                        case HID_SUBEVENT_CONNECTION_CLOSED:
                            // The connection was closed.
                            logi("UNSUPPORTED ---> HCI_EVENT_HID_META : HID_SUBEVENT_CONNECTION_CLOSED <---\n");
                            break;

                        default:
                            break;
                    }
                }
                case HCI_EVENT_INQUIRY_RESULT:
                    // logi("--> HCI_EVENT_INQUIRY_RESULT <--\n");
                    break;
                case HCI_EVENT_CONNECTION_REQUEST:
                    logi("--> HCI_EVENT_CONNECTION_REQUEST: link_type = %d <--\n",
                         hci_event_connection_request_get_link_type(packet));
                    if (IS_ENABLED(UNI_ENABLE_BREDR))
                        uni_bt_bredr_on_hci_connection_request(channel, packet, size);
                    break;
                case HCI_EVENT_CONNECTION_COMPLETE:
                    logi("--> HCI_EVENT_CONNECTION_COMPLETE\n");
                    if (IS_ENABLED(UNI_ENABLE_BREDR))
                        uni_bt_bredr_on_hci_connection_complete(channel, packet, size);
                    break;
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    logi("--> HCI_EVENT_DISCONNECTION_COMPLETE\n");
                    on_hci_disconnection_complete(channel, packet, size);
                    break;
                case HCI_EVENT_LINK_KEY_REQUEST:
                    logi("--> HCI_EVENT_LINK_KEY_REQUEST:\n");
                    break;
                case HCI_EVENT_ROLE_CHANGE:
                    logi("--> HCI_EVENT_ROLE_CHANGE\n");
                    break;
                case HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE:
                    logi("--> HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE\n");
                    break;
                case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
                    // logi("--> HCI_EVENT_INQUIRY_RESULT_WITH_RSSI <--\n");
                    break;
                case HCI_EVENT_EXTENDED_INQUIRY_RESPONSE:
                    // logi("--> HCI_EVENT_EXTENDED_INQUIRY_RESPONSE <--\n");
                    break;
                case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
                    if (IS_ENABLED(UNI_ENABLE_BREDR))
                        uni_bt_bredr_on_hci_remote_name_request_complete(channel, packet, size);
                    break;
                // L2CAP EVENTS
                case L2CAP_EVENT_CAN_SEND_NOW:
                    logd("--> L2CAP_EVENT_CAN_SEND_NOW\n");
                    uint16_t local_cid = l2cap_event_can_send_now_get_local_cid(packet);
                    device = uni_hid_device_get_instance_for_cid(local_cid);
                    if (device == NULL) {
                        loge("--->>> CANNOT FIND DEVICE");
                    } else {
                        uni_hid_device_send_queued_reports(device);
                    }
                    break;
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    logi("--> L2CAP_EVENT_INCOMING_CONNECTION\n");
                    if (IS_ENABLED(UNI_ENABLE_BREDR))
                        uni_bt_bredr_on_l2cap_incoming_connection(channel, packet, size);
                    break;
                case L2CAP_EVENT_CHANNEL_OPENED:
                    if (IS_ENABLED(UNI_ENABLE_BREDR))
                        uni_bt_bredr_on_l2cap_channel_opened(channel, packet, size);
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    if (IS_ENABLED(UNI_ENABLE_BREDR))
                        uni_bt_bredr_on_l2cap_channel_closed(channel, packet, size);
                    break;

                // GAP EVENTS
                case GAP_EVENT_INQUIRY_RESULT:
                    // logi("--> GAP_EVENT_INQUIRY_RESULT\n");
                    if (IS_ENABLED(UNI_ENABLE_BREDR))
                        uni_bt_bredr_on_gap_inquiry_result(channel, packet, size);
                    break;
                case GAP_EVENT_INQUIRY_COMPLETE:
                    logd("--> GAP_EVENT_INQUIRY_COMPLETE\n");
                    // This can happen when "exit periodic inquiry" is called.
                    // Just do nothing, don't call "start_scan" again.
                    break;
                case GAP_EVENT_ADVERTISING_REPORT:
                    if (IS_ENABLED(UNI_ENABLE_BLE))
                        uni_bt_le_on_gap_event_advertising_report(packet, size);
                    break;
                case GAP_EVENT_RSSI_MEASUREMENT: {
                    uint8_t rssi = gap_event_rssi_measurement_get_rssi(packet);
                    hci_con_handle_t conn = gap_event_rssi_measurement_get_con_handle(packet);
                    uni_hid_device_t* d = uni_hid_device_get_instance_for_connection_handle(conn);
                    if (!d) {
                        loge("Could not found device for connection handle: %x\n", conn);
                        break;
                    }
                    d->conn.rssi = rssi;
                    break;
                }
                // GATT EVENTS (BLE only)
                case GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
                    logd("--> GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT\n");
                    break;
                default:
                    break;
            }
            break;
        case L2CAP_DATA_PACKET:
            if (IS_ENABLED(UNI_ENABLE_BREDR))
                uni_bt_bredr_on_l2cap_data_packet(channel, packet, size);
            break;
        default:
            loge("unhandled packet type: 0x%02x\n", packet_type);
            break;
    }
}

// Properties
void uni_bt_set_gap_security_level(int gap) {
    uni_property_value_t val;

    val.u32 = gap;
    uni_property_set(UNI_PROPERTY_IDX_GAP_LEVEL, val);
}

int uni_bt_get_gap_security_level() {
    uni_property_value_t val;

    val = uni_property_get(UNI_PROPERTY_IDX_GAP_LEVEL);
    return val.u32;
}

void uni_bt_set_gap_inquiry_length(int len) {
    uni_property_value_t val;

    val.u8 = len;
    uni_property_set(UNI_PROPERTY_IDX_GAP_INQ_LEN, val);
}

int uni_bt_get_gap_inquiry_length(void) {
    uni_property_value_t val;

    val = uni_property_get(UNI_PROPERTY_IDX_GAP_INQ_LEN);
    return val.u8;
}

void uni_bt_set_gap_max_peridic_length(int len) {
    uni_property_value_t val;

    val.u8 = len;
    uni_property_set(UNI_PROPERTY_IDX_GAP_MAX_PERIODIC_LEN, val);
}

int uni_bt_get_gap_max_periodic_length(void) {
    uni_property_value_t val;

    val = uni_property_get(UNI_PROPERTY_IDX_GAP_MAX_PERIODIC_LEN);
    return val.u8;
}

void uni_bt_set_gap_min_peridic_length(int len) {
    uni_property_value_t val;

    val.u8 = len;
    uni_property_set(UNI_PROPERTY_IDX_GAP_MIN_PERIODIC_LEN, val);
}

int uni_bt_get_gap_min_periodic_length(void) {
    uni_property_value_t val;

    val = uni_property_get(UNI_PROPERTY_IDX_GAP_MIN_PERIODIC_LEN);
    return val.u8;
}

void uni_bt_get_local_bd_addr_safe(bd_addr_t addr) {
    memcpy(addr, uni_local_bd_addr, BD_ADDR_LEN);
}