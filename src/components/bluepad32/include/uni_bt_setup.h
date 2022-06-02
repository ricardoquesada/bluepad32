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

#ifndef UNI_BT_SETUP_H
#define UNI_BT_SETUP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

int uni_bt_setup(void);
bool uni_bt_setup_is_ready(void);
void uni_bt_setup_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);

// Properties
void uni_bt_setup_set_gap_security_level(int gap);
int uni_bt_setup_get_gap_security_level(void);
void uni_bt_setup_set_gap_inquiry_length(int len);
int uni_bt_setup_get_gap_inquiry_lenght(void);
void uni_bt_setup_set_gap_max_peridic_length(int len);
int uni_bt_setup_get_gap_max_periodic_lenght(void);
void uni_bt_setup_set_gap_min_peridic_length(int len);
int uni_bt_setup_get_gap_min_periodic_lenght(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_SETUP_H