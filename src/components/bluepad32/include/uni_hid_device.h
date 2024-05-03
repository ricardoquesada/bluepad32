// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_DEVICE_H
#define UNI_HID_DEVICE_H

#include <btstack.h>
#include <stdbool.h>
#include <stdint.h>

#include "bt/uni_bt_conn.h"
#include "controller/uni_controller.h"
#include "controller/uni_controller_type.h"
#include "parser/uni_hid_parser.h"
#include "uni_circular_buffer.h"
#include "uni_error.h"

#define HID_MAX_NAME_LEN 240
#define HID_MAX_DESCRIPTOR_LEN 512
#define HID_DEVICE_MAX_PARSER_DATA 256
#define HID_DEVICE_MAX_PLATFORM_DATA 256
// HID_DEVICE_CONNECTION_TIMEOUT_MS includes the time from when the device is created until it is ready.
#define HID_DEVICE_CONNECTION_TIMEOUT_MS 20000

typedef enum {
    SDP_QUERY_AFTER_CONNECT,   // If not set, this is the default one.
    SDP_QUERY_BEFORE_CONNECT,  // Special case for DualShock4 1st generation.
    SDP_QUERY_NOT_NEEDED,      // Because the Controller type was inferred by other means.
} uni_sdp_query_type_t;

struct uni_hid_device_s {
    uint32_t cod;  // Class of Device.
    uint16_t vendor_id;
    uint16_t product_id;
    char name[HID_MAX_NAME_LEN];

    // hid, cod, etc...
    uint32_t flags;

    // Will abort connection if the connection was not established after timeout.
    btstack_timer_source_t connection_timer;
    // Max amount of time to wait to get the device name.
    btstack_timer_source_t inquiry_remote_name_timer;

    // SDP
    uint8_t hid_descriptor[HID_MAX_DESCRIPTOR_LEN];
    uint16_t hid_descriptor_len;
    // DualShock4 1st gen requires to do the SDP query before l2cap connect,
    // otherwise it won't work.
    // And Nintendo Switch Pro gamepad requires to do the SDP query after l2cap
    // connect, so we use this variable to determine when to do the SDP query.
    // TODO: Actually this is not entirely true since it works Ok when using
    // Unijoysticle + BTstack + libusb in Linux. The correct thing to do is to
    // debug the Linux connection and see what packets are sent before the
    // connection.
    uni_sdp_query_type_t sdp_query_type;

    // Channels
    uint16_t hids_cid;  // BLE only

    // TODO: Create a union of gamepad/mouse/keyboard structs
    // At the moment "mouse" reuses gamepad struct, but it is a hack.
    // Gamepad
    uni_controller_type_t controller_type;        // type of controller. E.g: DualShock4, Switch, etc.
    uni_controller_subtype_t controller_subtype;  // sub-type of controller attached, used for Wii mostly
    uni_controller_t controller;                  // Data

    // Functions used to parse the usage page/usage.
    uni_report_parser_t report_parser;

    // Buttons that need to be released before triggering the action again.
    uint32_t misc_button_wait_release;
    // Buttons that need to wait for a delay before triggering the action again.
    uint32_t misc_button_wait_delay;
    // Needed for Nintendo Switch family of controllers.
    btstack_timer_source_t misc_button_delay_timer;

    // Circular buffer that contains the outgoing packets that couldn't be sent
    // immediately.
    uni_circular_buffer_t outgoing_buffer;

    // Bytes reserved to controller's parser instances.
    // E.g.: The Wii driver uses it for the state machine.
    uint8_t parser_data[HID_DEVICE_MAX_PARSER_DATA];

    // Bytes reserved to different platforms.
    // E.g.: C64 or Airlift might use it to store different values.
    uint8_t platform_data[HID_DEVICE_MAX_PLATFORM_DATA];

    // Bluetooth connection info.
    uni_bt_conn_t conn;

    // Link to parent device. Used only when the device is a "virtual child".
    // Safe to assume that when parent != NULL, then it is a "virtual" device.
    // For example, the mouse implemented by DualShock4 has the "gamepad" as parent.
    struct uni_hid_device_s* parent;
    // When a physical controller has a child, like a "virtual device"
    // For example, DualShock4 has the "mouse" as a child.
    struct uni_hid_device_s* child;
};
typedef struct uni_hid_device_s uni_hid_device_t;

// Callback function used as in get_instance_with_predicate
typedef uint8_t (*uni_hid_device_predicate_t)(uni_hid_device_t* d, void* data);

void uni_hid_device_setup(void);

