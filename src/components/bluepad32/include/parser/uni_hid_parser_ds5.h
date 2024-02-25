// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_DS5_H
#define UNI_HID_PARSER_DS5_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

/******* Dualsense(DS5) Adaptive Trigger Effects - Start *******/
// Built with help of https://gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db, licensed under MIT License
// Currently only implemented ones marked as "safe"

// Switches trigger effect off
void ds5_generate_trigger_effect_off(uint8_t effect[11]);

// position: should between 0 and 9, inclusive
// strength: should between 0 and 8, inclusive
void ds5_generate_trigger_effect_feedback(uint8_t effect[11], const uint8_t position, const uint8_t strength);

// start_position: should be between 2 and 7, inclusive
// end_position: should be between start_position + 1 and 8, inclusive
// strength: should be between 0 and 8, inclusive
void ds5_generate_trigger_effect_weapon(uint8_t effect[11],
                                        const uint8_t start_position,
                                        const uint8_t end_position,
                                        const uint8_t strength);

// Warning! Not to be confused with gamepad rumble feature. This is an adaptive trigger effect!
// position: should be between 0 and 9, inclusive
// amplitude: should be between 0 and 8, inclusive
// frequency: should be in Hz
void ds5_generate_trigger_effect_vibration(uint8_t effect[11],
                                           const uint8_t position,
                                           const uint8_t amplitude,
                                           const uint8_t frequency);
/******* Dualsense(DS5) Adaptive Trigger Effects - End *******/

// For DualSense gamepads
void uni_hid_parser_ds5_setup(struct uni_hid_device_s* d);
void uni_hid_parser_ds5_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_ds5_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_ds5_parse_feature_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_ds5_set_player_leds(struct uni_hid_device_s* d, uint8_t value);
void uni_hid_parser_ds5_set_lightbar_color(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b);
void uni_hid_parser_ds5_set_trigger_effect(struct uni_hid_device_s* d,
                                           const uint8_t trigger_type,
                                           const uint8_t trigger_effect[11]);
void uni_hid_parser_ds5_set_rumble(struct uni_hid_device_s* d, uint8_t value, uint8_t duration);
void uni_hid_parser_ds5_device_dump(struct uni_hid_device_s* d);
#endif  // UNI_HID_PARSER_DS5_H
