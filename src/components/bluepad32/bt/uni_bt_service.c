// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

// Bluepad32 BLE GATT Server Implementation (UUID: 4627C4A4-AC00-46B9-B688-AFC5C1BF7F63).
//
// Exposes runtime telemetry and configuration of the Bluepad32 host to companion
// BLE clients (such as `bluepad32-ble-client`).
//
// Key protocol invariants:
// 1. Connected Controllers (`AC05`, READ | NOTIFY | DYNAMIC) serializes each
//    device slot as a deterministic 16-byte little-endian `compact_device_t`.
// 2. When a subscribed central negotiates an ATT MTU capable of carrying the
//    entire table (`mtu >= sizeof(compact_devices) + 3`, i.e., >= 67 bytes for
//    4 slots), `notify_client()` transmits all slots in a single ATT notification.
//    If the central remains at the default 23-byte ATT MTU, `notify_client()`
//    falls back to streaming one 16-byte slot per `ATT_EVENT_CAN_SEND_NOW` event.
// 3. `populate_compact_device()` is the single source of truth for synchronizing
//    `compact_devices[]` across `init`, `connected`, `ready`, and `disconnected`
//    lifecycle transitions.

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

// Wire representation of a single controller slot transmitted over characteristic AC05.
//
// Binary layout (16 bytes, packed, little-endian):
//   Offset  Size  Type       Field               Description
//   0       1     uint8_t    idx                 Slot index (0 .. CONFIG_BLUEPAD32_MAX_DEVICES - 1)
//   1..6    6     bd_addr_t  addr                Bluetooth MAC address (all-zero when unoccupied)
//   7..8    2     uint16_t   vendor_id           USB/Bluetooth Vendor ID (little-endian)
//   9..10   2     uint16_t   product_id          USB/Bluetooth Product ID (little-endian)
//   11      1     uint8_t    state               Connection lifecycle state (uni_bt_conn_state_t)
//   12      1     uint8_t    incoming            0 = outgoing connection, non-zero = incoming
//   13..14  2     uint16_t   controller_type     Controller classification (uni_controller_type_t)
//   15      1     uint8_t    controller_subtype  Attachment subtype (uni_controller_subtype_t)
//
// Note: Fixed-width integer types (`uint8_t` / `uint16_t`) are used instead of C `enum`
// types because standard C enums are 4 bytes on toolchains without `-fshort-enums`,
// which would otherwise inflate the struct to 19 bytes and break cross-platform wire ABI.
typedef struct __attribute((packed)) {
    uint8_t idx;
    bd_addr_t addr;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t state;
    uint8_t incoming;
    uint16_t controller_type;
    uint8_t controller_subtype;
} compact_device_t;
_Static_assert(sizeof(compact_device_t) == 16, "compact_device_t must be 16 bytes");

// Per-client GATT connection state and notification subscription metadata.
typedef struct {
    bool notification_enabled;
    uint16_t value_handle;
    hci_con_handle_t connection_handle;
} client_connection_t;
static client_connection_t client_connections[MAX_NR_CLIENT_CONNECTIONS];

// Active client connection slot index. Currently only a single concurrent BLE client
// is supported (hardcoded to index 0).
static int notification_connection_idx;
// Round-robin slot cursor used only when falling back to single-slot notifications
// for clients with a 23-byte ATT MTU.
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

static void uni_att_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);
static int uni_att_write_callback(hci_con_handle_t con_handle,
                                  uint16_t att_handle,
                                  uint16_t transaction_mode,
                                  uint16_t offset,
                                  uint8_t* buffer,
                                  uint16_t buffer_size);
static uint16_t uni_att_read_callback(hci_con_handle_t conn_handle,
                                      uint16_t att_handle,
                                      uint16_t offset,
                                      uint8_t* buffer,
                                      uint16_t buffer_size);
static client_connection_t* connection_for_conn_handle(hci_con_handle_t conn_handle);
static void populate_compact_device(int idx, const uni_hid_device_t* d);
static bool next_notify_device(void);
static void notify_client(void);
static void maybe_notify_client(void);