uni_hid_device_t* uni_hid_device_create(bd_addr_t address);

// Used for controllers that implement two input devices like DualShock4, which is a gamepad and a mouse
// at the same time. The mouse will be the "virtual" device in this case.
uni_hid_device_t* uni_hid_device_create_virtual(uni_hid_device_t* parent);

// Don't add any other get_instance_for_XXX function.
// Instead use: get_instance_with_predicate()
uni_hid_device_t* uni_hid_device_get_instance_for_address(bd_addr_t addr);
uni_hid_device_t* uni_hid_device_get_instance_for_cid(uint16_t cid);
// BLE only
uni_hid_device_t* uni_hid_device_get_instance_for_hids_cid(uint16_t cid);
uni_hid_device_t* uni_hid_device_get_instance_for_connection_handle(hci_con_handle_t handle);
uni_hid_device_t* uni_hid_device_get_first_device_with_state(uni_bt_conn_state_t state);
uni_hid_device_t* uni_hid_device_get_instance_with_predicate(uni_hid_device_predicate_t predicate, void* data);
uni_hid_device_t* uni_hid_device_get_instance_for_idx(int idx);
int uni_hid_device_get_idx_for_instance(const uni_hid_device_t* d);

void uni_hid_device_init(uni_hid_device_t* d);

void uni_hid_device_set_ready(uni_hid_device_t* d);
// Returns whether the platform accepted the connection
bool uni_hid_device_set_ready_complete(uni_hid_device_t* d);

void uni_hid_device_request_inquire(void);

void uni_hid_device_on_connected(uni_hid_device_t* d, bool connected);
void uni_hid_device_connect(uni_hid_device_t* d);
void uni_hid_device_disconnect(uni_hid_device_t* d);
void uni_hid_device_delete(uni_hid_device_t* d);

void uni_hid_device_set_cod(uni_hid_device_t* d, uint32_t cod);
bool uni_hid_device_is_cod_supported(uint32_t cod);

// A new device has been discovered while scanning.
// @param addr: the Bluetooth address
// @param name: could be NULL, could be zero-length, or might contain the name.
// @param cod: Class of Device. See "uni_bt_defines.h" for possible values.
// @param rssi: Received Signal Strength Indicator (RSSI) measured in dBms. The higher (255) the better.
// @returns UNI_ERROR_SUCCESS if a connection to the device should be established.
uni_error_t uni_hid_device_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi);

void uni_hid_device_set_hid_descriptor(uni_hid_device_t* d, const uint8_t* descriptor, int len);
bool uni_hid_device_has_hid_descriptor(uni_hid_device_t* d);

void uni_hid_device_set_incoming(uni_hid_device_t* d, bool incoming);
bool uni_hid_device_is_incoming(uni_hid_device_t* d);

void uni_hid_device_set_name(uni_hid_device_t* d, const char* name);
bool uni_hid_device_has_name(uni_hid_device_t* d);

void uni_hid_device_set_product_id(uni_hid_device_t* d, uint16_t product_id);
uint16_t uni_hid_device_get_product_id(uni_hid_device_t* d);

void uni_hid_device_set_vendor_id(uni_hid_device_t* d, uint16_t vendor_id);
uint16_t uni_hid_device_get_vendor_id(uni_hid_device_t* d);

void uni_hid_device_dump_device(uni_hid_device_t* d);
void uni_hid_device_dump_all(void);

bool uni_hid_device_guess_controller_type_from_name(uni_hid_device_t* d, const char* name);
void uni_hid_device_guess_controller_type_from_pid_vid(uni_hid_device_t* d);
bool uni_hid_device_has_controller_type(uni_hid_device_t* d);

void uni_hid_device_process_controller(uni_hid_device_t* d);

void uni_hid_device_set_connection_handle(uni_hid_device_t* d, hci_con_handle_t handle);

void uni_hid_device_send_report(uni_hid_device_t* d, uint16_t cid, const uint8_t* report, uint16_t len);
void uni_hid_device_send_intr_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len);
void uni_hid_device_send_ctrl_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len);
void uni_hid_device_send_queued_reports(uni_hid_device_t* d);

bool uni_hid_device_does_require_hid_descriptor(uni_hid_device_t* d);

bool uni_hid_device_is_gamepad(uni_hid_device_t* d);
bool uni_hid_device_is_mouse(uni_hid_device_t* d);
bool uni_hid_device_is_keyboard(uni_hid_device_t* d);

bool uni_hid_device_is_virtual_device(uni_hid_device_t* d);

#endif  // UNI_HID_DEVICE_H
