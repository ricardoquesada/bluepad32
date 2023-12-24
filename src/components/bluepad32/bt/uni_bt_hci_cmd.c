// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "bt/uni_bt_hci_cmd.h"

// 1: Filter type: Connection Setup (0x02)
// 1: Filter condition type: Allow connection from Class of Devices (0x01)
// 3: COD
// 3: COD Mask
// 1: Autoaccept: 0x011 (no auto-accept), 0x02 (no auto-accept with role disabled)
//    0x03 (no auto-accept with role enabled)
const hci_cmd_t hci_set_event_filter_connection_cod = {HCI_OPCODE_HCI_SET_EVENT_FILTER, "11331"};

// 1: Filter type: Inquiry (0x01)
// 1: Filter condition type: Allow connection from Class of Devices (0x01)
// 3: COD
// 3: COD Mask
const hci_cmd_t hci_set_event_filter_inquiry_cod = {HCI_OPCODE_HCI_SET_EVENT_FILTER, "1133"};
