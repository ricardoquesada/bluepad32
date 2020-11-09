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

#include <stdbool.h>
#include <stdint.h>

#include "btstack.h"
#include "uni_circular_buffer.h"
#include "uni_gamepad.h"
#include "uni_hid_parser.h"

#define MAX_NAME_LEN 240
#define MAX_DESCRIPTOR_LEN 512

// clang-format off
#define MASK_COD_MAJOR_PERIPHERAL   0x0500  // 0b0000_0101_0000_0000
#define MASK_COD_MAJOR_AUDIO        0x0400  // 0b0000_0100_0000_0000
#define MASK_COD_MINOR_MASK         0x00FC  //             1111_1100
#define MASK_COD_MINOR_POINT_DEVICE 0x0080  //             1000_0000
#define MASK_COD_MINOR_KEYBOARD     0x0040  //             0100_0000
#define MASK_COD_MINOR_GAMEPAD      0x0008  //             0000_1000
#define MASK_COD_MINOR_JOYSTICK     0x0004  //             0000_0100
#define MASK_COD_MINOR_HANDS_FREE   0x0008  //             0000_1000
// clang-format on

enum DEVICE_STATE {
  STATE_DEVICE_DISCOVERED,
  STATE_REMOTE_NAME_REQUEST,
  STATE_REMOTE_NAME_INQUIRED,
  STATE_REMOTE_NAME_FETCHED,
  STATE_SDP_HID_DESCRIPTOR_REQUESTED,
  STATE_SDP_HID_DESCRIPTOR_FETCHED,
  STATE_SDP_VENDOR_REQUESTED,
  STATE_SDP_VENDOR_FETCHED,
  STATE_L2CAP_CONTROL_CONNECTION_REQUESTED,
  STATE_L2CAP_CONTROL_CONNECTED,
  STATE_L2CAP_INTERRUPT_CONNECTION_REQUESTED,
  STATE_L2CAP_INTERRUPT_CONNECTED,
  STATE_JOYSTICK_ASSIGNED,
};

struct uni_hid_device_s {
  bd_addr_t address;
  hci_con_handle_t con_handle;
  uint8_t page_scan_repetition_mode;
  uint16_t clock_offset;
  uint32_t cod;
  uint16_t vendor_id;
  uint16_t product_id;
  char name[MAX_NAME_LEN];

  // incoming, connected, hid, cod, etc...
  uint32_t flags;

  // SDP
  uint8_t hid_descriptor[MAX_DESCRIPTOR_LEN];
  uint16_t hid_descriptor_len;
  // DualShock4 1st gen requires to do the SDP query before l2cap connect,
  // otherwise it won't work.
  // And Nintendo Switch Pro gamepad requires to do the SDP query after l2cap
  // connect, so we use this variable to determine when to do the SDP query.
  // TODO: Actually this is not entirely true since it works Ok when using
  // Unijoysticle + btstack + libusb in Linux. The correct thing to do is to
  // debug the linux connection and see what packets are sent before the
  // connection.
  uint8_t sdp_query_before_connect;

  // Channels
  uint16_t hid_control_cid;
  uint16_t hid_interrupt_cid;
  enum DEVICE_STATE state;

  // Gamepad
  uint8_t controller_type;            // type of controller attached
  uni_emulation_mode_t emu_mode;      // type of controller to emulate
  uni_joystick_port_t joystick_port;  // which port does it control, A or B?
  uni_joystick_port_t
      prev_joystick_port;  // which port was used before switching emu mode

  // Functions used to parse the usage page/usage.
  uni_report_parser_t report_parser;

  uni_gamepad_t gamepad;

  // Buttons that needs to be released before triggering the action again.
  uint32_t wait_release_misc_button;

  // Circular buffer that contains the outgoing packets that couldn't be sent
  // immediately.
  uni_circular_buffer_t outgoing_buffer;

