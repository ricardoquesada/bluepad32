// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_H
#define UNI_BT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include <btstack.h>

#include "uni_hid_device.h"

// Private, don't use
extern bd_addr_t uni_local_bd_addr;

int uni_bt_init(void);

// Public functions
// Safe to call these functions from another task and/or CPU

// List stored Bluetooth keys, created when a device gets paired
void uni_bt_list_keys_safe(void);
void uni_bt_list_keys_unsafe(void);
// Delete stored Bluetooth keys
void uni_bt_del_keys_safe(void);
void uni_bt_del_keys_unsafe(void);
// Dump all connected devices.
void uni_bt_dump_devices_safe(void);
// Whether to enable new Bluetooth connections.
// When enabled, the device scans for new connections, and it will try to auto-connect to supported devices.
// When disabled, only devices that have paired before can connect.
void uni_bt_enable_new_connections_safe(bool enabled);
// Must be called from BTthread
void uni_bt_enable_new_connections_unsafe(bool enabled);
// Returns whether new connections are accepted.
bool uni_bt_enable_new_connections_is_enabled(void);
// Enables the BLE service
void uni_bt_enable_service_safe(bool enabled);

// Disconnects a device
void uni_bt_disconnect_device_safe(int device_idx);

// Get local BD address
void uni_bt_get_local_bd_addr_safe(bd_addr_t addr);

// Properties
void uni_bt_set_gap_security_level(int gap);
int uni_bt_get_gap_security_level(void);
void uni_bt_set_gap_inquiry_length(int len);
int uni_bt_get_gap_inquiry_length(void);
void uni_bt_set_gap_max_peridic_length(int len);
int uni_bt_get_gap_max_periodic_length(void);
void uni_bt_set_gap_min_peridic_length(int len);
int uni_bt_get_gap_min_periodic_length(void);

//
//  Private functions. Don't call them
//
void uni_bt_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_H