// Synchronizes `compact_devices[idx]` with the live state of HID device slot `d`.
//
// Always zeroes the wire struct first and preserves `idx` so unoccupied or
// disconnected slots serialize cleanly with an all-zero MAC address and `state = 0`.
// Why check `!d->conn.connected`: during `uni_hid_device_disconnect(d)`,
// `uni_bt_conn_disconnect(&d->conn)` sets `d->conn.connected = false` (without clearing
// `btaddr` or `vendor_id`) and immediately triggers `uni_bt_service_on_device_disconnected(d)`
// BEFORE `uni_hid_device_delete(d)` zeroes `*d`. Guarding on `d->conn.connected`
// guarantees that disconnected slots are cleared immediately before notifying the client.
static void populate_compact_device(int idx, const uni_hid_device_t* d) {
    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
        return;

    memset(&compact_devices[idx], 0, sizeof(compact_devices[idx]));
    compact_devices[idx].idx = (uint8_t)idx;

    if (!d || !d->conn.connected)
        return;

    memcpy(compact_devices[idx].addr, d->conn.btaddr, sizeof(compact_devices[idx].addr));
    compact_devices[idx].vendor_id = d->vendor_id;
    compact_devices[idx].product_id = d->product_id;
    compact_devices[idx].state = (uint8_t)d->conn.state;
    compact_devices[idx].incoming = (uint8_t)d->conn.incoming;
    compact_devices[idx].controller_type = (uint16_t)d->controller_type;
    compact_devices[idx].controller_subtype = (uint8_t)d->controller_subtype;
}

static bool is_notify_client_valid(void) {
    return ((client_connections[notification_connection_idx].connection_handle != HCI_CON_HANDLE_INVALID) &&
            (client_connections[notification_connection_idx].notification_enabled));
}

// Advances the round-robin slot cursor for 23-byte MTU single-slot notifications.
// Returns `true` when all `CONFIG_BLUEPAD32_MAX_DEVICES` slots have been sent.
static bool next_notify_device(void) {
    notification_device_idx++;
    if (notification_device_idx == CONFIG_BLUEPAD32_MAX_DEVICES) {
        notification_device_idx = 0;
        return true;
    }
    return false;
}

// Sends a GATT notification for characteristic AC05 (`compact_devices`) to the
// connected BLE client.
//
// Why compare `(size_t)mtu >= sizeof(...) + 3` instead of `mtu - 3 >= sizeof(...)`:
// `att_server_get_mtu()` returns `0` (`uint16_t`) if the HCI connection handle is
// no longer valid. Subtracting `3` from `0` would either wrap around in unsigned
// arithmetic or promote to signed `-3` and implicitly convert to `SIZE_MAX - 2`
// when compared against `sizeof(...)`. Adding the 3-byte ATT notification header
// overhead on the right-hand side is underflow-safe and `-Wsign-compare`-clean.
static void notify_client(void) {
    if (!is_notify_client_valid())
        return;

    client_connection_t* ctx = &client_connections[notification_connection_idx];
    uint16_t mtu = att_server_get_mtu(ctx->connection_handle);

    // If the negotiated ATT MTU can carry the entire compact_devices table (64 bytes + 3-byte ATT header),
    // send all slots in a single notification packet.
    if ((size_t)mtu >= sizeof(compact_devices) + 3) {
        logd("Notifying client idx = %d, all devices (%zu bytes, mtu = %u)\n", notification_connection_idx,
             sizeof(compact_devices), mtu);
        uint8_t status = att_server_notify(ctx->connection_handle, ctx->value_handle, (const uint8_t*)compact_devices,
                                           sizeof(compact_devices));
        if (status != ERROR_CODE_SUCCESS) {
            loge("BLE Service: Failed to notify client, error: %#x\n", status);
        }
        notification_device_idx = 0;
        return;
    }

    // Fallback for clients with default 23-byte ATT MTU: send one 16-byte slot per notification.
    if ((size_t)mtu >= sizeof(compact_devices[0]) + 3) {
        logd("Notifying client idx = %d, device idx = %d (mtu = %u)\n", notification_connection_idx,
             notification_device_idx, mtu);
        uint8_t status =
            att_server_notify(ctx->connection_handle, ctx->value_handle,
                              (const uint8_t*)&compact_devices[notification_device_idx], sizeof(compact_devices[0]));
        if (status != ERROR_CODE_SUCCESS) {
            loge("BLE Service: Failed to notify client, error: %#x\n", status);
        }

        bool finish_round = next_notify_device();
        if (!finish_round)
            att_server_request_can_send_now_event(ctx->connection_handle);
    }
}

