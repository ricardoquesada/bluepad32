// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_LE_H
#define UNI_BT_LE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

#include <btstack.h>
#include <btstack_config.h>

#include "bt/uni_bt_conn.h"
#include "uni_hid_device.h"

void uni_bt_le_on_hci_event_le_meta(const uint8_t* packet, uint16_t size);
void uni_bt_le_on_hci_event_encryption_change(const uint8_t* packet, uint16_t size);
void uni_bt_le_on_gap_event_advertising_report(const uint8_t* packet, uint16_t size);
void uni_bt_le_on_hci_disconnection_complete(uint16_t channel, const uint8_t* packet, uint16_t size);

void uni_bt_le_scan_start(void);
void uni_bt_le_scan_stop(void);

// Called from uni_hid_device_disconnect()
void uni_bt_le_disconnect(uni_hid_device_t* d);

void uni_bt_le_list_bonded_keys(void);
void uni_bt_le_delete_bonded_keys(void);
void uni_bt_le_setup(void);

void uni_bt_le_set_enabled(bool enabled);
bool uni_bt_le_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_LE_H