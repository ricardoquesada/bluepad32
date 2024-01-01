// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_HCI_CMD_H
#define UNI_HID_HCI_CMD_H

#include <stdint.h>

#include <hci_cmd.h>

enum {
    HCI_OPCODE_HCI_SET_EVENT_FILTER = HCI_OPCODE(OGF_CONTROLLER_BASEBAND, 0x05),
};

// Controller Baseband
extern const hci_cmd_t hci_set_event_filter_connection_cod;
extern const hci_cmd_t hci_set_event_filter_inquiry_cod;

#endif /* UNI_HID_HCI_CMD_H */
