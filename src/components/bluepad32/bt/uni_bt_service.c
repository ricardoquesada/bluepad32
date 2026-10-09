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
// 4. Dual-PDU Service Identity (`ADV_IND` + `SCAN_RSP`, `0x2A00`, `AC0D`):
//    Because Legacy BLE advertising PDUs are capped at 31 bytes and the Flags (3B)
//    plus 128-bit Bluepad32 Service UUID (18B) consume 21 bytes in `ADV_IND`,
//    `update_adv_and_scan_rsp()` places up to 8 bytes of the service name in
//    `ADV_IND` (`0x09` Complete Local Name if `<= 8` bytes, or `0x08` Shortened
//    Local Name if `> 8` bytes) and places the full `0x09` Complete Local Name
//    (up to 29 UTF-8 bytes) in `SCAN_RSP` via `gap_scan_response_set_data()`.
//    Both `GAP_DEVICE_NAME` (`0x2A00`, handle `0x0003`) and `AC0D` (handle `0x0022`)
//    are `DYNAMIC` so GATT reads and writes reflect runtime renames immediately.
// 5. Per-Connection Password Authentication Gate (`AC0E`, handle `0x0024`):
//    Public discovery characteristics (`0x2A00`, `AC01`, `AC0D` read, `AC0E` read/write)
//    are always accessible. When a non-empty password (`1..31` bytes) is configured,
//    protected reads (`AC02`–`AC09`) return `ATT_READ_ERROR_CODE_OFFSET | ATT_ERROR_INSUFFICIENT_AUTHENTICATION`
//    (`0xfe05`), and protected writes (`AC03`–`AC0D` and `AC05` CCCD `0x0012`) return
//    `ATT_ERROR_INSUFFICIENT_AUTHENTICATION` (`0x05`) until the client writes the
//    matching password to `AC0E` on that connection handle.

#include "bt/uni_bt_service.h"

#include <string.h>

#include <btstack.h>

#include "bt/uni_bt.h"
#include "bt/uni_bt_allowlist.h"
#include "bt/uni_bt_le.h"
#include "bt/uni_bt_service.gatt.h"
#include "bt/uni_bt_setup.h"
#include "controller/uni_gamepad.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_log.h"
#include "uni_property.h"
#include "uni_system.h"
#include "uni_version.h"
#include "uni_virtual_device.h"

// General Discoverable = 0x02
// BR/EDR Not supported = 0x04
#define APP_AD_FLAGS 0x06

// Max number of clients that can connect to the service at the same time.
#define MAX_NR_CLIENT_CONNECTIONS 1

// Maximum UTF-8 bytes of the service name that fit alongside Flags (3B) + 128-bit UUID (18B)
// + AD header (2B) inside the 31-byte Legacy Primary Advertising PDU (ADV_IND).
#define ADV_PRIMARY_NAME_MAX_LEN 8

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

