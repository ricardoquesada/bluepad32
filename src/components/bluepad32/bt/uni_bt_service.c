// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "bt/uni_bt_service.h"

#include <btstack.h>

#include "bt/uni_bt.h"
#include "bt/uni_bt_allowlist.h"
#include "bt/uni_bt_le.h"
#include "bt/uni_bt_service.gatt.h"
#include "controller/uni_gamepad.h"
#include "uni_common.h"
#include "uni_log.h"
#include "uni_system.h"
#include "uni_version.h"
#include "uni_virtual_device.h"

// General Discoverable = 0x02
// BR/EDR Not supported = 0x04
#define APP_AD_FLAGS 0x06

// Max number of clients that can connect to the service at the same time.
#define MAX_NR_CLIENT_CONNECTIONS 1

// Don't know how to increate MTU for notification, so use the minimum which is 20 (23 - 3)
#define NOTIFICATION_MTU 20

// Struct sent to the BLE client
// A compact version of uni_hid_device_t.
typedef struct __attribute((packed)) {
    uint8_t idx;          // device index number: 0...CONFIG_BLUEPAD32_MAX_DEVICES-1
    bd_addr_t addr;       // 6 bytes
    uint16_t vendor_id;   // 2 bytes
    uint16_t product_id;  // 2 bytes
    uint8_t state;
    uint8_t incoming;
    uint16_t controller_type;
    uni_controller_subtype_t controller_subtype;
} compact_device_t;
_Static_assert(sizeof(compact_device_t) <= NOTIFICATION_MTU, "compact_device_t too big");

// client connection
typedef struct {
    bool notification_enabled;
    uint16_t value_handle;
    hci_con_handle_t connection_handle;
} client_connection_t;
static client_connection_t client_connections[MAX_NR_CLIENT_CONNECTIONS];

// Iterate all over the connected clients, but only one is supported. Hardcoded to 0, don't change.
static int notification_connection_idx;
// Iterate all over the connected controllers
static int notification_device_idx;

static compact_device_t compact_devices[CONFIG_BLUEPAD32_MAX_DEVICES];
static bool service_enabled;

// clang-format off
static const uint8_t adv_data[] = {
    // Flags general discoverable
    2, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    // Name
    5, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,'B', 'P', '3', '2',
    // 4627C4A4-AC00-46B9-B688-AFC5C1BF7F63
    17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS,
    0x63, 0x7F, 0xBF, 0xC1, 0xC5, 0xAF, 0x88, 0xB6, 0xB9, 0x46, 0x00, 0xAC, 0xA4, 0xC4, 0x27, 0x46,
};
_Static_assert(sizeof(adv_data) <= 31, "adv_data too big");
// clang-format on
static const int adv_data_len = sizeof(adv_data);

static void att_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);
static int att_write_callback(hci_con_handle_t con_handle,
                              uint16_t att_handle,
                              uint16_t transaction_mode,
                              uint16_t offset,
                              uint8_t* buffer,
                              uint16_t buffer_size);
static uint16_t att_read_callback(hci_con_handle_t conn_handle,
                                  uint16_t att_handle,
                                  uint16_t offset,
                                  uint8_t* buffer,
                                  uint16_t buffer_size);
static client_connection_t* connection_for_conn_handle(hci_con_handle_t conn_handle);
static bool next_notify_device(void);
static void notify_client(void);
static void maybe_notify_client();

static bool is_notify_client_valid(void) {
    return ((client_connections[notification_connection_idx].connection_handle != HCI_CON_HANDLE_INVALID) &&
            (client_connections[notification_connection_idx].notification_enabled));
}

static bool next_notify_device(void) {
    // TODO: For simplicity, we send all 4 devices.
    // But we should only send the connected ones.
    notification_device_idx++;
    if (notification_device_idx == CONFIG_BLUEPAD32_MAX_DEVICES) {
        notification_device_idx = 0;
        return true;
    }
    return false;
}