  // Bytes reserved to gamepad's instances.
  // E.g: The Wii driver uses it for the state machine.
  uint8_t data[32];
};
typedef struct uni_hid_device_s uni_hid_device_t;

void uni_hid_device_init(void);

uni_hid_device_t* uni_hid_device_create(bd_addr_t address);
uni_hid_device_t* uni_hid_device_get_instance_for_address(bd_addr_t addr);
uni_hid_device_t* uni_hid_device_get_instance_for_cid(uint16_t cid);
uni_hid_device_t* uni_hid_device_get_instance_for_connection_handle(
    hci_con_handle_t handle);
uni_hid_device_t* uni_hid_device_get_first_device_with_state(
    enum DEVICE_STATE state);

// Which device is currently doing a SDP query.
void uni_hid_device_set_sdp_device(uni_hid_device_t* d);
// Returns the elapsed time since the last SDP query in microseconds.
uni_hid_device_t* uni_hid_device_get_sdp_device(uint64_t* elapsed /*out*/);

void uni_hid_device_assign_joystick_port(uni_hid_device_t* d);

void uni_hid_device_remove_entry_with_channel(uint16_t channel);

void uni_hid_device_request_inquire(void);

void uni_hid_device_set_connected(uni_hid_device_t* d, bool connected);

void uni_hid_device_set_cod(uni_hid_device_t* d, uint32_t cod);
bool uni_hid_device_is_cod_supported(uint32_t cod);

void uni_hid_device_set_hid_descriptor(uni_hid_device_t* d,
                                       const uint8_t* descriptor, int len);
bool uni_hid_device_has_hid_descriptor(uni_hid_device_t* d);

void uni_hid_device_set_incoming(uni_hid_device_t* d, bool incoming);
bool uni_hid_device_is_incoming(uni_hid_device_t* d);

void uni_hid_device_set_name(uni_hid_device_t* d, const uint8_t* name,
                             int name_len);
bool uni_hid_device_has_name(uni_hid_device_t* d);

void uni_hid_device_set_product_id(uni_hid_device_t* d, uint16_t product_id);
uint16_t uni_hid_device_get_product_id(uni_hid_device_t* d);

void uni_hid_device_set_vendor_id(uni_hid_device_t* d, uint16_t vendor_id);
uint16_t uni_hid_device_get_vendor_id(uni_hid_device_t* d);

void uni_hid_device_dump_device(uni_hid_device_t* d);
void uni_hid_device_dump_all(void);

bool uni_hid_device_is_orphan(uni_hid_device_t* d);

uint8_t uni_hid_device_guess_controller_type_from_packet(uni_hid_device_t* d,
                                                         const uint8_t* packet,
                                                         int len);
void uni_hid_device_guess_controller_type_from_pid_vid(uni_hid_device_t* d);
bool uni_hid_device_has_controller_type(uni_hid_device_t* d);

void uni_hid_device_process_gamepad(uni_hid_device_t* d);

void uni_hid_device_set_connection_handle(uni_hid_device_t* d,
                                          hci_con_handle_t handle);

void uni_hid_device_set_state(uni_hid_device_t* d, enum DEVICE_STATE s);
enum DEVICE_STATE uni_hid_device_get_state(uni_hid_device_t* d);

void uni_hid_device_send_report(uni_hid_device_t* d, uint16_t cid,
                                const uint8_t* report, uint16_t len);
void uni_hid_device_send_intr_report(uni_hid_device_t* d, const uint8_t* report,
                                     uint16_t len);
void uni_hid_device_send_ctrl_report(uni_hid_device_t* d, const uint8_t* report,
                                     uint16_t len);
void uni_hid_device_send_queued_reports(uni_hid_device_t* d);

void uni_hid_device_set_joystick_port(uni_hid_device_t* d,
                                      uni_joystick_port_t p);

// events

void uni_hid_device_on_emu_mode_change(void);

#endif  // UNI_HID_DEVICE_H
