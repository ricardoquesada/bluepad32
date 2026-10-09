// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_SERVICE_H
#define UNI_BT_SERVICE_H

#include <stdbool.h>

#include "uni_hid_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file uni_bt_service.h
 * @brief Bluetooth Low Energy (BLE) Configuration & Telemetry GATT Service for Bluepad32.
 *
 * Exposes the Bluepad32 primary GATT service (`4627C4A4-AC00-46B9-B688-AFC5C1BF7F63`)
 * with characteristics `AC01` through `AC0E`:
 * - **Dual-PDU BLE Service Identity (`0x2A00` & `AC0D`):**
 *   Fits the 16-byte 128-bit service UUID and a custom UTF-8 device name (`1..29` bytes)
 *   within Legacy BLE's 31-byte packet limit by placing Flags (3B) + 128-bit UUID (18B) +
 *   up to 8 bytes of Shortened/Complete Local Name (`0x08`/`0x09`) in the Primary Advertising
 *   PDU (`ADV_IND`), and the full Complete Local Name (`0x09`, up to 29 bytes) in the Scan
 *   Response PDU (`SCAN_RSP`). The same dynamic name is served via GAP Device Name (`0x2A00`)
 *   and characteristic `AC0D`.
 * - **Per-Connection Password Authentication Gate (`AC0E`):**
 *   When a non-empty password (`1..31` UTF-8 bytes) is configured, unauthenticated clients
 *   can only read public identity characteristics (`0x2A00`, `AC01`, `AC0D`, `AC0E`) and
 *   write the password to `AC0E`. All protected reads (`AC02`–`AC09`), writes (`AC03`–`AC0D`),
 *   and `AC05` CCCD subscriptions/notifications are rejected with
 *   `ATT_ERROR_INSUFFICIENT_AUTHENTICATION` (`0x05`) until `AC0E` is unlocked for that connection.
 */

/**
 * @brief Maximum UTF-8 byte length for the BLE service advertised device name (`AC0D`).
 *
 * A 31-byte Legacy BLE Scan Response PDU (`SCAN_RSP`) requires 2 header bytes
 * (`length` + AD Type `0x09` Complete Local Name), leaving exactly 29 bytes for the name.
 */
#define UNI_BT_SERVICE_NAME_MAX_LEN 29

/**
 * @brief Maximum UTF-8 byte length for the BLE service session password (`AC0E`).
 */
#define UNI_BT_SERVICE_PASSWORD_MAX_LEN 31

/**
 * @brief Per-connection authentication state reported when reading characteristic `AC0E`.
 */
typedef enum {
    /** `0`: No password is configured (`""`); all GATT characteristics are openly accessible. */
    UNI_BT_SERVICE_AUTH_STATE_OPEN = 0,
    /** `1`: A password is configured and the current BLE connection has not yet authenticated. */
    UNI_BT_SERVICE_AUTH_STATE_REQUIRED = 1,
    /** `2`: A password is configured and the current BLE connection has authenticated via `AC0E`. */
    UNI_BT_SERVICE_AUTH_STATE_AUTHENTICATED = 2,

    // Compatibility aliases
    UNI_BT_SERVICE_AUTH_OPEN = UNI_BT_SERVICE_AUTH_STATE_OPEN,
    UNI_BT_SERVICE_AUTH_REQUIRED = UNI_BT_SERVICE_AUTH_STATE_REQUIRED,
    UNI_BT_SERVICE_AUTH_AUTHENTICATED = UNI_BT_SERVICE_AUTH_STATE_AUTHENTICATED,
} uni_bt_service_auth_state_t;

/**
 * @brief Initializes the Bluepad32 BLE GATT server and starts `ADV_IND` + `SCAN_RSP` advertising.
 */
void uni_bt_service_init(void);

/**
 * @brief Deinitializes the Bluepad32 BLE GATT server, clears active client sessions, and stops advertising.
 */
void uni_bt_service_deinit(void);

/**
 * @brief Checks whether the Bluepad32 BLE configuration service is enabled.
 * @return `true` if enabled, `false` otherwise.
 */
bool uni_bt_service_is_enabled(void);

/**
 * @brief Enables or disables the Bluepad32 BLE configuration service and persists the setting in NVS/TLV.
 *
 * If the Bluetooth stack has already reached `SETUP_STATE_READY`, enabling or disabling the service
 * immediately starts or stops the ATT server and BLE advertisements at runtime.
 *
 * @param enabled `true` to enable and advertise the BLE service, `false` to disable it.
 */
void uni_bt_service_set_enabled(bool enabled);

/**
 * @brief Returns the configured BLE service advertised device name (`AC0D` / `0x2A00`).
 * @return Non-NULL NUL-terminated UTF-8 string (`1..29` bytes, default `"Bluepad32"`).
 */
const char* uni_bt_service_get_name(void);

/**
 * @brief Sets and persists the BLE service advertised device name (`AC0D` / `0x2A00`).
 *
 * Immediately rebuilds both the 31-byte Primary Advertising PDU (`ADV_IND`) and the 31-byte
 * Scan Response PDU (`SCAN_RSP`). Names longer than `UNI_BT_SERVICE_NAME_MAX_LEN` (29 bytes)
 * are truncated. Passing `NULL` or `""` resets the name to `CONFIG_BLUEPAD32_BLE_SERVICE_NAME`
 * (or `"Bluepad32"` if empty).
 *
 * @param name UTF-8 device name string, or `NULL`/empty to reset to the default name.
 */
void uni_bt_service_set_name(const char* name);

/**
 * @brief Returns the configured BLE service session password.
 * @return Non-NULL NUL-terminated UTF-8 string (`""` when open / no password required).
 */
const char* uni_bt_service_get_password(void);

/**
 * @brief Sets and persists the BLE service session password (`AC0E`).
 *
 * Passwords longer than `UNI_BT_SERVICE_PASSWORD_MAX_LEN` (31 bytes) are truncated.
 * Passing `NULL` or `""` clears the password and restores open access (`UNI_BT_SERVICE_AUTH_STATE_OPEN`)
 * for all connected clients. Setting or changing a non-empty password immediately revokes
 * authentication (`authenticated = false`) and disables `AC05` notifications on any active connections.
 *
 * @param password UTF-8 password string, or `NULL`/`""` to disable password protection.
 */
void uni_bt_service_set_password(const char* password);

/**
 * @brief Checks whether a non-empty password is required to access protected BLE service characteristics.
 * @return `true` if password protection is active (`AC0E` requires authentication), `false` if open.
 */
bool uni_bt_service_is_password_required(void);

// Callbacks from uni_hid_device that will be notified to the BLE client.

/**
 * @brief Callback invoked when a HID device becomes ready.
 * @param d Pointer to the HID device.
 */
void uni_bt_service_on_device_ready(const uni_hid_device_t* d);

/**
 * @brief Callback invoked when a HID device connects.
 * @param d Pointer to the HID device.
 */
void uni_bt_service_on_device_connected(const uni_hid_device_t* d);

/**
 * @brief Callback invoked when a HID device disconnects.
 * @param d Pointer to the HID device.
 */
void uni_bt_service_on_device_disconnected(const uni_hid_device_t* d);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_SERVICE_H