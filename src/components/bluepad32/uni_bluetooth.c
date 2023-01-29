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

#include "uni_bluetooth.h"

#include <btstack.h>
#include <btstack_config.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"
#include "uni_ble.h"
#include "uni_bt_conn.h"
#include "uni_bt_defines.h"
#include "uni_bt_sdp.h"
#include "uni_bt_setup.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_hci_cmd.h"
#include "uni_hid_device.h"
#include "uni_hid_device_vendors.h"
#include "uni_hid_parser.h"
#include "uni_log.h"
#include "uni_platform.h"

#define INQUIRY_REMOTE_NAME_TIMEOUT_MS 4500
_Static_assert(INQUIRY_REMOTE_NAME_TIMEOUT_MS < HID_DEVICE_CONNECTION_TIMEOUT_MS, "Timeout too big");

// globals
// Used to implement connection timeout and reconnect timer
static btstack_context_callback_registration_t cmd_callback_registration;

static bool bt_scanning_enabled = true;

static void on_l2cap_channel_closed(uint16_t channel, const uint8_t* packet, uint16_t size);
static void on_l2cap_channel_opened(uint16_t channel, const uint8_t* packet, uint16_t size);
static void on_l2cap_incoming_connection(uint16_t channel, const uint8_t* packet, uint16_t size);
static void on_l2cap_data_packet(uint16_t channel, const uint8_t* packet, uint16_t sizel);
static void on_gap_inquiry_result(uint16_t channel, const uint8_t* packet, uint16_t size);
static void on_hci_connection_complete(uint16_t channel, const uint8_t* packet, uint16_t size);
static void on_hci_connection_request(uint16_t channel, const uint8_t* packet, uint16_t size);
static void l2cap_create_control_connection(uni_hid_device_t* d);
static void l2cap_create_interrupt_connection(uni_hid_device_t* d);
static void inquiry_remote_name_timeout_callback(btstack_timer_source_t* ts);
static uint8_t start_scan(void);
static uint8_t stop_scan(void);

enum {
    CMD_BT_DEL_KEYS,
    CMD_BT_LIST_KEYS,
    CMD_BT_ENABLE,
    CMD_BT_DISABLE,
    CMD_DUMP_DEVICES,
    CMD_DISCONNECT_DEVICE,
};

static void on_hci_connection_request(uint16_t channel, const uint8_t* packet, uint16_t size) {
    bd_addr_t event_addr;
    uint32_t cod;
    uni_hid_device_t* device;

    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    hci_event_connection_request_get_bd_addr(packet, event_addr);
    cod = hci_event_connection_request_get_class_of_device(packet);

    device = uni_hid_device_get_instance_for_address(event_addr);
    if (device == NULL) {
        device = uni_hid_device_create(event_addr);
        if (device == NULL) {
            logi("Cannot create new device... no more slots available\n");
            return;
        }
    }
    uni_hid_device_set_cod(device, cod);
    uni_hid_device_set_incoming(device, true);
    logi("on_hci_connection_request from: address = %s, cod=0x%04x\n", bd_addr_to_str(event_addr), cod);
}

static void on_hci_connection_complete(uint16_t channel, const uint8_t* packet, uint16_t size) {
    bd_addr_t event_addr;
    uni_hid_device_t* d;
    hci_con_handle_t handle;
    uint8_t status;

    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    hci_event_connection_complete_get_bd_addr(packet, event_addr);
    status = hci_event_connection_complete_get_status(packet);
    if (status) {
        logi("on_hci_connection_complete failed (0x%02x) for %s\n", status, bd_addr_to_str(event_addr));
        return;
    }

    d = uni_hid_device_get_instance_for_address(event_addr);
    if (d == NULL) {
        logi("on_hci_connection_complete: failed to get device for %s\n", bd_addr_to_str(event_addr));
        return;
    }

    handle = hci_event_connection_complete_get_connection_handle(packet);
    uni_hid_device_set_connection_handle(d, handle);

    // if (uni_hid_device_is_incoming(d)) {
    //   hci_send_cmd(&hci_authentication_requested, handle);
    // }

    int cod = d->cod;
    bool is_keyboard = ((cod & UNI_BT_COD_MAJOR_MASK) == UNI_BT_COD_MAJOR_PERIPHERAL) && (cod & UNI_BT_COD_MINOR_MASK);
    if (is_keyboard) {
        // gap_request_security_level(handle, LEVEL_1);
    }
#ifndef CONFIG_BLUEPAD32_GAP_SECURITY
    // Seems to help on certain devices when using GAP Security level 0.
    // For exmaple, Dualshock 3 and Nintendo Switch works when the l2cap security level is 0,
    // and then I request it here to be 2.
    // But this is not perfect solution, since other gamepads requires that L2CAP be at Level 2.
    gap_request_security_level(handle, LEVEL_2);
#endif
}