// Requests a BTstack `ATT_EVENT_CAN_SEND_NOW` callback if any connected client
// has subscribed to AC05 notifications.
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

static int uni_att_write_callback(hci_con_handle_t con_handle,
                                  uint16_t att_handle,
                                  uint16_t transaction_mode,
                                  uint16_t offset,
                                  uint8_t* buffer,
                                  uint16_t buffer_size) {
    ARG_UNUSED(transaction_mode);

    logd("uni_att_write_callback: con handle=%#x, att_handle=%#x, offset=%d\n", con_handle, att_handle, offset);
    //    printf_hexdump(buffer, buffer_size);

    client_connection_t* ctx;

    switch (att_handle) {
        case ATT_CHARACTERISTIC_4627C4A4_AC03_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC03: Whether to enable BLE connections (1-byte boolean).
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool enabled = buffer[0];
            uni_bt_le_set_enabled(enabled);
            return ATT_ERROR_SUCCESS;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC04_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC04: Start or stop scanning for new controller connections (1-byte boolean).
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool enabled = buffer[0];
            if (enabled)
                uni_bt_start_scanning_and_autoconnect_unsafe();
            else
                uni_bt_stop_scanning_unsafe();
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC05_46B9_B688_AFC5C1BF7F63_01_CLIENT_CONFIGURATION_HANDLE: {
            // AC05 CCCD: Enable or disable notifications for connected controllers.
            if (buffer_size < 2)
                return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
            if (offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            ctx = connection_for_conn_handle(con_handle);
            if (!ctx)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            ctx->notification_enabled =
                little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
            ctx->value_handle = ATT_CHARACTERISTIC_4627C4A4_AC05_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE;
            notification_device_idx = 0;
            if (ctx->notification_enabled)
                att_server_request_can_send_now_event(ctx->connection_handle);

            logi("BLE Service: Notification enabled = %d for handle %#x\n", ctx->notification_enabled,
                 ctx->connection_handle);
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC06_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC06: Controller mappings preset (0 = Xbox, 1 = Nintendo Switch, 2 = Custom).
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            uint8_t type = buffer[0];
            if (type >= UNI_GAMEPAD_MAPPINGS_TYPE_COUNT)
                return ATT_ERROR_VALUE_NOT_ALLOWED;
            uni_gamepad_set_mappings_type(type);
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC07_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC07: Whether to enforce the Bluetooth MAC allowlist in connections.
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool enabled = buffer[0];
            uni_bt_allowlist_set_enabled(enabled);
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC08_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC08: Atomically replace the Bluetooth MAC allowlist.
            // Accepts `K * 6` bytes (`0 <= K <= CONFIG_BLUEPAD32_MAX_ALLOWLIST`).
            // Why skip `00:00:00:00:00:00`: mobile BLE stacks (Android BluetoothGatt / iOS CoreBluetooth)
            // may reject 0-byte GATT writes, so clients clearing the last allowlist entry write a single
            // 6-byte all-zero sentinel (`00:00:00:00:00:00`), which clears the allowlist cleanly here.
            if (offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            if ((buffer_size % sizeof(bd_addr_t)) != 0 ||
                buffer_size > CONFIG_BLUEPAD32_MAX_ALLOWLIST * sizeof(bd_addr_t))
                return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;

            const bd_addr_t zero_addr = {0, 0, 0, 0, 0, 0};
            uni_bt_allowlist_remove_all();

            size_t count = buffer_size / sizeof(bd_addr_t);
            for (size_t i = 0; i < count; i++) {
                bd_addr_t addr;
                bd_addr_copy(addr, &buffer[i * sizeof(bd_addr_t)]);
                if (bd_addr_cmp(addr, zero_addr) != 0) {
                    uni_bt_allowlist_add_addr(addr);
                }
            }
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC09_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC09: Whether to enable virtual child devices (e.g., DualSense touchpad mouse).
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool enabled = buffer[0];
            uni_virtual_device_set_enabled(enabled);
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC0A_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC0A: Disconnect and delete the controller at slot `idx`.
            // Both `uni_hid_device_disconnect(d)` and `uni_hid_device_delete(d)` must be called
            // (matching `CMD_DISCONNECT_DEVICE` in `uni_bt.c`) so the slot in `g_devices[]` is
            // freed for future controller connections instead of leaking.
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            uint8_t idx = buffer[0];
            if (idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
                return ATT_ERROR_VALUE_NOT_ALLOWED;
            uni_hid_device_t* d = uni_hid_device_get_instance_for_idx(idx);
            if (!d || !d->conn.connected)
                return ATT_ERROR_VALUE_NOT_ALLOWED;
            uni_hid_device_disconnect(d);
            uni_hid_device_delete(d);
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC0B_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC0B: Delete stored Bluetooth bond keys.
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool delete_keys = buffer[0];
            if (!delete_keys)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            uni_bt_del_keys_unsafe();
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC0C_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC0C: Reset / reboot the Bluepad32 microcontroller immediately.
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

static uint16_t uni_att_read_callback(hci_con_handle_t conn_handle,
                                      uint16_t att_handle,
                                      uint16_t offset,
                                      uint8_t* buffer,
                                      uint16_t buffer_size) {
    ARG_UNUSED(conn_handle);

    switch (att_handle) {
        case ATT_CHARACTERISTIC_4627C4A4_AC01_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // AC01: Firmware version string (UTF-8, without NUL terminator).
            return att_read_callback_handle_blob((const uint8_t*)uni_version, (uint16_t)strlen(uni_version), offset,
                                                 buffer, buffer_size);
        case ATT_CHARACTERISTIC_4627C4A4_AC02_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC02: Max supported concurrent connections.
            const uint8_t max = CONFIG_BLUEPAD32_MAX_DEVICES;
            return att_read_callback_handle_blob(&max, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC03_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC03: Whether BLE controller connections are enabled.
            const uint8_t enabled = uni_bt_le_is_enabled();
            return att_read_callback_handle_blob(&enabled, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC04_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC04: Whether controller scanning/inquiry is active.
            const uint8_t scanning = uni_bt_is_scanning();
            return att_read_callback_handle_blob(&scanning, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC05_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // AC05: All controller slots (4 * 16 = 64 bytes).
            return att_read_callback_handle_blob((const void*)compact_devices, (uint16_t)sizeof(compact_devices),
                                                 offset, buffer, buffer_size);
        case ATT_CHARACTERISTIC_4627C4A4_AC06_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC06: Active controller button mappings preset (0 = Xbox, 1 = Switch, 2 = Custom).
            const uint8_t mappings_type = uni_gamepad_get_mappings_type();
            return att_read_callback_handle_blob(&mappings_type, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC07_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC07: Whether Bluetooth MAC allowlist enforcement is enabled.
            const uint8_t allowlist_enabled = uni_bt_allowlist_is_enabled();
            return att_read_callback_handle_blob(&allowlist_enabled, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC08_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC08: Compacted list of non-zero MAC addresses currently in the allowlist.
            const bd_addr_t* addresses = NULL;
            int total = 0;
            bd_addr_t compacted[CONFIG_BLUEPAD32_MAX_ALLOWLIST];
            int count = 0;
            const bd_addr_t zero_addr = {0, 0, 0, 0, 0, 0};

            uni_bt_allowlist_get_all(&addresses, &total);
            for (int i = 0; i < total && count < CONFIG_BLUEPAD32_MAX_ALLOWLIST; i++) {
                if (bd_addr_cmp(addresses[i], zero_addr) != 0) {
                    bd_addr_copy(compacted[count++], addresses[i]);
                }
            }
            return att_read_callback_handle_blob(
                (const uint8_t*)compacted, (uint16_t)(sizeof(bd_addr_t) * (size_t)count), offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC09_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC09: Whether virtual child devices are enabled.
            const uint8_t virtual_enabled = uni_virtual_device_is_enabled();
            return att_read_callback_handle_blob(&virtual_enabled, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC0A_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // AC0A: Write-only characteristic (Disconnect a device).
            loge("BLE Service: 4627C4A4_AC0A_46B9_B688_AFC5C1BF7F63 does not support read\n");
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC0B_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // AC0B: Write-only characteristic (Delete stored Bluetooth bond keys).
            loge("BLE Service: 4627C4A4_AC0B_46B9_B688_AFC5C1BF7F63 does not support read\n");
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC0C_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // AC0C: Write-only characteristic (Reset device).
            loge("BLE Service: 4627C4A4_AC0C_46B9_B688_AFC5C1BF7F63 does not support read\n");
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

static void uni_att_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    client_connection_t* ctx;
    int mtu;

    if (packet_type != HCI_EVENT_PACKET)
        return;

    switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CONNECTED:
            // Claim a free client slot for the newly connected central.
            ctx = connection_for_conn_handle(HCI_CON_HANDLE_INVALID);
            if (!ctx)
                break;
            ctx->connection_handle = att_event_connected_get_handle(packet);
            mtu = att_server_get_mtu(ctx->connection_handle);
            logi("BLE Service: New client connected handle = %#x, mtu = %d\n", ctx->connection_handle, mtu);
            break;
        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
            mtu = att_event_mtu_exchange_complete_get_MTU(packet);
            ctx = connection_for_conn_handle(att_event_mtu_exchange_complete_get_handle(packet));
            if (!ctx)
                break;
            logi("BLE Service: MTU exchange complete for handle = %#x, mtu = %d\n", ctx->connection_handle, mtu);
            break;
        case ATT_EVENT_CAN_SEND_NOW:
            notify_client();
            break;
        case ATT_EVENT_DISCONNECTED:
            ctx = connection_for_conn_handle(att_event_disconnected_get_handle(packet));
            if (!ctx)
                break;
            logi("BLE Service: client disconnected, handle = %#x\n", ctx->connection_handle);
            memset(ctx, 0, sizeof(*ctx));
            ctx->connection_handle = HCI_CON_HANDLE_INVALID;
            notification_device_idx = 0;
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // Handled via ATT_EVENT_DISCONNECTED.
            break;
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    // Deprecated. Replaced by ATT_EVENT_CONNECTED.
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
 * Configures the ATT Server with the pre-compiled ATT Database generated from the .gatt file,
 * hydrates all `compact_devices[]` slots from `g_devices[]`, and enables BLE advertisements.
 */
void uni_bt_service_init(void) {
    logi("Starting Bluepad32 BLE service UUID: 4627C4A4-AC00-46B9-B688-AFC5C1BF7F63\n");

    // Setup ATT server.
    att_server_init(profile_data, uni_att_read_callback, uni_att_write_callback);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr = {0};

    memset(&client_connections, 0, sizeof(client_connections));
    for (int i = 0; i < MAX_NR_CLIENT_CONNECTIONS; i++)
        client_connections[i].connection_handle = HCI_CON_HANDLE_INVALID;
    notification_device_idx = 0;

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++)
        populate_compact_device(i, uni_hid_device_get_instance_for_idx(i));

    // register for ATT events
    att_server_register_packet_handler(uni_att_packet_handler);

    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*)adv_data);
    gap_advertisements_enable(true);
}

bool uni_bt_service_is_enabled(void) {
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

// Invoked from the BTstack task after a controller transitions to `UNI_BT_CONN_STATE_DEVICE_READY`
// and all SDP/GATT metadata (`vendor_id`, `product_id`, `controller_type`, `controller_subtype`)
// has been resolved.
void uni_bt_service_on_device_ready(const uni_hid_device_t* d) {
    // Must be called from BTstack task
    if (!d)
        return;
    if (!service_enabled)
        return;

    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0)
        return;

    populate_compact_device(idx, d);

    maybe_notify_client();
}

// Invoked from the BTstack task when a controller establishes its initial Bluetooth connection.
void uni_bt_service_on_device_connected(const uni_hid_device_t* d) {
    // Must be called from BTstack task
    if (!d)
        return;
    if (!service_enabled)
        return;

    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0)
        return;

    populate_compact_device(idx, d);

    maybe_notify_client();
}

// Invoked from the BTstack task when a controller disconnects. Because `d->conn.connected`
// is already `false`, `populate_compact_device(idx, d)` zeroes the slot while preserving `idx`.
void uni_bt_service_on_device_disconnected(const uni_hid_device_t* d) {
    // Must be called from BTstack task
    if (!d)
        return;
    if (!service_enabled)
        return;

    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0)
        return;

    populate_compact_device(idx, d);

    maybe_notify_client();
}
