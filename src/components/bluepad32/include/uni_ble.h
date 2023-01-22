/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2023 Ricardo Quesada

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

#ifndef UNI_BLE_H
#define UNI_BLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

void uni_ble_on_connection_complete(const uint8_t* packet, uint16_t size);
void uni_ble_on_encryption_change(const uint8_t* packet, uint16_t size);
void uni_ble_on_gap_event_advertising_report(const uint8_t* packet, uint16_t size);

void uni_ble_setup(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BLE_H