static void on_gap_inquiry_result(uint16_t channel, const uint8_t* packet, uint16_t size) {
    bd_addr_t addr;
    uni_hid_device_t* device = NULL;
    char name_buffer[HID_MAX_NAME_LEN + 1] = {0};
    int name_len = 0;
    uint8_t rssi = 255;

    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    gap_event_inquiry_result_get_bd_addr(packet, addr);
    uint8_t page_scan_repetition_mode = gap_event_inquiry_result_get_page_scan_repetition_mode(packet);
    uint16_t clock_offset = gap_event_inquiry_result_get_clock_offset(packet);
    uint32_t cod = gap_event_inquiry_result_get_class_of_device(packet);

    logi("Device found: %s ", bd_addr_to_str(addr));
    logi("with COD: 0x%06x, ", (unsigned int)cod);
    logi("pageScan %d, ", page_scan_repetition_mode);
    logi("clock offset 0x%04x", clock_offset);
    if (gap_event_inquiry_result_get_rssi_available(packet)) {
        rssi = gap_event_inquiry_result_get_rssi(packet);
        logi(", rssi %u dBm", rssi);
    }
    if (gap_event_inquiry_result_get_name_available(packet)) {
        name_len = gap_event_inquiry_result_get_name_len(packet);
        memcpy(name_buffer, gap_event_inquiry_result_get_name(packet), name_len);
        name_buffer[name_len] = 0;
        logi(", name '%s'", name_buffer);
    }
    logi("\n");
    // As returned by BTStack, the bigger the RSSI number, the better, being 255 the closest possible (?).
    if (rssi < (255 - 100))
        logi("Device %s too far away, try moving it closer to Bluepad32 device\n", bd_addr_to_str(addr));

    if (uni_hid_device_is_cod_supported(cod)) {
        device = uni_hid_device_get_instance_for_address(addr);
        if (device) {
            if (device->conn.state == UNI_BT_CONN_STATE_DEVICE_READY) {
                // Could happen that the device is already connected (E.g: 8BitDo Arcade Stick in Switch mode).
                // If so, just ignore the inquiry result.
                return;
            }
            logi("Device already added, waiting (current state=0x%02x)...\n", device->conn.state);
        } else {
            // Device not found, create one.
            device = uni_hid_device_create(addr);
            if (device == NULL) {
                loge("\nError: cannot create device, no more available slots\n");
                return;
            }
            uni_bt_conn_set_state(&device->conn, UNI_BT_CONN_STATE_DEVICE_DISCOVERED);
            uni_hid_device_set_cod(device, cod);
            device->conn.page_scan_repetition_mode = page_scan_repetition_mode;
            device->conn.clock_offset = clock_offset | UNI_BT_CLOCK_OFFSET_VALID;

            if (name_len > 0 && !uni_hid_device_has_name(device)) {
                uni_hid_device_set_name(device, name_buffer);
                uni_bt_conn_set_state(&device->conn, UNI_BT_CONN_STATE_REMOTE_NAME_FETCHED);
            }
        }
        uni_bluetooth_process_fsm(device);
    }
}

