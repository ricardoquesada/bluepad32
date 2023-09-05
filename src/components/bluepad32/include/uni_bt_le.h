/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2023 Ricardo Quesada

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
****************************************************************************/

#ifndef UNI_BT_LE_H
#define UNI_BT_LE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

#include <btstack.h>
#include <btstack_config.h>

#include "uni_bt_conn.h"
#include "uni_hid_device.h"

void uni_bt_le_on_hci_event_le_meta(const uint8_t* packet, uint16_t size);
void uni_bt_le_on_hci_event_encryption_change(const uint8_t* packet, uint16_t size);
void uni_bt_le_on_gap_event_advertising_report(const uint8_t* packet, uint16_t size);
void uni_bt_le_on_hci_diconnection_complete(uint16_t channel, const uint8_t* packet, uint16_t size);

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