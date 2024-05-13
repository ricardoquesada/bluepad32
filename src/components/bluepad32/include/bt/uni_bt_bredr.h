// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_BREDR_H
#define UNI_BT_BREDR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

#include <btstack.h>
#include <btstack_config.h>

#include "bt/uni_bt_conn.h"
#include "uni_hid_device.h"

void uni_bt_bredr_scan_start(void);
void uni_bt_bredr_scan_stop(void);

// Called from uni_hid_device_disconnect()
void uni_bt_bredr_disconnect(uni_hid_device_t* d);

void uni_bt_bredr_list_bonded_keys(void);
void uni_bt_bredr_delete_bonded_keys(void);
void uni_bt_bredr_setup(void);

void uni_bt_bredr_set_enabled(bool enabled);
bool uni_bt_bredr_is_enabled(void);

void uni_bt_bredr_l2cap_create_control_connection(uni_hid_device_t* d);
void uni_bt_bredr_process_fsm(uni_hid_device_t* d);

void uni_bt_bredr_on_l2cap_incoming_connection(uint16_t channel, const uint8_t* packet, uint16_t size);
void uni_bt_bredr_on_l2cap_channel_opened(uint16_t channel, const uint8_t* packet, uint16_t size);
void uni_bt_bredr_on_l2cap_channel_closed(uint16_t channel, const uint8_t* packet, uint16_t size);
void uni_bt_bredr_on_l2cap_data_packet(uint16_t channel, const uint8_t* packet, uint16_t size);
void uni_bt_bredr_on_gap_inquiry_result(uint16_t channel, const uint8_t* packet, uint16_t size);
void uni_bt_bredr_on_hci_connection_request(uint16_t channel, const uint8_t* packet, uint16_t size);
void uni_bt_bredr_on_hci_connection_complete(uint16_t channel, const uint8_t* packet, uint16_t size);
void uni_bt_bredr_on_hci_disconnection_complete(uint16_t channel, const uint8_t* packet, uint16_t size);
void uni_bt_bredr_on_hci_pin_code_request(uint16_t channel, const uint8_t* packet, uint16_t size);
void uni_bt_bredr_on_hci_remote_name_request_complete(uint16_t channel, const uint8_t* packet, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_BREDR_H