static void on_l2cap_channel_opened(uint16_t channel, const uint8_t* packet, uint16_t size) {
    uint16_t psm;
    uint8_t status;
    uint16_t local_cid, remote_cid;
    uint16_t local_mtu, remote_mtu;
    hci_con_handle_t handle;
    bd_addr_t address;
    uni_hid_device_t* device;
    uint8_t incoming;

    ARG_UNUSED(size);

    logi("L2CAP_EVENT_CHANNEL_OPENED (channel=0x%04x)\n", channel);

    l2cap_event_channel_opened_get_address(packet, address);
    device = uni_hid_device_get_instance_for_address(address);
    if (device == NULL) {
        loge("on_l2cap_channel_opened: could not find device for address %s\n", bd_addr_to_str(address));
        return;
    }

    status = l2cap_event_channel_opened_get_status(packet);
    if (status) {
        logi("L2CAP Connection failed: 0x%02x.\n", status);
        // Practice showed that if the connections fails, just disconnect/remove
        // so that the connection can start again.
        if (status == L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY) {
            logi("Probably GAP-security-related issues. Set GAP security to 2\n");
        }
        logi("Removing key for device: %s.\n", bd_addr_to_str(address));
        gap_drop_link_key_for_bd_addr(device->conn.btaddr);
        uni_hid_device_disconnect(device);
        uni_hid_device_delete(device);
        /* 'device' is destroyed, don't use */
        return;
    }
    psm = l2cap_event_channel_opened_get_psm(packet);
    local_cid = l2cap_event_channel_opened_get_local_cid(packet);
    remote_cid = l2cap_event_channel_opened_get_remote_cid(packet);
    handle = l2cap_event_channel_opened_get_handle(packet);
    incoming = l2cap_event_channel_opened_get_incoming(packet);
    local_mtu = l2cap_event_channel_opened_get_local_mtu(packet);
    remote_mtu = l2cap_event_channel_opened_get_remote_mtu(packet);

    logi(
        "PSM: 0x%04x, local CID=0x%04x, remote CID=0x%04x, handle=0x%04x, "
        "incoming=%d, local MTU=%d, remote MTU=%d, addr=%s\n",
        psm, local_cid, remote_cid, handle, incoming, local_mtu, remote_mtu, bd_addr_to_str(address));

    switch (psm) {
        case PSM_HID_CONTROL:
            device->conn.control_cid = l2cap_event_channel_opened_get_local_cid(packet);
            logi("HID Control opened, cid 0x%02x\n", device->conn.control_cid);
            uni_bt_conn_set_state(&device->conn, UNI_BT_CONN_STATE_L2CAP_CONTROL_CONNECTED);
            break;
        case PSM_HID_INTERRUPT:
            device->conn.interrupt_cid = l2cap_event_channel_opened_get_local_cid(packet);
            logi("HID Interrupt opened, cid 0x%02x\n", device->conn.interrupt_cid);
            uni_bt_conn_set_state(&device->conn, UNI_BT_CONN_STATE_L2CAP_INTERRUPT_CONNECTED);

            // Set "connected" only after PSM_HID_INTERRUPT.
            uni_hid_device_connect(device);
            break;
        default:
            logi("Unknown PSM = 0x%02x\n", psm);
            break;
    }
    uni_bluetooth_process_fsm(device);
}

static void on_l2cap_channel_closed(uint16_t channel, const uint8_t* packet, uint16_t size) {
    uint16_t local_cid;
    uni_hid_device_t* device;

    ARG_UNUSED(size);

    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
    logi("L2CAP_EVENT_CHANNEL_CLOSED: 0x%04x (channel=0x%04x)\n", local_cid, channel);
    device = uni_hid_device_get_instance_for_cid(local_cid);
    if (device == NULL) {
        // Device might already been closed if the Control or Interrupt PSM was closed first.
        logi("Couldn't not find hid_device for cid = 0x%04x\n", local_cid);
        return;
    }
    uni_hid_device_disconnect(device);
    uni_hid_device_delete(device);
    /* device is destroyed after this call, don't use it */
}

