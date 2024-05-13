// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "bt/uni_bt_bredr.h"

#include <inttypes.h>
#include <stdbool.h>

#include <btstack.h>

#include "sdkconfig.h"

#include "bt/uni_bt.h"
#include "bt/uni_bt_allowlist.h"
#include "bt/uni_bt_defines.h"
#include "bt/uni_bt_sdp.h"
#include "platform/uni_platform.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_log.h"

// These are the only two supported platforms with BR/EDR support.
#if !(defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_TARGET_POSIX) || defined(CONFIG_TARGET_PICO_W))
#error "This file can only be compiled for ESP32, Pico W, or Posix"
#endif

#define INQUIRY_REMOTE_NAME_TIMEOUT_MS 4500
_Static_assert(INQUIRY_REMOTE_NAME_TIMEOUT_MS < HID_DEVICE_CONNECTION_TIMEOUT_MS, "Timeout too big");

static bool bt_bredr_enabled = true;

static void l2cap_create_control_connection(uni_hid_device_t* d) {
    uint8_t status;
    status = l2cap_create_channel(uni_bt_packet_handler, d->conn.btaddr, BLUETOOTH_PSM_HID_CONTROL,
                                  UNI_BT_L2CAP_CHANNEL_MTU, &d->conn.control_cid);
    if (status) {
        loge("\nConnecting or Auth to HID Control failed: 0x%02x", status);
    } else {
        uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_L2CAP_CONTROL_CONNECTION_REQUESTED);
    }
}

static void l2cap_create_interrupt_connection(uni_hid_device_t* d) {
    uint8_t status;
    status = l2cap_create_channel(uni_bt_packet_handler, d->conn.btaddr, BLUETOOTH_PSM_HID_INTERRUPT,
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
    uni_bt_bredr_process_fsm(d);
}

void uni_bt_bredr_scan_start(void) {
    uint8_t status;

    status = gap_inquiry_periodic_start(uni_bt_get_gap_inquiry_length(), uni_bt_get_gap_max_periodic_length(),
                                        uni_bt_get_gap_min_periodic_length());
    if (status)
        loge("Failed to start period inquiry, error=0x%02x\n", status);
    logi("BR/EDR scan -> 1\n");
}

void uni_bt_bredr_scan_stop(void) {
    uint8_t status;

    status = gap_inquiry_stop();
    if (status)
        loge("Error: cannot stop inquiry (0x%02x), please try again\n", status);

    logi("BR/EDR scan -> 0\n");
}

// Called from uni_hid_device_disconnect()
void uni_bt_bredr_disconnect(uni_hid_device_t* d) {
    if (gap_get_connection_type(d->conn.handle) != GAP_CONNECTION_INVALID) {
        gap_disconnect(d->conn.handle);
        d->conn.handle = UNI_BT_CONN_HANDLE_INVALID;
    } else {
        // After calling gap_disconnect() we should not call l2cap_disonnect(),
        // since gap_disconnect() will take care of it.
        // But if the handle is not present, then call it manually.
        if (d->conn.control_cid) {
            l2cap_disconnect(d->conn.control_cid);
            d->conn.control_cid = 0;
        }

        if (d->conn.interrupt_cid) {
            l2cap_disconnect(d->conn.interrupt_cid);
            d->conn.interrupt_cid = 0;
        }
    }
}

void uni_bt_bredr_delete_bonded_keys(void) {
    bd_addr_t addr;
    link_key_t link_key;
    link_key_type_t type;
    btstack_link_key_iterator_t it;

    // BR/EDR
    int ok = gap_link_key_iterator_init(&it);
    if (!ok) {
        loge("Link key iterator not implemented\n");
        return;
    }

    logi("Deleting stored BR/EDR link keys:\n");
    while (gap_link_key_iterator_get_next(&it, addr, link_key, &type)) {
        logi("%s - type %u, key: ", bd_addr_to_str(addr), (int)type);
        printf_hexdump(link_key, 16);
        gap_drop_link_key_for_bd_addr(addr);
    }

    logi(".\n");
    gap_link_key_iterator_done(&it);
}

void uni_bt_bredr_list_bonded_keys(void) {
    bd_addr_t addr;
    link_key_t link_key;
    link_key_type_t type;
    btstack_link_key_iterator_t it;

    int ok = gap_link_key_iterator_init(&it);
    if (!ok) {
        loge("Link key iterator not implemented\n");
        return;
    }

    logi("Bluetooth BR/EDR keys:\n");
    while (gap_link_key_iterator_get_next(&it, addr, link_key, &type)) {
        logi("%s - type %u - key: ", bd_addr_to_str(addr), (int)type);
        printf_hexdump(link_key, 16);
    }
    gap_link_key_iterator_done(&it);
}

void uni_bt_bredr_setup(void) {
    int security_level = uni_bt_get_gap_security_level();
    gap_set_security_level(security_level);

    gap_connectable_control(1);

    // Enable once we add support for "BP32 BT Service"
    gap_discoverable_control(0);

    gap_set_page_scan_type(PAGE_SCAN_MODE_INTERLACED);
    // gap_set_page_timeout(0x2000);
    // gap_set_page_scan_activity(0x50, 0x12);
    // gap_inquiry_set_scan_activity(0x50, 0x12);

    // Needed for some incoming connections
    uni_bt_sdp_server_init();

    l2cap_register_service(uni_bt_packet_handler, BLUETOOTH_PSM_HID_INTERRUPT, UNI_BT_L2CAP_CHANNEL_MTU,
                           security_level);
    l2cap_register_service(uni_bt_packet_handler, BLUETOOTH_PSM_HID_CONTROL, UNI_BT_L2CAP_CHANNEL_MTU, security_level);

    // Allow sniff mode requests by HID device and support role switch
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

    // Enable RSSI and EIR for gap_inquiry
    // TODO: Do we need EIR, since the name will be requested if not provided?
    hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);

    // try to become master on incoming connections
    hci_set_master_slave_policy(HCI_ROLE_MASTER);

    logi("Gap security level: %d\n", security_level);
    logi("Periodic Inquiry: max=%d, min=%d, len=%d\n", uni_bt_get_gap_max_periodic_length(),
         uni_bt_get_gap_min_periodic_length(), uni_bt_get_gap_inquiry_length());
}

