// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_H
#define UNI_BT_H

#include <stdbool.h>

#include <btstack.h>

#include "uni_hid_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file uni_bt.h
 * @brief Thread-safe Bluetooth stack management and lifecycle control.
 */

// Private, don't use
extern bd_addr_t uni_local_bd_addr;

/**
 * @brief Initialize the Bluetooth stack.
 * @return 0 on success, or an error code.
 */
int uni_bt_init(void);

// Public functions
// Safe to call these functions from another task and/or CPU

/** @brief List stored Bluetooth keys, created when a device gets paired (Thread-safe) */
void uni_bt_list_keys_safe(void);
/** @brief List stored Bluetooth keys (Not thread-safe) */
void uni_bt_list_keys_unsafe(void);
/** @brief Delete stored Bluetooth keys (Thread-safe) */
void uni_bt_del_keys_safe(void);
/** @brief Delete stored Bluetooth keys (Not thread-safe) */
void uni_bt_del_keys_unsafe(void);
/** @brief Dump all connected devices to the console (Thread-safe). */
void uni_bt_dump_devices_safe(void);

/**
 * @brief Whether to enable new Bluetooth connections.
 * When enabled, the device scans for new connections, and it will try to auto-connect to supported devices.
 * When disabled, only devices that have paired before can connect.
 * @deprecated Use `uni_bt_start_scanning_and_autoconnect_safe` or `uni_bt_stop_scanning_safe`.
 */
void uni_bt_enable_new_connections_safe(bool enabled) __attribute__((deprecated));

/** @brief Starts scanning for new connections and auto-connects to supported devices (Thread-safe). */
void uni_bt_start_scanning_and_autoconnect_safe(void);

/** @brief Stops scanning for new connections (Thread-safe). */
void uni_bt_stop_scanning_safe(void);

/**
 * @brief Must be called from BTthread
 * @deprecated Use `uni_bt_start_scanning_and_autoconnect_unsafe` or `uni_bt_stop_scanning_unsafe`.
 */
void uni_bt_enable_new_connections_unsafe(bool enabled) __attribute__((deprecated));

/** @brief Starts scanning for new connections and auto-connects to supported devices (Not thread-safe). */
void uni_bt_start_scanning_and_autoconnect_unsafe(void);

/** @brief Stops scanning for new connections (Not thread-safe). */
void uni_bt_stop_scanning_unsafe(void);

/**
 * @brief Returns whether new connections are accepted.
 * @deprecated Use `uni_bt_is_scanning`.
 */
bool uni_bt_enable_new_connections_is_enabled(void) __attribute__((deprecated()));

/** @brief Returns whether scanning is currently active. */
bool uni_bt_is_scanning(void);

/**
 * @brief Allow or disallow incoming connections.
 * Bonded devices are able to connect even when scanning is disabled.
 * This function is used to prevent any kind of incoming connections.
 * If you want to disable any kind of BT connection make sure to:
 * - stop scanning
 * - disallow incoming connections
 * @param allow True to allow, false to disallow.
 */
void uni_bt_allow_incoming_connections(bool allow);

/** @brief Returns whether incoming connections are allowed. */
bool uni_bt_incoming_connections_is_allowed(void);

/**
 * @brief Enables the BLE service (Thread-safe).
 * @param enabled True to enable, false to disable.
 */
void uni_bt_enable_service_safe(bool enabled);

/**
 * @brief Disconnects a device by its index (Thread-safe).
 * @param device_idx The index of the device to disconnect.
 */
void uni_bt_disconnect_device_safe(int device_idx);

/**
 * @brief Gets the local BD address (Thread-safe).
 * @param addr The address to populate.
 */
void uni_bt_get_local_bd_addr_safe(bd_addr_t addr);

// Properties
/** @brief Sets the GAP security level. */
void uni_bt_set_gap_security_level(int gap);
/** @brief Gets the GAP security level. */
int uni_bt_get_gap_security_level(void);
/** @brief Sets the GAP inquiry length. */
void uni_bt_set_gap_inquiry_length(int len);
/** @brief Gets the GAP inquiry length. */
int uni_bt_get_gap_inquiry_length(void);
/** @brief Sets the GAP maximum periodic length. */
void uni_bt_set_gap_max_periodic_length(int len);
/** @brief Gets the GAP maximum periodic length. */
int uni_bt_get_gap_max_periodic_length(void);
/** @brief Sets the GAP minimum periodic length. */
void uni_bt_set_gap_min_periodic_length(int len);
/** @brief Gets the GAP minimum periodic length. */
int uni_bt_get_gap_min_periodic_length(void);

//
//  Private functions. Don't call them
//
/** @brief Private packet handler. Do not call. */
void uni_bt_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_H