static void on_l2cap_incoming_connection(uint16_t channel, const uint8_t* packet, uint16_t size) {
    bd_addr_t event_addr;
    uni_hid_device_t* device;
    uint16_t local_cid, remote_cid;
    uint16_t psm;
    hci_con_handle_t handle;

    ARG_UNUSED(size);

    // Incoming connections are always accepted, regardless whether Bluetooth scanning is disabled.

    psm = l2cap_event_incoming_connection_get_psm(packet);
    handle = l2cap_event_incoming_connection_get_handle(packet);
    local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
    remote_cid = l2cap_event_incoming_connection_get_remote_cid(packet);
    l2cap_event_incoming_connection_get_address(packet, event_addr);

    logi(
        "L2CAP_EVENT_INCOMING_CONNECTION (psm=0x%04x, local_cid=0x%04x, "
        "remote_cid=0x%04x, handle=0x%04x, "
        "channel=0x%04x, addr=%s\n",
        psm, local_cid, remote_cid, handle, channel, bd_addr_to_str(event_addr));

    l2cap_event_incoming_connection_get_address(packet, event_addr);
    device = uni_hid_device_get_instance_for_address(event_addr);

    if (device && device->conn.state == UNI_BT_CONN_STATE_DEVICE_READY) {
        // It could happen that a device "disconnects" without actually sending the
        // disconnect packet. So Bluepad32 thinks it is connected, while the gamepad not.
        // And if the gamepad tries to connect again, it will "conflict" the Bluepad32 state.
        // E.g: Xbox Wireless m1708 behaves like this
        logi("Device %s with an existing connection, disconnecting current connection\n", bd_addr_to_str(event_addr));
        uni_hid_device_disconnect(device);
        uni_hid_device_delete(device);
        /* device is destroyed after this call, don't use it */
        device = NULL;
    }

    switch (psm) {
        case PSM_HID_CONTROL:
            if (device == NULL) {
                device = uni_hid_device_create(event_addr);
                if (device == NULL) {
                    loge("ERROR: no more available free devices\n");
                    l2cap_decline_connection(channel);
                    break;
                }
            }
            l2cap_accept_connection(channel);
            uni_hid_device_set_connection_handle(device, handle);
            device->conn.control_cid = channel;
            uni_hid_device_set_incoming(device, true);
            break;
        case PSM_HID_INTERRUPT:
            if (device == NULL) {
                loge("Could not find device for PSM_HID_INTERRUPT = 0x%04x\n", channel);
                l2cap_decline_connection(channel);
                break;
            }
            device->conn.interrupt_cid = channel;
            l2cap_accept_connection(channel);
            break;
        default:
            logi("Unknown PSM = 0x%02x\n", psm);
    }
}

static void on_l2cap_data_packet(uint16_t channel, const uint8_t* packet, uint16_t size) {
    uni_hid_device_t* d;

    d = uni_hid_device_get_instance_for_cid(channel);
    if (d == NULL) {
        loge("on_l2cap_data_packet: invalid cid: 0x%04x, ignoring packet\n", channel);
        // printf_hexdump(packet, size);
        return;
    }

    // Sanity check. It must have at least a transaction type and a report id.
    if (size < 2) {
        loge("on_l2cap_data_packet: invalid packet size, ignoring packet\n");
        return;
    }

    if (channel == d->conn.control_cid) {
        // Feature report
        if (d->report_parser.parse_feature_report)
            // Skip the first byte which must be 0xa3
            d->report_parser.parse_feature_report(d, &packet[1], size - 1);
        return;
    }

    if (channel != d->conn.interrupt_cid) {
        loge("on_l2cap_data_packet: invalid interrupt CID: got 0x%02x, want: 0x%02x\n", channel, d->conn.interrupt_cid);
        return;
    }

    // It must be an input report
    // DATA | INPUT_REPORT: 0xa1
    if (packet[0] != ((HID_MESSAGE_TYPE_DATA << 4) | HID_REPORT_TYPE_INPUT)) {
        loge("on_l2cap_data_packet: unexpected transaction type: got 0x%02x, want: 0x0a1\n", packet[0]);
        printf_hexdump(packet, size);
        return;
    }

    // Skip the first byte, which is always 0xa1
    uni_hid_parse_input_report(d, &packet[1], size - 1);
    uni_hid_device_process_controller(d);
}

static void l2cap_create_control_connection(uni_hid_device_t* d) {
    uint8_t status;
    status = l2cap_create_channel(uni_bluetooth_packet_handler, d->conn.btaddr, BLUETOOTH_PSM_HID_CONTROL,
                                  UNI_BT_L2CAP_CHANNEL_MTU, &d->conn.control_cid);
    if (status) {
        loge("\nConnecting or Auth to HID Control failed: 0x%02x", status);
    } else {
        uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_L2CAP_CONTROL_CONNECTION_REQUESTED);
    }
}

