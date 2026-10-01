// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_LE_H
#define UNI_BT_LE_H

#include <inttypes.h>
#include <stdbool.h>

#include <btstack.h>
#include <btstack_config.h>

#include "bt/uni_bt_conn.h"
#include "uni_hid_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle `HCI_EVENT_LE_META` subevents from the BLE controller.
 * @param packet Pointer to the raw packet.
 * @param size Size of the packet.
 */
void uni_bt_le_on_hci_event_le_meta(const uint8_t* packet, uint16_t size);

/**
 * @brief Handle encryption change events.
 * @param packet Pointer to the raw packet.
 * @param size Size of the packet.
 */
void uni_bt_le_on_hci_event_encryption_change(const uint8_t* packet, uint16_t size);

/**
 * @brief Handle `HCI_EVENT_GATTSERVICE_META` subevents from the BLE Device Information Service (DIS) client.
 *
 * Processes `GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_DONE` to transition to HID over GATT (HIDS)
 * discovery, and validates `ATT_ERROR_SUCCESS` on `GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_PNP_ID`
 * before updating the device's Vendor ID and Product ID.
 *
 * @param packet Pointer to the raw BTstack `HCI_EVENT_GATTSERVICE_META` packet.
 * @param size   Size of `packet` in bytes.
 */
void uni_bt_le_on_hci_event_gattservice_meta(const uint8_t* packet, uint16_t size);

/**
 * @brief Handle GAP advertising reports.
 * @param packet Pointer to the raw packet.
 * @param size Size of the packet.
 */
void uni_bt_le_on_gap_event_advertising_report(const uint8_t* packet, uint16_t size);

/**
 * @brief Handle HCI disconnection complete events.
 * @param channel The channel.
 * @param packet Pointer to the raw packet.
 * @param size Size of the packet.
 */
void uni_bt_le_on_hci_disconnection_complete(uint16_t channel, const uint8_t* packet, uint16_t size);

/** @brief Start LE scanning. */
void uni_bt_le_scan_start(void);

/** @brief Stop LE scanning. */
void uni_bt_le_scan_stop(void);

/**
 * @brief Disconnect LE device.
 * Called from uni_hid_device_disconnect().
 * @param d Pointer to the HID device.
 */
void uni_bt_le_disconnect(uni_hid_device_t* d);

/** @brief List bonded LE keys. */
void uni_bt_le_list_bonded_keys(void);

/** @brief Delete bonded LE keys. */
void uni_bt_le_delete_bonded_keys(void);

/** @brief Setup LE subsystem. */
void uni_bt_le_setup(void);

/**
 * @brief Set LE enabled or disabled.
 * @param enabled True to enable, false to disable.
 */
void uni_bt_le_set_enabled(bool enabled);

/**
 * @brief Check if LE is enabled.
 * @return True if enabled, false otherwise.
 */
bool uni_bt_le_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_LE_H