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
 * @brief Bluetooth LE Service interface for Bluepad32.
 */

/** @brief Initializes the Bluepad32 BLE service. */
void uni_bt_service_init(void);

/** @brief Deinitializes the Bluepad32 BLE service. */
void uni_bt_service_deinit(void);

/**
 * @brief Checks whether the Bluepad32 BLE service is enabled.
 * @return true if enabled, false otherwise.
 */
bool uni_bt_service_is_enabled(void);

/**
 * @brief Sets whether the Bluepad32 BLE service is enabled.
 * @param enabled true to enable, false to disable.
 */
void uni_bt_service_set_enabled(bool enabled);

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