static void l2cap_create_interrupt_connection(uni_hid_device_t* d) {
    uint8_t status;
    status = l2cap_create_channel(uni_bluetooth_packet_handler, d->conn.btaddr, BLUETOOTH_PSM_HID_INTERRUPT,
                                  UNI_BT_L2CAP_CHANNEL_MTU, &d->conn.interrupt_cid);
    if (status) {
        loge("\nConnecting or Auth to HID Interrupt failed: 0x%02x", status);
    } else {
        uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_L2CAP_INTERRUPT_CONNECTION_REQUESTED);
    }
}

static void inquiry_remote_name_timeout_callback(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = btstack_run_loop_get_timer_context(ts);
    loge("Failed to inquiry name for %s, using a fake one\n", bd_addr_to_str(d->conn.btaddr));
    // The device has no name. Just fake one
    uni_hid_device_set_name(d, "Controller without name");
    uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_REMOTE_NAME_FETCHED);
    uni_bluetooth_process_fsm(d);
}

static void bluetooth_del_keys(void) {
    bd_addr_t addr;
    link_key_t link_key;
    link_key_type_t type;
    btstack_link_key_iterator_t it;

    int ok = gap_link_key_iterator_init(&it);
    if (!ok) {
        loge("Link key iterator not implemented\n");
        return;
    }

    while (gap_link_key_iterator_get_next(&it, addr, link_key, &type)) {
        logi("Deleting key: %s - type %u - key: ", bd_addr_to_str(addr), (int)type);
        printf_hexdump(link_key, 16);
        gap_drop_link_key_for_bd_addr(addr);
    }
    gap_link_key_iterator_done(&it);
}

static void bluetooth_list_keys(void) {
    bd_addr_t addr;
    link_key_t link_key;
    link_key_type_t type;
    btstack_link_key_iterator_t it;

    int ok = gap_link_key_iterator_init(&it);
    if (!ok) {
        loge("Link key iterator not implemented\n");
        return;
    }

    logi("Bluetooth keys:\n");
    while (gap_link_key_iterator_get_next(&it, addr, link_key, &type)) {
        logi("%s - type %u - key: ", bd_addr_to_str(addr), (int)type);
        printf_hexdump(link_key, 16);
    }
    gap_link_key_iterator_done(&it);
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

static void cmd_callback(void* context) {
    uni_hid_device_t* d;
    uint32_t ctx = (uint32_t)context;
    uint16_t cmd = ctx & 0xffff;
    uint16_t args = ctx >> 16;

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
        default:
            loge("Unknown command: %#x\n", cmd);
            break;
    }
}

static uint8_t start_scan(void) {
    uint8_t status;
    logd("--> Scanning for new gamepads...\n");

    // BLE
    uni_ble_scan_start();

    // BR/EDR
    status =
        gap_inquiry_periodic_start(uni_bt_setup_get_gap_inquiry_lenght(), uni_bt_setup_get_gap_max_periodic_lenght(),
                                   uni_bt_setup_get_gap_min_periodic_lenght());
    if (status)
        loge("Error: cannot start inquiry (0x%02x), please try again\n", status);
    return status;
}

static uint8_t stop_scan(void) {
    uint8_t status;
    logi("--> Stop scanning for new gamepads\n");

    // BLE
    uni_ble_scan_stop();

    // BR/EDR
    status = gap_inquiry_stop();
    if (status)
        loge("Error: cannot stop inquiry (0x%02x), please try again\n", status);
    return status;
}

//
// Public functions
//

void uni_bluetooth_del_keys_safe(void) {
    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)CMD_BT_DEL_KEYS;
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

void uni_bluetooth_list_keys_safe(void) {
    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)CMD_BT_LIST_KEYS;
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

void uni_bluetooth_enable_new_connections_safe(bool enabled) {
    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)(enabled ? (intptr_t)CMD_BT_ENABLE : (intptr_t)CMD_BT_DISABLE);
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

void uni_bluetooth_dump_devices_safe(void) {
    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)CMD_DUMP_DEVICES;
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