// Per-client GATT connection state, notification subscription, and session authentication metadata.
typedef struct {
    bool notification_enabled;
    bool authenticated;
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
static bool config_loaded;
static bool service_enabled = true;
static bool service_initialized;

static char service_name[UNI_BT_SERVICE_NAME_MAX_LEN + 1];
static char service_password[UNI_BT_SERVICE_PASSWORD_MAX_LEN + 1];

// Static buffers passed to BTstack's gap_advertisements_set_data() and gap_scan_response_set_data(),
// which store raw pointers without copying.
static uint8_t adv_data[31];
static uint8_t adv_data_len;
static uint8_t scan_rsp_data[31];
static uint8_t scan_rsp_data_len;

// 128-bit Bluepad32 Service UUID (4627C4A4-AC00-46B9-B688-AFC5C1BF7F63) in little-endian wire order.
static const uint8_t k_service_uuid128_le[16] = {
    0x63, 0x7F, 0xBF, 0xC1, 0xC5, 0xAF, 0x88, 0xB6, 0xB9, 0x46, 0x00, 0xAC, 0xA4, 0xC4, 0x27, 0x46,
};

static void ensure_config_loaded(void);
static void update_adv_and_scan_rsp(void);
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
static client_connection_t* connection_or_alloc_for_conn_handle(hci_con_handle_t conn_handle);
static bool is_conn_authenticated(hci_con_handle_t conn_handle);
static bool constant_time_password_matches(const uint8_t* candidate, uint16_t candidate_len, const char* expected);
static void populate_compact_device(int idx, const uni_hid_device_t* d);
static bool next_notify_device(void);
static void notify_client(void);
static void maybe_notify_client(void);

// Guards GAP advertising calls in fuzzing/unit-test builds (`FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION`)
// where setters may be invoked before `hci_init()` allocates `hci_stack`.
static bool is_hci_stack_ready(void) {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    return hci_get_stack() != NULL;
#else
    return true;
#endif
}

// Rebuilds both the 31-byte Primary Advertising PDU (`adv_data`) and the 31-byte
// Scan Response PDU (`scan_rsp_data`), and registers them with GAP when the
// BLE service is initialized.
static void update_adv_and_scan_rsp(void) {
    size_t full_name_len = strlen(service_name);
    if (full_name_len > UNI_BT_SERVICE_NAME_MAX_LEN) {
        full_name_len = UNI_BT_SERVICE_NAME_MAX_LEN;
    }
    size_t primary_name_len = (full_name_len <= ADV_PRIMARY_NAME_MAX_LEN) ? full_name_len : ADV_PRIMARY_NAME_MAX_LEN;

    // 1. Primary Advertising PDU (ADV_IND, <= 31 bytes):
    //    [0..2]   Flags (3 bytes)
    //    [3..20]  Complete List of 128-bit Service Class UUIDs (18 bytes)
    //    [21..]   Complete Local Name (0x09) if full_name_len <= 8, else Shortened Local Name (0x08) (2 +
    //    primary_name_len bytes)
    uint8_t pos = 0;
    memset(adv_data, 0, sizeof(adv_data));
    adv_data[pos++] = 2;
    adv_data[pos++] = BLUETOOTH_DATA_TYPE_FLAGS;
    adv_data[pos++] = APP_AD_FLAGS;

    adv_data[pos++] = 17;
    adv_data[pos++] = BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS;
    memcpy(&adv_data[pos], k_service_uuid128_le, sizeof(k_service_uuid128_le));
    pos += (uint8_t)sizeof(k_service_uuid128_le);

    adv_data[pos++] = (uint8_t)(1 + primary_name_len);
    adv_data[pos++] = (full_name_len <= ADV_PRIMARY_NAME_MAX_LEN) ? BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME
                                                                  : BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME;
    if (primary_name_len > 0) {
        memcpy(&adv_data[pos], service_name, primary_name_len);
        pos += (uint8_t)primary_name_len;
    }
    adv_data_len = pos;

    // 2. Scan Response PDU (SCAN_RSP, <= 31 bytes):
    //    Carries the full Complete Local Name (0x09) up to 29 UTF-8 bytes.
    memset(scan_rsp_data, 0, sizeof(scan_rsp_data));
    scan_rsp_data[0] = (uint8_t)(1 + full_name_len);
    scan_rsp_data[1] = BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME;
    if (full_name_len > 0) {
        memcpy(&scan_rsp_data[2], service_name, full_name_len);
    }
    scan_rsp_data_len = (uint8_t)(2 + full_name_len);

    if (service_initialized && is_hci_stack_ready()) {
        gap_advertisements_set_data(adv_data_len, adv_data);
        gap_scan_response_set_data(scan_rsp_data_len, scan_rsp_data);
    }
}

// Lazily loads BLE service configuration from `uni_property` on first access.
//
// Why copy `UNI_PROPERTY_IDX_BLE_SERVICE_NAME` into `service_name` BEFORE reading
// `UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD`:
// Both `uni_property_btstack_tlv.c` and `uni_property_esp32.c` return string property
// values in a single shared static buffer (`str_ret[128]`). Copying the name immediately
// prevents the subsequent password lookup from overwriting the name buffer.
static void ensure_config_loaded(void) {
    if (config_loaded)
        return;
    config_loaded = true;

    for (int i = 0; i < MAX_NR_CLIENT_CONNECTIONS; i++) {
        client_connections[i].connection_handle = HCI_CON_HANDLE_INVALID;
    }

    uni_property_value_t en_val = uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED);
    service_enabled = (en_val.u8 != 0);

    uni_property_value_t name_val = uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_NAME);
    const char* src_name = (name_val.str && name_val.str[0] != '\0') ? name_val.str : CONFIG_BLUEPAD32_BLE_SERVICE_NAME;
    if (!src_name || src_name[0] == '\0') {
        src_name = "Bluepad32";
    }
    memset(service_name, 0, sizeof(service_name));
    strncpy(service_name, src_name, UNI_BT_SERVICE_NAME_MAX_LEN);
    service_name[UNI_BT_SERVICE_NAME_MAX_LEN] = '\0';

    uni_property_value_t pass_val = uni_property_get(UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD);
    const char* src_pass = pass_val.str ? pass_val.str : CONFIG_BLUEPAD32_BLE_SERVICE_PASSWORD;
    if (!src_pass) {
        src_pass = "";
    }
    memset(service_password, 0, sizeof(service_password));
    strncpy(service_password, src_pass, UNI_BT_SERVICE_PASSWORD_MAX_LEN);
    service_password[UNI_BT_SERVICE_PASSWORD_MAX_LEN] = '\0';

    update_adv_and_scan_rsp();
}