static void notify_client(void) {
    logd("Notifying client idx = %d, device idx = %d\n", notification_connection_idx, notification_device_idx);
    uint8_t status;
    client_connection_t* ctx;
    bool finish_round;

    if (!is_notify_client_valid())
        return;

    ctx = &client_connections[notification_connection_idx];

    // send
    status = att_server_notify(ctx->connection_handle, ctx->value_handle,
                               (const uint8_t*)&compact_devices[notification_device_idx], sizeof(compact_devices[0]));
    if (status != ERROR_CODE_SUCCESS) {
        loge("BLE Service: Failed to notify client, error: %#x\n", status);
    }

    finish_round = next_notify_device();
    if (!finish_round)
        att_server_request_can_send_now_event(ctx->connection_handle);
}

static void maybe_notify_client(void) {
    client_connection_t* ctx = NULL;

    for (int i = 0; i < MAX_NR_CLIENT_CONNECTIONS; i++) {
        if (client_connections[i].connection_handle != HCI_CON_HANDLE_INVALID &&
            client_connections[i].notification_enabled) {
            ctx = &client_connections[i];
            break;
        }
    }
    if (ctx)
        att_server_request_can_send_now_event(ctx->connection_handle);
}

static int att_write_callback(hci_con_handle_t con_handle,
                              uint16_t att_handle,
                              uint16_t transaction_mode,
                              uint16_t offset,
                              uint8_t* buffer,
                              uint16_t buffer_size) {
    ARG_UNUSED(transaction_mode);

    logd("att_write_callback: con handle=%#x, att_handle=%#x, offset=%d\n", con_handle, att_handle, offset);
    //    printf_hexdump(buffer, buffer_size);

    client_connection_t* ctx;

    switch (att_handle) {
        case ATT_CHARACTERISTIC_4627C4A4_AC03_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Whether to enable BLE connections
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool enabled = buffer[0];
            uni_bt_le_set_enabled(enabled);
            return ATT_ERROR_SUCCESS;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC04_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Scan for new connections
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool enabled = buffer[0];
            uni_bt_enable_new_connections_unsafe(enabled);
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC06_46B9_B688_AFC5C1BF7F63_01_CLIENT_CONFIGURATION_HANDLE: {
            // Notify connected devices
            ctx = connection_for_conn_handle(con_handle);
            if (!ctx)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            ctx->notification_enabled =
                little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
            ctx->value_handle = ATT_CHARACTERISTIC_4627C4A4_AC06_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE;
            if (ctx->notification_enabled)
                att_server_request_can_send_now_event(ctx->connection_handle);

            logi("BLE Service: Notification enabled = %d for handle %#x\n", ctx->notification_enabled,
                 ctx->connection_handle);
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC07_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Mappings: Nintendo or Xbox: A,B,X,Y vs B,A,Y,X
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            uint8_t type = buffer[0];
            if (type >= UNI_GAMEPAD_MAPPINGS_TYPE_COUNT)
                return 0;
            uni_gamepad_set_mappings_type(type);
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC08_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Whether to enable Allowlist in connections
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool enabled = buffer[0];
            uni_bt_allowlist_set_enabled(enabled);
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC09_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // TODO
            // List of addresses in the allowlist.
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC0A_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Whether to enable Virtual Devices
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool enabled = buffer[0];
            uni_virtual_device_set_enabled(enabled);
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC0B_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Disconnect a device
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            int idx = buffer[0];
            if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            uni_hid_device_t* d = uni_hid_device_get_instance_for_idx(idx);
            uni_hid_device_disconnect(d);
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC0C_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Delete stored Bluetooth bond keys
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool delete = buffer[0];
            if (!delete)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            uni_bt_del_keys_unsafe();
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC0D_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Reset device
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool reset = buffer[0];
            if (!reset)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            uni_system_reboot();
            break;
        }
        default:
            logi("BLE Service: Unsupported write to 0x%04x, len %u\n", att_handle, buffer_size);
            return ATT_ERROR_ATTRIBUTE_NOT_FOUND;
    }
    return ATT_ERROR_SUCCESS;
}

