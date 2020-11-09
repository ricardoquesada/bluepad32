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

#ifndef UNI_HID_PARSER_WII_H
#define UNI_HID_PARSER_WII_H

#include <stdint.h>

#include "btstack.h"
#include "uni_hid_parser.h"

void uni_hid_parser_wii_setup(struct uni_hid_device_s* d);
void uni_hid_parser_wii_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_wii_parse_raw(struct uni_hid_device_s* d,
                                  const uint8_t* report, uint16_t len);
void uni_hid_parser_wii_update_led(struct uni_hid_device_s* d);

#endif  // UNI_HID_PARSER_WIIUPRO_H