void uni_bt_bredr_set_enabled(bool enabled) {
    bt_bredr_enabled = enabled;
}

bool uni_bt_bredr_is_enabled(void) {
    return bt_bredr_enabled;
}

void uni_bt_bredr_process_fsm(uni_hid_device_t* d) {
    // TODO: Move to uni_bt_bredr.c

    // A device can be in the following states:
    // - discovered (which might or might not have a name)
    // - received incoming connection
    // - establishing a connection
    // - fetching the name (in case it doesn't have one)
    // - SDP query to get VID/PID and HID descriptor
    //   Although the HID descriptor might not be needed on some devices
    // The order in which those states are executed vary from gamepad to gamepad
    uni_bt_conn_state_t state;

    // logi("uni_bt_process_fsm: %p = 0x%02x\n", d, d->state);
    if (d == NULL) {
        loge("uni_bt_process_fsm: Invalid device\n");
    }
    // Two possible flows:
    // - Incoming (initiated by gamepad)
    // - Or discovered (initiated by Bluepad32).

    state = uni_bt_conn_get_state(&d->conn);

    logi("uni_bt_process_fsm, bd addr:%s,  state: %d, incoming:%d\n", bd_addr_to_str(d->conn.btaddr), state,
         uni_hid_device_is_incoming(d));

    // Does it have a name?
    // The name is fetched at the very beginning, when we initiate the connection,
    // Or at the very end, when it is an incoming connection.
    if (!uni_hid_device_has_name(d) &&
        ((state == UNI_BT_CONN_STATE_DEVICE_DISCOVERED) || state == UNI_BT_CONN_STATE_L2CAP_INTERRUPT_CONNECTED)) {
        logi("uni_bt_process_fsm: requesting name\n");

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
            logi("uni_bt_process_fsm: gamepad is 'Wireless Controller', starting SDP query\n");
            d->sdp_query_type = SDP_QUERY_BEFORE_CONNECT;
            uni_bt_sdp_query_start(d);
            /* 'd' might be invalid */
            return;
        }

        if (uni_hid_device_guess_controller_type_from_name(d, d->name)) {
            logi("uni_bt_process_fsm: Guess controller from name\n");
            d->sdp_query_type = SDP_QUERY_NOT_NEEDED;
            uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_SDP_HID_DESCRIPTOR_FETCHED);
        }

        if (uni_hid_device_is_incoming(d)) {
            if (d->sdp_query_type == SDP_QUERY_NOT_NEEDED) {
                logi("uni_bt_process_fsm: Device is ready\n");
                uni_hid_device_set_ready(d);
            } else {
                logi("uni_bt_process_fsm: starting SDP query\n");
                uni_bt_sdp_query_start(d);
                /* 'd' might be invalid */
            }
            return;
        }
        // else, not an incoming connection
        logi("uni_bt_process_fsm: Starting L2CAP connection\n");
        l2cap_create_control_connection(d);
        return;
    }

    if (state == UNI_BT_CONN_STATE_SDP_VENDOR_FETCHED) {
        logi("uni_bt_process_fsm: querying HID descriptor\n");
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
            logi("uni_bt_process_fsm: Starting L2CAP connection\n");
            l2cap_create_control_connection(d);
        } else {
            logi("uni_bt_process_fsm: Device is ready\n");
            uni_hid_device_set_ready(d);
        }
        return;
    }

    if (!uni_hid_device_is_incoming(d)) {
        if (state == UNI_BT_CONN_STATE_L2CAP_CONTROL_CONNECTED) {
            logi("uni_bt_process_fsm: Create L2CAP interrupt connection\n");
            l2cap_create_interrupt_connection(d);
            return;
        }

        if (state == UNI_BT_CONN_STATE_L2CAP_INTERRUPT_CONNECTED) {
            switch (d->sdp_query_type) {
                case SDP_QUERY_BEFORE_CONNECT:
                case SDP_QUERY_NOT_NEEDED:
                    logi("uni_bt_process_fsm: Device is ready\n");
                    uni_hid_device_set_ready(d);
                    break;
                case SDP_QUERY_AFTER_CONNECT:
                    logi("uni_bt_process_fsm: starting SDP query\n");
                    uni_bt_sdp_query_start(d);
                    /* 'd' might be invalid */
                    break;
                default:
                    break;
            }
        }
    }
}