static uint16_t att_read_callback(hci_con_handle_t conn_handle,
                                  uint16_t att_handle,
                                  uint16_t offset,
                                  uint8_t* buffer,
                                  uint16_t buffer_size) {
    ARG_UNUSED(conn_handle);

    switch (att_handle) {
        case ATT_CHARACTERISTIC_4627C4A4_AC01_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // version
            return att_read_callback_handle_blob((const uint8_t*)uni_version, (uint16_t)strlen(uni_version), offset,
                                                 buffer, buffer_size);
        case ATT_CHARACTERISTIC_4627C4A4_AC02_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Max supported connections
            const uint8_t max = CONFIG_BLUEPAD32_MAX_DEVICES;
            return att_read_callback_handle_blob(&max, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC03_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Whether to enable BLE connections
            const uint8_t enabled = uni_bt_le_is_enabled();
            return att_read_callback_handle_blob(&enabled, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC04_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Scan for new connections
            const uint8_t scanning = uni_bt_enable_new_connections_is_enabled();
            return att_read_callback_handle_blob(&scanning, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC05_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // Connected devices
            return att_read_callback_handle_blob((const void*)compact_devices, (uint16_t)sizeof(compact_devices),
                                                 offset, buffer, buffer_size);
        case ATT_CHARACTERISTIC_4627C4A4_AC06_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // Notify all connected devices, only when there is a change, one at a time.
            // Ideally it should be merged with the previous one, but don't know how to increase
            // the MTU for the notify package.
            // Notify only. Read not supported.
            loge("BLE Service: 4627C4A4_AC06_46B9_B688_AFC5C1BF7F63 does not support read\n");
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC07_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Mappings type: Xbox, Nintendo or custom
            const uint8_t mappings_type = uni_gamepad_get_mappings_type();
            return att_read_callback_handle_blob(&mappings_type, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC08_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Whether to enable Allowlist in connections
            // Scan for new connections
            const uint8_t allowlist_enabled = uni_bt_allowlist_is_enabled();
            return att_read_callback_handle_blob(&allowlist_enabled, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC09_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // List of addresses in the allowlist.
            const bd_addr_t* addresses;
            int total;
            uni_bt_allowlist_get_all(&addresses, &total);
            return att_read_callback_handle_blob((const uint8_t*)addresses, (uint16_t)sizeof(bd_addr_t) * total, offset,
                                                 buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC0A_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Whether to enable Virtual Devices
            const uint8_t virtual_enabled = uni_virtual_device_is_enabled();
            return att_read_callback_handle_blob(&virtual_enabled, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC0B_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // Disconnect a device
            loge("BLE Service: 4627C4A4_AC0B_46B9_B688_AFC5C1BF7F63 does not support read\n");
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC0C_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // Delete stored Bluetooth bond keys
            loge("BLE Service: 4627C4A4_AC0C_46B9_B688_AFC5C1BF7F63 does not support read\n");
            break;

        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_01_CLIENT_CONFIGURATION_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_MANUFACTURER_NAME_STRING_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_MODEL_NUMBER_STRING_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_SERIAL_NUMBER_STRING_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_HARDWARE_REVISION_STRING_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_FIRMWARE_REVISION_STRING_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_SOFTWARE_REVISION_STRING_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_SYSTEM_ID_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_IEEE_11073_20601_REGULATORY_CERTIFICATION_DATA_LIST_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_PNP_ID_01_VALUE_HANDLE:
            break;
        default:
            break;
    }
    return 0;
}

static client_connection_t* connection_for_conn_handle(hci_con_handle_t conn_handle) {
    int i;
    for (i = 0; i < MAX_NR_CLIENT_CONNECTIONS; i++) {
        if (client_connections[i].connection_handle == conn_handle)
            return &client_connections[i];
    }
    return NULL;
}

static void att_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    client_connection_t* ctx;
    int mtu;

    if (packet_type != HCI_EVENT_PACKET)
        return;

    switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CONNECTED:
            // setup new
            ctx = connection_for_conn_handle(HCI_CON_HANDLE_INVALID);
            if (!ctx)
                break;
            ctx->connection_handle = att_event_connected_get_handle(packet);
            mtu = att_server_get_mtu(ctx->connection_handle);
            logi("BLE Service: New client connected handle = %#x, mtu = %d\n", ctx->connection_handle, mtu);
            break;
        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
            mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3;
            ctx = connection_for_conn_handle(att_event_mtu_exchange_complete_get_handle(packet));
            if (!ctx)
                break;
            break;
        case ATT_EVENT_CAN_SEND_NOW:
            printf_hexdump(packet, size);
            notify_client();
            break;
        case ATT_EVENT_DISCONNECTED:
            ctx = connection_for_conn_handle(att_event_disconnected_get_handle(packet));
            if (!ctx)
                break;
            logi("BLE Service: client disconnected, handle = %#x\n", ctx->connection_handle);
            memset(ctx, 0, sizeof(*ctx));
            ctx->connection_handle = HCI_CON_HANDLE_INVALID;
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // Do something?
            break;
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    // Deprecated. Replaced by ATT_EVENT_CONNECTED
                    break;
                default:
                    logi("Unsupported HCI_EVENT_LE_META: %#x\n", hci_event_le_meta_get_subevent_code(packet));
                    break;
            }
            break;
        default:
            logi("BLE Service: Unsupported ATT_EVENT: %#x\n", hci_event_packet_get_type(packet));
            break;
    }
}

void uni_bt_service_deinit(void) {
    att_server_deinit();
    gap_advertisements_enable(false);
}

/*
 * Configures the ATT Server with the pre-compiled ATT Database generated from the .gatt file.
 * Finally, it configures the advertisements.
 */
void uni_bt_service_init(void) {
    logi("Starting Bluepad32 BLE service UUID: 4627C4A4-AC00-46B9-B688-AFC5C1BF7F63\n");

    // Setup ATT server.
    att_server_init(profile_data, att_read_callback, att_write_callback);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;

    memset(null_addr, 0, 6);
    memset(compact_devices, 0, sizeof(compact_devices));
    memset(&client_connections, 0, sizeof(client_connections));
    for (int i = 0; i < MAX_NR_CLIENT_CONNECTIONS; i++)
        client_connections[i].connection_handle = HCI_CON_HANDLE_INVALID;
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++)
        compact_devices[i].idx = i;

    // register for ATT events
    att_server_register_packet_handler(att_packet_handler);

    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*)adv_data);
    gap_advertisements_enable(true);
}

bool uni_bt_service_is_enabled() {
    return service_enabled;
}

void uni_bt_service_set_enabled(bool enabled) {
    if (enabled == service_enabled)
        return;

    service_enabled = enabled;

    if (service_enabled)
        uni_bt_service_init();
    else
        uni_bt_service_deinit();
}

void uni_bt_service_on_device_ready(const uni_hid_device_t* d) {
    // Must be called from BTstack task
    if (!d)
        return;
    if (!service_enabled)
        return;

    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0)
        return;

    // Update the things that could have changed from "on_device_connected" callback.
    compact_devices[idx].controller_subtype = d->controller_subtype;
    compact_devices[idx].state = d->conn.connected;

    maybe_notify_client();
}

void uni_bt_service_on_device_connected(const uni_hid_device_t* d) {
    // Must be called from BTstack task
    if (!d)
        return;
    if (!service_enabled)
        return;

    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0)
        return;
    compact_devices[idx].vendor_id = d->vendor_id;
    compact_devices[idx].product_id = d->product_id;
    compact_devices[idx].controller_type = d->controller_type;
    compact_devices[idx].controller_subtype = d->controller_subtype;
    memcpy(compact_devices[idx].addr, d->conn.btaddr, 6);
    compact_devices[idx].state = d->conn.state;
    compact_devices[idx].incoming = d->conn.incoming;

    maybe_notify_client();
}

void uni_bt_service_on_device_disconnected(const uni_hid_device_t* d) {
    // Must be called from BTstack task
    if (!d)
        return;
    if (!service_enabled)
        return;

    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0)
        return;
    memset(&compact_devices[idx], 0, sizeof(compact_devices[0]));
    compact_devices[idx].idx = idx;

    maybe_notify_client();
}
