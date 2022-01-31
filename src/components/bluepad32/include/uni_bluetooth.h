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

#ifndef UNI_BLUETOOTH_H
#define UNI_BLUETOOTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "uni_hid_device.h"

int uni_bluetooth_init(void);

// Public functions
// Safe to call these functions from another task and/or CPU
void uni_bluetooth_del_keys_safe(void);
void uni_bluetooth_enable_new_connections_safe(bool enabled);

// Private functions.
// TODO: Should be moved to a new file: uni_bt_state.c
void uni_bluetooth_process_fsm(uni_hid_device_t* d);
void uni_bluetooth_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BLUETOOTH_H