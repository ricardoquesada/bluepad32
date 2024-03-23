// SPDX-License-Identifier: Apache-2.0, MIT
// Copyright 2019 Ricardo Quesada
// 2021-2022 John "Nielk1" Klein
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_DS5_H
#define UNI_HID_PARSER_DS5_H

#include <stdint.h>

#include "parser/uni_hid_parser.h"

typedef struct __attribute((packed)) {
    uint8_t effect;
    uint8_t data[10];
} ds5_adaptive_trigger_effect_t;

typedef enum {
    UNI_ADAPTIVE_TRIGGER_TYPE_LEFT,
    UNI_ADAPTIVE_TRIGGER_TYPE_RIGHT,
} ds5_adaptive_trigger_type_t;

// For DualSense gamepads
void uni_hid_parser_ds5_setup(struct uni_hid_device_s* d);
void uni_hid_parser_ds5_init_report(struct uni_hid_device_s* d);
void uni_hid_parser_ds5_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_ds5_parse_feature_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len);
void uni_hid_parser_ds5_set_player_leds(struct uni_hid_device_s* d, uint8_t value);
void uni_hid_parser_ds5_set_lightbar_color(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b);
void uni_hid_parser_ds5_play_dual_rumble(struct uni_hid_device_s* d,
                                         uint16_t start_delay_ms,
                                         uint16_t duration_ms,
                                         uint8_t weak_magnitude,
                                         uint8_t strong_magnitude);
void uni_hid_parser_ds5_device_dump(struct uni_hid_device_s* d);

// Unique to DualSense. Not part of the "hid_parser" interface
// Warning: Adaptive trigger API is experimental. It might change in the future without further notice.

// Turns off adaptive trigger effect.
ds5_adaptive_trigger_effect_t ds5_new_adaptive_trigger_effect_off(void);

// Provides feedback when the user depresses the trigger equal to, or greater than, the start position.
// position: should between 0 and 9, inclusive
// strength: should between 0 and 8, inclusive
ds5_adaptive_trigger_effect_t ds5_new_adaptive_trigger_effect_feedback(uint8_t position, uint8_t strength);

// Provides feedback when the user depresses the trigger between the start and the end positions.
// start_position: should be between 2 and 7, inclusive
// end_position: should be between start_position + 1 and 8, inclusive
// strength: should be between 0 and 8, inclusive
ds5_adaptive_trigger_effect_t ds5_new_adaptive_trigger_effect_weapon(uint8_t start_position,
                                                                     uint8_t end_position,
                                                                     uint8_t strength);

// Vibrates when the user depresses the trigger equal to, or greater than, the start position.
// position: should be between 0 and 9, inclusive
// amplitude: should be between 0 and 8, inclusive
// frequency: should be in Hz
ds5_adaptive_trigger_effect_t ds5_new_adaptive_trigger_effect_vibration(uint8_t position,
                                                                        uint8_t amplitude,
                                                                        uint8_t frequency);

void ds5_set_adaptive_trigger_effect(struct uni_hid_device_s* d,
                                     ds5_adaptive_trigger_type_t trigger_type,
                                     const ds5_adaptive_trigger_effect_t* effect);

#endif  // UNI_HID_PARSER_DS5_H