void uni_bt_bredr_on_l2cap_incoming_connection(uint16_t channel, const uint8_t* packet, uint16_t size) {
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

    if (!uni_bt_allowlist_is_allowed_addr(event_addr)) {
        loge("Declining incoming connection: Device not in allow-list: %s\n", bd_addr_to_str(event_addr));
        l2cap_decline_connection(channel);
        return;
    }

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

void uni_bt_bredr_on_l2cap_channel_opened(uint16_t channel, const uint8_t* packet, uint16_t size) {
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
        // Practice showed that if the connection fails, just disconnect/remove
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
    uni_bt_bredr_process_fsm(device);
}

void uni_bt_bredr_on_l2cap_channel_closed(uint16_t channel, const uint8_t* packet, uint16_t size) {
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

void uni_bt_bredr_on_l2cap_data_packet(uint16_t channel, const uint8_t* packet, uint16_t size) {
    uni_hid_device_t* d;

    d = uni_hid_device_get_instance_for_cid(channel);
    if (d == NULL) {
        loge("on_l2cap_data_packet: invalid cid: 0x%04x, ignoring packet\n", channel);
        // printf_hexdump(packet, size);
        return;
    }

    // Sanity check. It must have at least a transaction type and a report id.
    if (size < 2) {
        // Might happen with certain gamepads like DS3 that sends a "0" after enabling rumble.
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

void uni_bt_bredr_on_gap_inquiry_result(uint16_t channel, const uint8_t* packet, uint16_t size) {
    bd_addr_t addr;
    uni_hid_device_t* d = NULL;
    char name_buffer[HID_MAX_NAME_LEN + 1] = {0};
    int name_len = 0;
    uint8_t rssi = 255;
    bool supported;

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

    supported = uni_hid_device_on_device_discovered(addr, name_buffer, cod, rssi) == UNI_ERROR_SUCCESS;
    if (supported) {
        d = uni_hid_device_get_instance_for_address(addr);
        if (d) {
            if (d->conn.state == UNI_BT_CONN_STATE_DEVICE_READY) {
                // Could happen that the device is already connected (E.g: 8BitDo Arcade Stick in Switch mode).
                // If so, just ignore the inquiry result.
                return;
            }
            logi("Device already added, waiting (current state=0x%02x)...\n", d->conn.state);
        } else {
            // Device not found, create one.
            d = uni_hid_device_create(addr);
            if (d == NULL) {
                loge("\nError: cannot create device, no more available slots\n");
                return;
            }
            uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_DEVICE_DISCOVERED);
            uni_hid_device_set_cod(d, cod);
            d->conn.page_scan_repetition_mode = page_scan_repetition_mode;
            d->conn.clock_offset = clock_offset | UNI_BT_CLOCK_OFFSET_VALID;
            d->conn.rssi = rssi;

            if (name_len > 0 && !uni_hid_device_has_name(d)) {
                uni_hid_device_set_name(d, name_buffer);
                uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_REMOTE_NAME_FETCHED);
            }
        }
        uni_bt_bredr_process_fsm(d);
    }
}

void uni_bt_bredr_on_hci_connection_request(uint16_t channel, const uint8_t* packet, uint16_t size) {
    bd_addr_t event_addr;
    uint32_t cod;
    uni_hid_device_t* d;

    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    hci_event_connection_request_get_bd_addr(packet, event_addr);
    cod = hci_event_connection_request_get_class_of_device(packet);

    d = uni_hid_device_get_instance_for_address(event_addr);
    if (d == NULL) {
        d = uni_hid_device_create(event_addr);
        if (d == NULL) {
            logi("Cannot create new device... no more slots available\n");
            return;
        }
    }
    uni_hid_device_set_cod(d, cod);
    uni_hid_device_set_incoming(d, true);
    logi("on_hci_connection_request from: address = %s, cod=0x%04x\n", bd_addr_to_str(event_addr), cod);
}

void uni_bt_bredr_on_hci_connection_complete(uint16_t channel, const uint8_t* packet, uint16_t size) {
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

void uni_bt_bredr_on_hci_disconnection_complete(uint16_t channel, const uint8_t* packet, uint16_t size) {
    ARG_UNUSED(channel);
    ARG_UNUSED(packet);
    ARG_UNUSED(size);

    // Do something ???
}

void uni_bt_bredr_on_hci_pin_code_request(uint16_t channel, const uint8_t* packet, uint16_t size) {
    // gap_pin_code_response_binary() does not copy the data, and data
    // must be valid until the next hci_send_cmd is called.
    static bd_addr_t pin_code;
    bd_addr_t local_addr;
    uni_hid_device_t* d;
    bd_addr_t event_addr;

    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    // TODO: Move to uni_bt_bredr.c
    bool is_mouse_or_keyboard = false;

    logi("--> HCI_EVENT_PIN_CODE_REQUEST\n");
    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
    d = uni_hid_device_get_instance_for_address(event_addr);
    if (!d) {
        loge("Failed to get device for: %s, assuming it is not a mouse\n", bd_addr_to_str(event_addr));
    } else {
        is_mouse_or_keyboard =
            ((d->cod & UNI_BT_COD_MAJOR_MASK) == UNI_BT_COD_MAJOR_PERIPHERAL) &&  // Is it a peripheral ?
            (d->cod & UNI_BT_COD_MINOR_KEYBOARD_AND_MICE);                        // and is it a mouse or keyboard ?
    }

    if (is_mouse_or_keyboard) {
        // For mouse/keyboard, use "0000" as pins, which seems to be the expected one.
        // "1234" could also be a valid pin.
        logi("Using PIN code: '0000'\n");
        gap_pin_code_response_binary(event_addr, (uint8_t*)"0000", 4);
    } else {
        // FIXME: Assumes incoming connection from Nintendo Wii using Sync.
        // Move as a plugin to Wii code.
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
}

void uni_bt_bredr_on_hci_remote_name_request_complete(uint16_t channel, const uint8_t* packet, uint16_t size) {
    uni_hid_device_t* d;
    uint8_t status;
    bd_addr_t event_addr;

    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    logi("--> HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE\n");
    hci_event_remote_name_request_complete_get_bd_addr(packet, event_addr);
    d = uni_hid_device_get_instance_for_address(event_addr);
    if (d != NULL) {
        const char* name = NULL;
        status = hci_event_remote_name_request_complete_get_status(packet);
        if (status) {
            // Failed to get the name, just fake one
            logi("Failed to fetch name for %s, error = 0x%02x\n", bd_addr_to_str(event_addr), status);
            name = "Controller without name";
        } else {
            name = hci_event_remote_name_request_complete_get_remote_name(packet);
        }
        logi("Name: '%s'\n", name);
        uni_hid_device_set_name(d, name);
        // It could happen that the device is already connected, but the NAME_REQUEST
        // has just finished. So, do not update the state:
        // See: https://gitlab.com/ricardoquesada/bluepad32/-/issues/21
        if (uni_bt_conn_get_state(&d->conn) < UNI_BT_CONN_STATE_DEVICE_PENDING_READY) {
            // Only update state if the device is not already ready.
            uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_REMOTE_NAME_FETCHED);
            uni_bt_bredr_process_fsm(d);
        }

        // Remove timer
        btstack_run_loop_remove_timer(&d->inquiry_remote_name_timer);
    }
}