void uni_bluetooth_disconnect_device_safe(int device_idx) {
    uint8_t idx = (uint8_t)device_idx;
    cmd_callback_registration.callback = &cmd_callback;
    cmd_callback_registration.context = (void*)(CMD_DISCONNECT_DEVICE | (idx << 16));
    btstack_run_loop_execute_on_main_thread(&cmd_callback_registration);
}

void uni_bluetooth_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    uint8_t event;
    bd_addr_t event_addr;
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
                    uni_ble_on_hci_event_le_meta(packet, size);
                    break;
                case HCI_EVENT_ENCRYPTION_CHANGE:
                    uni_ble_on_hci_event_encryption_change(packet, size);
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
                    bool is_mouse = false;

                    logi("--> HCI_EVENT_PIN_CODE_REQUEST\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    device = uni_hid_device_get_instance_for_address(event_addr);
                    if (!device) {
                        loge("Failed to get device for: %s, assuming it is not a mouse\n", bd_addr_to_str(event_addr));
                    } else {
                        uint32_t mouse_cod = UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE;
                        is_mouse = (device->cod & mouse_cod) == mouse_cod;
                    }

                    if (is_mouse) {
                        // For mice, use "0000" as pins, which seems to be the exected one.
                        logi("Using PIN code: '0000'\n");
                        gap_pin_code_response_binary(event_addr, (uint8_t*)"0000", 4);
                    } else {
                        // gap_pin_code_response_binary() does not copy the data, and data
                        // must be valid until the next hci_send_cmd is called.
                        static bd_addr_t pin_code;
                        bd_addr_t local_addr;

                        // FIXME: Assumes incoming connection from Nintendo Wii using Sync.
                        //
                        // From: https://wiibrew.org/wiki/Wiimote#Bluetooth_Pairing:
                        //  If connecting by holding down the 1+2 buttons, the PIN is the
                        //  bluetooth address of the wiimote backwards, if connecting by
                        //  pressing the "sync" button on the back of the wiimote, then the
                        //  PIN is the bluetooth address of the host backwards.
                        gap_local_bd_addr(local_addr);
                        reverse_bd_addr(local_addr, pin_code);
                        logi("Using PIN code: \n");
                        printf_hexdump(pin_code, sizeof(pin_code));
                        gap_pin_code_response_binary(event_addr, pin_code, sizeof(pin_code));
                    }
                    break;
                }
                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    logi("SSP User Confirmation Request with numeric value '%" PRIu32 "'\n",
                         little_endian_read_32(packet, 8));
                    logi("SSP User Confirmation Auto accept\n");
                    break;
                case HCI_EVENT_HID_META: {
                    logi("UNSUPPORTED ---> HCI_EVENT_HID_META <---\n");
                    uint8_t code = hci_event_hid_meta_get_subevent_code(packet);
                    logi("HCI HID META SUBEVENT: 0x%02x\n", code);
                    break;
                }
                case HCI_EVENT_INQUIRY_RESULT:
                    // logi("--> HCI_EVENT_INQUIRY_RESULT <--\n");
                    break;
                case HCI_EVENT_CONNECTION_REQUEST:
                    logi("--> HCI_EVENT_CONNECTION_REQUEST: link_type = %d <--\n",
                         hci_event_connection_request_get_link_type(packet));
                    on_hci_connection_request(channel, packet, size);
                    break;
                case HCI_EVENT_CONNECTION_COMPLETE:
                    logi("--> HCI_EVENT_CONNECTION_COMPLETE\n");
                    on_hci_connection_complete(channel, packet, size);
                    break;
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    logi("--> HCI_EVENT_DISCONNECTION_COMPLETE\n");
                    handle = hci_event_disconnection_complete_get_connection_handle(packet);
                    // Xbox Wireless Controller starts an incoming connection when told to
                    // enter in "discovery mode". If the connection fails (HCI_EVENT_DISCONNECTION_COMPLETE
                    // is generated) then it starts the discovery.
                    // So, just delete the possible-previous created entry. This highly increase
                    // the reliability with Xbox Wireless controllers.
                    device = uni_hid_device_get_instance_for_connection_handle(handle);
                    if (device) {
                        logi("Device %s disconnected, deleting it\n", bd_addr_to_str(device->conn.btaddr));
                        uni_hid_device_disconnect(device);
                        uni_hid_device_delete(device);
                        device = NULL;
                    }
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
                    logi("--> HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE\n");
                    hci_event_remote_name_request_complete_get_bd_addr(packet, event_addr);
                    device = uni_hid_device_get_instance_for_address(event_addr);
                    if (device != NULL) {
                        // FIXME: This must be a const char*
                        char* name = NULL;
                        status = hci_event_remote_name_request_complete_get_status(packet);
                        if (status) {
                            // Failed to get the name, just fake one
                            logi("Failed to fetch name for %s, error = 0x%02x\n", bd_addr_to_str(event_addr), status);
                            name = "Controller without name";
                        } else {
                            name = (char*)hci_event_remote_name_request_complete_get_remote_name(packet);
                        }
                        logi("Name: '%s'\n", name);
                        uni_hid_device_set_name(device, name);
                        // It could happen that the device is already connected, but the NAME_REQUEST
                        // has just finished. So, do not update the state:
                        // See: https://gitlab.com/ricardoquesada/bluepad32/-/issues/21
                        if (uni_bt_conn_get_state(&device->conn) < UNI_BT_CONN_STATE_DEVICE_PENDING_READY) {
                            // Only update state if the device is not already ready.
                            uni_bt_conn_set_state(&device->conn, UNI_BT_CONN_STATE_REMOTE_NAME_FETCHED);
                            uni_bluetooth_process_fsm(device);
                        }

                        // Remove timer
                        btstack_run_loop_remove_timer(&device->inquiry_remote_name_timer);
                    }
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
                    on_l2cap_incoming_connection(channel, packet, size);
                    break;
                case L2CAP_EVENT_CHANNEL_OPENED:
                    on_l2cap_channel_opened(channel, packet, size);
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    on_l2cap_channel_closed(channel, packet, size);
                    break;

                // GAP EVENTS
                case GAP_EVENT_INQUIRY_RESULT:
                    // logi("--> GAP_EVENT_INQUIRY_RESULT\n");
                    on_gap_inquiry_result(channel, packet, size);
                    break;
                case GAP_EVENT_INQUIRY_COMPLETE:
                    logd("--> GAP_EVENT_INQUIRY_COMPLETE\n");
                    // This can happen when "exit periodic inquiry" is called.
                    // Just do nothing, don't call "start_scan" again.
                    break;
                case GAP_EVENT_ADVERTISING_REPORT:
                    uni_ble_on_gap_event_advertising_report(packet, size);
                    break;
                // GATT EVENTS (BLE only)
                case GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
                    logd("--> GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT\n");
                    break;
                default:
                    break;
            }
            break;
        case L2CAP_DATA_PACKET:
            on_l2cap_data_packet(channel, packet, size);
            break;
        default:
            loge("unhandled packet type: 0x%02x\n", packet_type);
            break;
    }
}

