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

#include "uni_hci_cmd.h"

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
