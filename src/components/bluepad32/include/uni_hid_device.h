/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2019 Ricardo Quesada

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

#ifndef UNI_HID_DEVICE_H
#define UNI_HID_DEVICE_H

#include <btstack.h>
#include <stdbool.h>
#include <stdint.h>

#include "uni_bt_conn.h"
#include "uni_circular_buffer.h"
#include "uni_controller.h"
#include "uni_gamepad.h"
#include "uni_hid_parser.h"

#define HID_MAX_NAME_LEN 240
#define HID_MAX_DESCRIPTOR_LEN 512
#define HID_DEVICE_MAX_PARSER_DATA 128
#define HID_DEVICE_MAX_PLATFORM_DATA 128
// HID_DEVICE_CONNECTION_TIMEOUT_MS includes the time from when the device is created until it is ready.
#define HID_DEVICE_CONNECTION_TIMEOUT_MS 20000

typedef enum {
    CONTROLLER_SUBTYPE_NONE = 0,
    CONTROLLER_SUBTYPE_WIIMOTE_HORIZ,
    CONTROLLER_SUBTYPE_WIIMOTE_VERT,
    CONTROLLER_SUBTYPE_WIIMOTE_ACCEL,
    CONTROLLER_SUBTYPE_WIIMOTE_NCHK,
    CONTROLLER_SUBTYPE_WIIMOTE_NCHK2JOYS,
    CONTROLLER_SUBTYPE_WIIMOTE_NCHKACCEL,
    CONTROLLER_SUBTYPE_WII_CLASSIC,
    CONTROLLER_SUBTYPE_WIIUPRO,
    CONTROLLER_SUBTYPE_WII_BALANCE_BOARD
} uni_controller_subtype_t;

typedef enum {
    SDP_QUERY_AFTER_CONNECT,   // If not set, this is the default one.
    SDP_QUERY_BEFORE_CONNECT,  // Special case for DualShock4 1st generation.
    SDP_QUERY_NOT_NEEDED,      // Because the Controller type was inferred by other means.
} uni_sdp_query_type_t;

struct uni_hid_device_s {
    uint32_t cod;  // class of device
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
    // Unijoysticle + btstack + libusb in Linux. The correct thing to do is to
    // debug the Linux connection and see what packets are sent before the
    // connection.
    uni_sdp_query_type_t sdp_query_type;

    // Channels
    uint16_t hids_cid;  // BLE only

    // TODO: Create a union of gamepad/mouse/keyboard structs
    // At the moment "mouse" reuses gamepad struct, but it is a hack.
    // Gamepad
    uint16_t controller_type;                     // type of controller. E.g: DualShock4, Switch ,etc.
    uni_controller_subtype_t controller_subtype;  // sub-type of controller attached
    uni_controller_t controller;                  // What kind of controller it is

    // Functions used to parse the usage page/usage.
    uni_report_parser_t report_parser;

    // Buttons that needs to be released before triggering the action again.
    uint32_t misc_button_wait_release;
    // Buttons that needs to wait for a delay before triggering the action again.
    uint32_t misc_button_wait_delay;
    // Needed for Nintendo Switch family of controllers.
    btstack_timer_source_t misc_button_delay_timer;

    // Circular buffer that contains the outgoing packets that couldn't be sent
    // immediately.
    uni_circular_buffer_t outgoing_buffer;

    // Bytes reserved to gamepad's parser instances.
    // E.g: The Wii driver uses it for the state machine.
    uint8_t parser_data[HID_DEVICE_MAX_PARSER_DATA];

    // Bytes reserved to different platforms.
    // E.g: C64 or Airlift might use it to store different values.
    uint8_t platform_data[HID_DEVICE_MAX_PLATFORM_DATA];

    // Bluetooth connection info.
    uni_bt_conn_t conn;
};
typedef struct uni_hid_device_s uni_hid_device_t;

// Callback function used as in get_instance_with_predicate
typedef uint8_t (*uni_hid_device_predicate_t)(uni_hid_device_t* d, void* data);

void uni_hid_device_setup(void);

uni_hid_device_t* uni_hid_device_create(bd_addr_t address);

// Don't add any other get_instance_for_XXX function.
// Insteaad use: get_instance_with_predicate()
uni_hid_device_t* uni_hid_device_get_instance_for_address(bd_addr_t addr);
uni_hid_device_t* uni_hid_device_get_instance_for_cid(uint16_t cid);
// BLE only
uni_hid_device_t* uni_hid_device_get_instance_for_hids_cid(uint16_t cid);
uni_hid_device_t* uni_hid_device_get_instance_for_connection_handle(hci_con_handle_t handle);
uni_hid_device_t* uni_hid_device_get_first_device_with_state(uni_bt_conn_state_t state);
uni_hid_device_t* uni_hid_device_get_instance_with_predicate(uni_hid_device_predicate_t predicate, void* data);
uni_hid_device_t* uni_hid_device_get_instance_for_idx(int idx);
int uni_hid_device_get_idx_for_instance(uni_hid_device_t* d);

void uni_hid_device_init(uni_hid_device_t* d);

void uni_hid_device_set_ready(uni_hid_device_t* d);
void uni_hid_device_set_ready_complete(uni_hid_device_t* d);

void uni_hid_device_request_inquire(void);

void uni_hid_device_on_connected(uni_hid_device_t* d, bool connected);
void uni_hid_device_connect(uni_hid_device_t* d);
void uni_hid_device_disconnect(uni_hid_device_t* d);
void uni_hid_device_delete(uni_hid_device_t* d);

void uni_hid_device_set_cod(uni_hid_device_t* d, uint32_t cod);
bool uni_hid_device_is_cod_supported(uint32_t cod);

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

#endif  // UNI_HID_DEVICE_H