void uni_bluetooth_process_fsm(uni_hid_device_t* d) {
    // A device can be in the following states:
    // - discovered (which might or might not have a name)
    // - received incoming connection
    // - establishing a connection
    // - fetching the name (in case it doesn't have one)
    // - SDP query to get VID/PID and HID descriptor
    //   Although the HID descriptor might not be needed on some devices
    // The order in which those states are executed vary from gamepad to gamepad
    uni_bt_conn_state_t state;

    // logi("uni_bluetooth_process_fsm: %p = 0x%02x\n", d, d->state);
    if (d == NULL) {
        loge("uni_bluetooth_process_fsm: Invalid device\n");
    }
    // Two possible flows:
    // - Incoming (initiated by gamepad)
    // - Or discovered (initiated by Bluepad32).

    state = uni_bt_conn_get_state(&d->conn);

    logi("uni_bluetooth_process_fsm, bd addr:%s,  state: %d, incoming:%d\n", bd_addr_to_str(d->conn.btaddr), state,
         uni_hid_device_is_incoming(d));

    // Does it have a name?
    // The name is fetched at the very beginning, when we initiate the connection,
    // Or at the very end, when it is an incoming connection.
    if (!uni_hid_device_has_name(d) &&
        ((state == UNI_BT_CONN_STATE_DEVICE_DISCOVERED) || state == UNI_BT_CONN_STATE_L2CAP_INTERRUPT_CONNECTED)) {
        logi("uni_bluetooth_process_fsm: requesting name\n");

        if (d->conn.clock_offset & UNI_BT_CLOCK_OFFSET_VALID)
            gap_remote_name_request(d->conn.btaddr, d->conn.page_scan_repetition_mode, d->conn.clock_offset);
        else
            gap_remote_name_request(d->conn.btaddr, 0x02, 0x0000);

        uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_REMOTE_NAME_INQUIRED);

        // Some devices might not respond to the name request
        btstack_run_loop_set_timer(&d->inquiry_remote_name_timer, INQUIRY_REMOTE_NAME_TIMEOUT_MS);
        btstack_run_loop_set_timer_context(&d->inquiry_remote_name_timer, d);
        btstack_run_loop_set_timer_handler(&d->inquiry_remote_name_timer, &inquiry_remote_name_timeout_callback);
        btstack_run_loop_add_timer(&d->inquiry_remote_name_timer);
        return;
    }

    if (state == UNI_BT_CONN_STATE_REMOTE_NAME_FETCHED) {
        // TODO: Move comparison to DS4 code
        if (strcmp("Wireless Controller", d->name) == 0) {
            logi("uni_bluetooth_process_fsm: gamepad is 'Wireless Controller', starting SDP query\n");
            d->sdp_query_type = SDP_QUERY_BEFORE_CONNECT;
            uni_bt_sdp_query_start(d);
            /* 'd' might be invalid */
            return;
        }

        if (uni_hid_device_guess_controller_type_from_name(d, d->name)) {
            logi("uni_bluetooth_process_fsm: Guess controller from name\n");
            d->sdp_query_type = SDP_QUERY_NOT_NEEDED;
            uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_SDP_HID_DESCRIPTOR_FETCHED);
        }

        if (uni_hid_device_is_incoming(d)) {
            if (d->sdp_query_type == SDP_QUERY_NOT_NEEDED) {
                logi("uni_bluetooth_process_fsm: Device is ready\n");
                uni_hid_device_set_ready(d);
            } else {
                logi("uni_bluetooth_process_fsm: starting SDP query\n");
                uni_bt_sdp_query_start(d);
                /* 'd' might be invalid */
            }
            return;
        }
        // else, not an incoming connection
        logi("uni_bluetooth_process_fsm: Starting L2CAP connection\n");
        l2cap_create_control_connection(d);
        return;
    }

    if (state == UNI_BT_CONN_STATE_SDP_VENDOR_FETCHED) {
        logi("uni_bluetooth_process_fsm: querying HID descriptor\n");
        uni_bt_sdp_query_start_hid_descriptor(d);
        return;
    }

    if (state == UNI_BT_CONN_STATE_SDP_HID_DESCRIPTOR_FETCHED) {
        if (uni_hid_device_is_incoming(d)) {
            uni_hid_device_set_ready(d);
            return;
        }

        // Not incoming
        if (d->sdp_query_type == SDP_QUERY_BEFORE_CONNECT) {
            logi("uni_bluetooth_process_fsm: Starting L2CAP connection\n");
            l2cap_create_control_connection(d);
        } else {
            logi("uni_bluetooth_process_fsm: Device is ready\n");
            uni_hid_device_set_ready(d);
        }
        return;
    }

    if (!uni_hid_device_is_incoming(d)) {
        if (state == UNI_BT_CONN_STATE_L2CAP_CONTROL_CONNECTED) {
            logi("uni_bluetooth_process_fsm: Create L2CAP interrupt connection\n");
            l2cap_create_interrupt_connection(d);
            return;
        }

        if (state == UNI_BT_CONN_STATE_L2CAP_INTERRUPT_CONNECTED) {
            switch (d->sdp_query_type) {
                case SDP_QUERY_BEFORE_CONNECT:
                case SDP_QUERY_NOT_NEEDED:
                    logi("uni_bluetooth_process_fsm: Device is ready\n");
                    uni_hid_device_set_ready(d);
                    break;
                case SDP_QUERY_AFTER_CONNECT:
                    logi("uni_bluetooth_process_fsm: starting SDP query\n");
                    uni_bt_sdp_query_start(d);
                    /* 'd' might be invalid */
                    break;
                default:
                    break;
            }
        }
    }
}
