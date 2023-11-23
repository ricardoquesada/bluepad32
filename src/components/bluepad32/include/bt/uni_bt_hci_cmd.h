/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2022 Ricardo Quesada

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
