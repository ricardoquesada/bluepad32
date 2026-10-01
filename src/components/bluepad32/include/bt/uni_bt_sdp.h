// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_SDP_H
#define UNI_BT_SDP_H

#include <stdint.h>

#include "uni_hid_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file uni_bt_sdp.h
 * @brief Bluetooth Classic (BR/EDR) Service Discovery Protocol (SDP) client and server interface.
 *
 * Manages sequential SDP queries for Device ID (Vendor ID / Product ID) and HID Report
 * Descriptors during BR/EDR gamepad connection setup, initializes the local Device ID
 * SDP service record required by DualShock/DualSense controllers, and exposes packet-level
 * result handlers for in-process protocol verification.
 */

/**
 * @brief Start the SDP query sequence (VID/PID followed by HID descriptor) for a connecting device.
 *
 * Only one SDP query may be active at a time. If another SDP query is already in progress,
 * the new device `d` is disconnected and deleted.
 *
 * @param d Pointer to the connecting HID device instance.
 */
void uni_bt_sdp_query_start(uni_hid_device_t* d);

/**
 * @brief Complete the active SDP query sequence and advance the BR/EDR connection state machine.
 *
 * Cancels the SDP query timeout timer, clears the active SDP device pointer, transitions
 * the connection state to `UNI_BT_CONN_STATE_SDP_HID_DESCRIPTOR_FETCHED`, and invokes
 * `uni_bt_bredr_process_fsm(d)`.
 *
 * @param d Pointer to the HID device whose SDP query completed.
 */
void uni_bt_sdp_query_end(uni_hid_device_t* d);

/**
 * @brief Abort an in-progress SDP query for the specified device, if active.
 *
 * If `d` is currently the active `sdp_device`, removes `sdp_query_timer` from
 * the BTstack run loop and clears `sdp_device = NULL`. Safe to call for any device.
 *
 * @param d Pointer to the HID device being disconnected, deleted, or aborted.
 */
void uni_bt_sdp_query_abort(uni_hid_device_t* d);

/**
 * @brief Initiate an SDP PnP Information (`0x1200`) query to fetch Vendor ID and Product ID.
 *
 * @param d Pointer to the target HID device instance.
 */
void uni_bt_sdp_query_start_vid_pid(uni_hid_device_t* d);

/**
 * @brief Initiate an SDP Human Interface Device (`0x1124`) query to fetch the HID Report Descriptor.
 *
 * If the device controller type is already known not to require a HID descriptor, skips the
 * query and immediately invokes `uni_bt_sdp_query_end(d)`.
 *
 * @param d Pointer to the target HID device instance.
 */
void uni_bt_sdp_query_start_hid_descriptor(uni_hid_device_t* d);

/**
 * @brief Initialize the local SDP server and register the Bluepad32 Device ID service record.
 *
 * DualShock and DualSense controllers query the host's Device ID SDP record upon reconnection;
 * registering this record ensures reliable controller reconnections.
 */
void uni_bt_sdp_server_init(void);

/**
 * @brief Bind the active module-scoped `sdp_device` pointer for in-process unit testing.
 *
 * Allows unit tests to exercise `uni_handle_sdp_pid_query_result()` and
 * `uni_handle_sdp_hid_query_result()` with synthetic `SDP_EVENT_QUERY_ATTRIBUTE_VALUE` and
 * `SDP_EVENT_QUERY_COMPLETE` packets without initiating a live BTstack L2CAP SDP client
 * connection or arming run-loop timers.
 *
 * @param d Pointer to the synthetic `uni_hid_device_t` under test, or `NULL` to reset.
 */
void uni_bt_sdp_set_device_for_test(uni_hid_device_t* d);

/**
 * @brief BTstack SDP client callback for Device ID (PnP Information) query results.
 *
 * Reassembles streamed `SDP_EVENT_QUERY_ATTRIBUTE_VALUE` bytes into an internal attribute
 * buffer (bounded by `MAX_ATTRIBUTE_VALUE_SIZE`, 512 bytes), extracts `BLUETOOTH_ATTRIBUTE_VENDOR_ID`
 * (`0x0201`) and `BLUETOOTH_ATTRIBUTE_PRODUCT_ID` (`0x0202`) Data Elements, and on
 * `SDP_EVENT_QUERY_COMPLETE` resolves the controller type and advances the BR/EDR connection FSM.
 *
 * @param packet_type HCI packet type (expected `HCI_EVENT_PACKET`).
 * @param channel     BTstack channel identifier (unused).
 * @param packet      Pointer to the raw SDP event packet buffer.
 * @param size        Length of `packet` in bytes.
 */
void uni_handle_sdp_pid_query_result(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);

/**
 * @brief BTstack SDP client callback for HID Service (`0x1124`) descriptor query results.
 *
 * Reassembles streamed `SDP_EVENT_QUERY_ATTRIBUTE_VALUE` bytes into an internal attribute
 * buffer (bounded by `MAX_ATTRIBUTE_VALUE_SIZE`, 512 bytes), walks the nested Data Element
 * Sequence (DES) for `BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST` (`0x0206`) to extract the
 * `DE_STRING` HID report descriptor, and on `SDP_EVENT_QUERY_COMPLETE` finalizes the SDP
 * query via `uni_bt_sdp_query_end()`.
 *
 * @param packet_type HCI packet type (expected `HCI_EVENT_PACKET`).
 * @param channel     BTstack channel identifier (unused).
 * @param packet      Pointer to the raw SDP event packet buffer.
 * @param size        Length of `packet` in bytes.
 */
void uni_handle_sdp_hid_query_result(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_SDP_H