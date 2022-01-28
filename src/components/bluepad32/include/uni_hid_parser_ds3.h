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

#ifndef UNI_HID_PARSER_DS3_H
#define UNI_HID_PARSER_DS3_H

#include <stdbool.h>
#include <stdint.h>

#include "uni_hid_parser.h"

// For DUALSHOCK 3 gamepads
void uni_hid_parser_ds3_setup(struct uni_hid_device_s* d);
void uni_hid_parser_ds3_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_ds3_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_ds3_set_player_leds(struct uni_hid_device_s* d, uint8_t leds);
void uni_hid_parser_ds3_set_rumble(struct uni_hid_device_s* d, uint8_t value, uint8_t duration);
bool uni_hid_parser_ds3_does_name_match(struct uni_hid_device_s* d, const char* name);

#endif  // UNI_HID_PARSER_DS3_H