// Synchronizes `compact_devices[idx]` with the live state of HID device slot `d`.
//
// Always zeroes the wire struct first and preserves `idx` so unoccupied or
// disconnected slots (when `d == NULL`) serialize cleanly with an all-zero MAC
// address and `state = 0`.
static void populate_compact_device(int idx, const uni_hid_device_t* d) {
    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
        return;

    memset(&compact_devices[idx], 0, sizeof(compact_devices[idx]));
    compact_devices[idx].idx = (uint8_t)idx;

    if (!d)
        return;

    memcpy(compact_devices[idx].addr, d->conn.btaddr, sizeof(compact_devices[idx].addr));
    compact_devices[idx].vendor_id = d->vendor_id;
    compact_devices[idx].product_id = d->product_id;
    compact_devices[idx].state = (uint8_t)d->conn.state;
    compact_devices[idx].incoming = (uint8_t)d->conn.incoming;
    compact_devices[idx].controller_type = (uint16_t)d->controller_type;
    compact_devices[idx].controller_subtype = (uint8_t)d->controller_subtype;
}

// Returns `true` only if a BLE client is connected, has subscribed to `AC05` notifications,
// and has satisfied the `AC0E` password authentication gate.
static bool is_notify_client_valid(void) {
    return ((client_connections[notification_connection_idx].connection_handle != HCI_CON_HANDLE_INVALID) &&
            (client_connections[notification_connection_idx].notification_enabled) &&
            is_conn_authenticated(client_connections[notification_connection_idx].connection_handle));
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
// has subscribed to AC05 notifications and is authenticated.
static void maybe_notify_client(void) {
    client_connection_t* ctx = NULL;

    for (int i = 0; i < MAX_NR_CLIENT_CONNECTIONS; i++) {
        if (client_connections[i].connection_handle != HCI_CON_HANDLE_INVALID &&
            client_connections[i].notification_enabled &&
            is_conn_authenticated(client_connections[i].connection_handle)) {
            ctx = &client_connections[i];
            break;
        }
    }
    if (ctx)
        att_server_request_can_send_now_event(ctx->connection_handle);
}

// Compares a candidate password payload (`candidate[0..candidate_len-1]`) against the
// NUL-terminated `expected` password without early-return branching on byte mismatches,
// preventing byte-by-byte timing side-channel attacks over ATT writes to `AC0E`.
static bool constant_time_password_matches(const uint8_t* candidate, uint16_t candidate_len, const char* expected) {
    size_t expected_len = strlen(expected);
    uint8_t diff = (candidate_len == expected_len) ? 0 : 1;
    for (uint16_t i = 0; i < candidate_len; i++) {
        uint8_t exp_byte = (i < expected_len) ? (uint8_t)expected[i] : 0;
        diff |= (uint8_t)(candidate[i] ^ exp_byte);
    }
    return diff == 0;
}

// Returns `true` if the BLE service is open (`service_password == ""`) or if the
// client connection identified by `conn_handle` has unlocked its session via `AC0E`.
static bool is_conn_authenticated(hci_con_handle_t conn_handle) {
    if (!uni_bt_service_is_password_required())
        return true;
    client_connection_t* ctx = connection_for_conn_handle(conn_handle);
    return ctx != NULL && ctx->authenticated;
}

static int uni_att_write_callback(hci_con_handle_t con_handle,
                                  uint16_t att_handle,
                                  uint16_t transaction_mode,
                                  uint16_t offset,
                                  uint8_t* buffer,
                                  uint16_t buffer_size) {
    ARG_UNUSED(transaction_mode);
    ensure_config_loaded();

    logd("uni_att_write_callback: con handle=%#x, att_handle=%#x, offset=%d\n", con_handle, att_handle, offset);
    //    printf_hexdump(buffer, buffer_size);

    client_connection_t* ctx;

    // Handle AC0E (Password Authentication Gate) first so unauthenticated clients can unlock the session.
    if (att_handle == ATT_CHARACTERISTIC_4627C4A4_AC0E_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE) {
        if (offset != 0)
            return ATT_ERROR_REQUEST_NOT_SUPPORTED;
        if (buffer_size == 0 || buffer_size > UNI_BT_SERVICE_PASSWORD_MAX_LEN)
            return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;

        ctx = connection_or_alloc_for_conn_handle(con_handle);
        if (!ctx)
            return ATT_ERROR_REQUEST_NOT_SUPPORTED;

        if (!uni_bt_service_is_password_required()) {
            ctx->authenticated = true;
            return ATT_ERROR_SUCCESS;
        }

        if (!constant_time_password_matches(buffer, buffer_size, service_password)) {
            ctx->authenticated = false;
            logi("BLE Service: Authentication failed for handle %#x\n", con_handle);
            return ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
        }

        ctx->authenticated = true;
        logi("BLE Service: Client authenticated for handle %#x\n", con_handle);
        return ATT_ERROR_SUCCESS;
    }

    switch (att_handle) {
        case ATT_CHARACTERISTIC_4627C4A4_AC03_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC03: Whether to enable BLE connections (1-byte boolean).
            if (!is_conn_authenticated(con_handle))
                return ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool enabled = buffer[0];
            uni_bt_le_set_enabled(enabled);
            return ATT_ERROR_SUCCESS;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC04_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC04: Start or stop scanning for new controller connections (1-byte boolean).
            if (!is_conn_authenticated(con_handle))
                return ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
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
            if (!is_conn_authenticated(con_handle))
                return ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
            if (buffer_size < 2)
                return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
            if (offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            ctx = connection_or_alloc_for_conn_handle(con_handle);
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
            if (!is_conn_authenticated(con_handle))
                return ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
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
            if (!is_conn_authenticated(con_handle))
                return ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
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
            if (!is_conn_authenticated(con_handle))
                return ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
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
            if (!is_conn_authenticated(con_handle))
                return ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
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
            if (!is_conn_authenticated(con_handle))
                return ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            uint8_t idx = buffer[0];
            if (idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            uni_hid_device_t* d = uni_hid_device_get_instance_for_idx(idx);
            if (!d || !d->conn.connected)
                return ATT_ERROR_VALUE_NOT_ALLOWED;
            uni_hid_device_disconnect(d);
            uni_hid_device_delete(d);
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC0B_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC0B: Delete stored Bluetooth bond keys.
            if (!is_conn_authenticated(con_handle))
                return ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
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
            if (!is_conn_authenticated(con_handle))
                return ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
            if (buffer_size != 1 || offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            bool reset = buffer[0];
            if (!reset)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            uni_system_reboot();
            break;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC0D_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC0D: Set BLE service advertised device name (UTF-8, 1..29 bytes).
            if (!is_conn_authenticated(con_handle))
                return ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
            if (offset != 0)
                return ATT_ERROR_REQUEST_NOT_SUPPORTED;
            if (buffer_size == 0 || buffer_size > UNI_BT_SERVICE_NAME_MAX_LEN)
                return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
            char new_name[UNI_BT_SERVICE_NAME_MAX_LEN + 1];
            memcpy(new_name, buffer, buffer_size);
            new_name[buffer_size] = '\0';
            if (new_name[0] == '\0')
                return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
            uni_bt_service_set_name(new_name);
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
    ensure_config_loaded();

    switch (att_handle) {
        case ATT_CHARACTERISTIC_GAP_DEVICE_NAME_01_VALUE_HANDLE:
        case ATT_CHARACTERISTIC_4627C4A4_AC0D_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // GAP Device Name (0x2A00) and AC0D (Service Name): publicly readable without authentication.
            return att_read_callback_handle_blob((const uint8_t*)service_name, (uint16_t)strlen(service_name), offset,
                                                 buffer, buffer_size);
        case ATT_CHARACTERISTIC_4627C4A4_AC01_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // AC01: Firmware version string (UTF-8, without NUL terminator), publicly readable.
            return att_read_callback_handle_blob((const uint8_t*)uni_version, (uint16_t)strlen(uni_version), offset,
                                                 buffer, buffer_size);
        case ATT_CHARACTERISTIC_4627C4A4_AC0E_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC0E: Session authentication status (0 = open, 1 = password required, 2 = authenticated).
            uint8_t state = UNI_BT_SERVICE_AUTH_STATE_OPEN;
            if (uni_bt_service_is_password_required()) {
                state = is_conn_authenticated(conn_handle) ? UNI_BT_SERVICE_AUTH_STATE_AUTHENTICATED
                                                           : UNI_BT_SERVICE_AUTH_STATE_REQUIRED;
            }
            return att_read_callback_handle_blob(&state, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC02_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC02: Max supported concurrent connections.
            if (!is_conn_authenticated(conn_handle))
                return (uint16_t)(ATT_READ_ERROR_CODE_OFFSET | ATT_ERROR_INSUFFICIENT_AUTHENTICATION);
            const uint8_t max = CONFIG_BLUEPAD32_MAX_DEVICES;
            return att_read_callback_handle_blob(&max, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC03_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC03: Whether BLE controller connections are enabled.
            if (!is_conn_authenticated(conn_handle))
                return (uint16_t)(ATT_READ_ERROR_CODE_OFFSET | ATT_ERROR_INSUFFICIENT_AUTHENTICATION);
            const uint8_t enabled = uni_bt_le_is_enabled();
            return att_read_callback_handle_blob(&enabled, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC04_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC04: Whether controller scanning/inquiry is active.
            if (!is_conn_authenticated(conn_handle))
                return (uint16_t)(ATT_READ_ERROR_CODE_OFFSET | ATT_ERROR_INSUFFICIENT_AUTHENTICATION);
            const uint8_t scanning = uni_bt_is_scanning();
            return att_read_callback_handle_blob(&scanning, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC05_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // AC05: All controller slots (4 * 16 = 64 bytes).
            if (!is_conn_authenticated(conn_handle))
                return (uint16_t)(ATT_READ_ERROR_CODE_OFFSET | ATT_ERROR_INSUFFICIENT_AUTHENTICATION);
            return att_read_callback_handle_blob((const void*)compact_devices, (uint16_t)sizeof(compact_devices),
                                                 offset, buffer, buffer_size);
        case ATT_CHARACTERISTIC_4627C4A4_AC06_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC06: Active controller button mappings preset (0 = Xbox, 1 = Switch, 2 = Custom).
            if (!is_conn_authenticated(conn_handle))
                return (uint16_t)(ATT_READ_ERROR_CODE_OFFSET | ATT_ERROR_INSUFFICIENT_AUTHENTICATION);
            const uint8_t mappings_type = uni_gamepad_get_mappings_type();
            return att_read_callback_handle_blob(&mappings_type, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC07_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC07: Whether Bluetooth MAC allowlist enforcement is enabled.
            if (!is_conn_authenticated(conn_handle))
                return (uint16_t)(ATT_READ_ERROR_CODE_OFFSET | ATT_ERROR_INSUFFICIENT_AUTHENTICATION);
            const uint8_t allowlist_enabled = uni_bt_allowlist_is_enabled();
            return att_read_callback_handle_blob(&allowlist_enabled, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC08_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // AC08: Compacted list of non-zero MAC addresses currently in the allowlist.
            if (!is_conn_authenticated(conn_handle))
                return (uint16_t)(ATT_READ_ERROR_CODE_OFFSET | ATT_ERROR_INSUFFICIENT_AUTHENTICATION);
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
            if (!is_conn_authenticated(conn_handle))
                return (uint16_t)(ATT_READ_ERROR_CODE_OFFSET | ATT_ERROR_INSUFFICIENT_AUTHENTICATION);
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

// Looks up an existing `client_connection_t` slot matching `conn_handle`.
static client_connection_t* connection_for_conn_handle(hci_con_handle_t conn_handle) {
    for (int i = 0; i < MAX_NR_CLIENT_CONNECTIONS; i++) {
        if (client_connections[i].connection_handle == conn_handle)
            return &client_connections[i];
    }
    return NULL;
}

// Returns the existing `client_connection_t` slot for `conn_handle`, or claims a free
// (`HCI_CON_HANDLE_INVALID`) slot if not yet registered.
//
// Why lazy allocation is needed in addition to `ATT_EVENT_CONNECTED`:
// Unit tests that exercise ATT PDUs directly via `att_handle_request()` without first
// injecting an `ATT_EVENT_CONNECTED` HCI event still need a per-connection slot to record
// `AC05` CCCD subscriptions (`0x0012`) or `AC0E` password authentication state (`0x0024`).
static client_connection_t* connection_or_alloc_for_conn_handle(hci_con_handle_t conn_handle) {
    if (conn_handle == HCI_CON_HANDLE_INVALID)
        return NULL;
    client_connection_t* ctx = connection_for_conn_handle(conn_handle);
    if (ctx)
        return ctx;
    ctx = connection_for_conn_handle(HCI_CON_HANDLE_INVALID);
    if (!ctx)
        return NULL;
    memset(ctx, 0, sizeof(*ctx));
    ctx->connection_handle = conn_handle;
    ctx->authenticated = !uni_bt_service_is_password_required();
    return ctx;
}

static void uni_att_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    client_connection_t* ctx;
    int mtu;

    if (packet_type != HCI_EVENT_PACKET)
        return;

    switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CONNECTED: {
            // Claim or reset the client slot for the newly connected central.
            hci_con_handle_t handle = att_event_connected_get_handle(packet);
            ctx = connection_or_alloc_for_conn_handle(handle);
            if (!ctx)
                break;
            ctx->notification_enabled = false;
            ctx->authenticated = !uni_bt_service_is_password_required();
            mtu = att_server_get_mtu(ctx->connection_handle);
            logi("BLE Service: New client connected handle = %#x, mtu = %d\n", ctx->connection_handle, mtu);
            break;
        }
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
    service_initialized = false;
    memset(&client_connections, 0, sizeof(client_connections));
    for (int i = 0; i < MAX_NR_CLIENT_CONNECTIONS; i++) {
        client_connections[i].connection_handle = HCI_CON_HANDLE_INVALID;
    }
    notification_device_idx = 0;
    att_server_deinit();
    if (is_hci_stack_ready()) {
        gap_advertisements_enable(false);
    }
}

/*
 * Configures the ATT Server with the pre-compiled ATT Database generated from the .gatt file,
 * hydrates all `compact_devices[]` slots from `g_devices[]`, and enables BLE advertisements.
 */
void uni_bt_service_init(void) {
    ensure_config_loaded();
    service_enabled = true;
    service_initialized = true;

    logi("Starting Bluepad32 BLE service UUID: 4627C4A4-AC00-46B9-B688-AFC5C1BF7F63 (name='%s', auth=%s)\n",
         service_name, uni_bt_service_is_password_required() ? "password" : "open");

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

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        uni_hid_device_t* d = uni_hid_device_get_instance_for_idx(i);
        populate_compact_device(i, (d && d->conn.connected) ? d : NULL);
    }

    // register for ATT events
    att_server_register_packet_handler(uni_att_packet_handler);

    if (is_hci_stack_ready()) {
        gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
        update_adv_and_scan_rsp();
        gap_advertisements_enable(true);
    }
}

bool uni_bt_service_is_enabled(void) {
    ensure_config_loaded();
    return service_enabled;
}

void uni_bt_service_set_enabled(bool enabled) {
    ensure_config_loaded();
    service_enabled = enabled;
    uni_property_set(UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED, (uni_property_value_t){.u8 = enabled ? 1 : 0});

    // Only start/stop the ATT server if `uni_bt_setup` has already reached `SETUP_STATE_READY`.
    // When called during early `platform->init()` (before `uni_bt_le_setup()` / `sm_init()`),
    // `uni_bt_setup` will invoke `uni_bt_service_init()` at the proper point in the boot sequence.
    if (enabled) {
        if (!service_initialized && uni_bt_setup_is_ready()) {
            uni_bt_service_init();
        }
    } else {
        if (service_initialized && uni_bt_setup_is_ready()) {
            uni_bt_service_deinit();
        }
    }
}

const char* uni_bt_service_get_name(void) {
    ensure_config_loaded();
    return service_name;
}

void uni_bt_service_set_name(const char* name) {
    ensure_config_loaded();

    const char* effective_name = (name && name[0] != '\0') ? name : CONFIG_BLUEPAD32_BLE_SERVICE_NAME;
    if (!effective_name || effective_name[0] == '\0') {
        effective_name = "Bluepad32";
    }

    memset(service_name, 0, sizeof(service_name));
    strncpy(service_name, effective_name, UNI_BT_SERVICE_NAME_MAX_LEN);
    service_name[UNI_BT_SERVICE_NAME_MAX_LEN] = '\0';

    uni_property_set(UNI_PROPERTY_IDX_BLE_SERVICE_NAME, (uni_property_value_t){.str = service_name});
    update_adv_and_scan_rsp();
}

const char* uni_bt_service_get_password(void) {
    ensure_config_loaded();
    return service_password;
}

void uni_bt_service_set_password(const char* password) {
    ensure_config_loaded();

    const char* effective_pass = password ? password : "";
    memset(service_password, 0, sizeof(service_password));
    strncpy(service_password, effective_pass, UNI_BT_SERVICE_PASSWORD_MAX_LEN);
    service_password[UNI_BT_SERVICE_PASSWORD_MAX_LEN] = '\0';

    uni_property_set(UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD, (uni_property_value_t){.str = service_password});

    // Setting or rotating a non-empty password immediately re-locks any active client
    // sessions and revokes `AC05` notification subscriptions until re-authenticated.
    bool pass_required = (service_password[0] != '\0');
    for (int i = 0; i < MAX_NR_CLIENT_CONNECTIONS; i++) {
        if (pass_required) {
            client_connections[i].authenticated = false;
            client_connections[i].notification_enabled = false;
        } else if (client_connections[i].connection_handle != HCI_CON_HANDLE_INVALID) {
            client_connections[i].authenticated = true;
        }
    }
}

bool uni_bt_service_is_password_required(void) {
    ensure_config_loaded();
    return service_password[0] != '\0';
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

// Invoked from the BTstack task when a controller disconnects. Passing `NULL` to
// `populate_compact_device(idx, NULL)` zeroes the slot while preserving `idx`.
void uni_bt_service_on_device_disconnected(const uni_hid_device_t* d) {
    // Must be called from BTstack task
    if (!d)
        return;
    if (!service_enabled)
        return;

    int idx = uni_hid_device_get_idx_for_instance(d);
    if (idx < 0)
        return;

    populate_compact_device(idx, NULL);

    maybe_notify_client();
}
