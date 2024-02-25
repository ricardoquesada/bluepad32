// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_PARSER_H
#define UNI_HID_PARSER_H

#include <stdint.h>

// Forward declarations
struct uni_hid_device_s;

// BTstack bug:
// see: https://github.com/bluekitchen/btstack/issues/187
struct hid_globals_s {
    int32_t logical_minimum;
    int32_t logical_maximum;
    uint16_t usage_page;
    uint8_t report_size;
    uint8_t report_count;
    uint8_t report_id;
};
typedef struct hid_globals_s hid_globals_t;

typedef void (*report_setup_fn_t)(struct uni_hid_device_s* d);
typedef void (*report_init_report_fn_t)(struct uni_hid_device_s* d);
typedef void (*report_parse_usage_fn_t)(struct uni_hid_device_s* d,
                                        hid_globals_t* globals,
                                        uint16_t usage_page,
                                        uint16_t usage,
                                        int32_t value);
// "parse_input_report" receives uni_hid_device_s instead of gamepad since it is needed
// for devices like Nintendo. If needed, the same thing should be done for
// "parse_usage".
typedef void (*report_parse_input_report_fn_t)(struct uni_hid_device_s* d, const uint8_t* report, uint16_t report_len);
typedef void (*report_parse_feature_report_fn_t)(struct uni_hid_device_s* d,
                                                 const uint8_t* report,
                                                 uint16_t report_len);
typedef void (*report_set_player_leds_fn_t)(struct uni_hid_device_s* d, uint8_t leds);
typedef void (*report_set_lightbar_color_fn_t)(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b);
typedef void (*report_set_trigger_effect_fn_t)(struct uni_hid_device_s* d,
                                               const uint8_t trigger_type,
                                               const uint8_t trigger_effect[11]);
typedef void (*report_set_rumble_fn_t)(struct uni_hid_device_s* d, uint8_t force, uint8_t duration);
typedef void (*report_device_dump_t)(struct uni_hid_device_s* d);

// Parsers should implement these optional functions:
typedef struct {
    // Called only once when the type of gamepad is known.
    report_setup_fn_t setup;
    // Called before starting a new report
    report_init_report_fn_t init_report;
    // Called for each usage in the report: usage page + usage + value
    report_parse_usage_fn_t parse_usage;
    // Called with the raw input report
    report_parse_input_report_fn_t parse_input_report;
    // Called with the feature report
    report_parse_feature_report_fn_t parse_feature_report;
    // If implemented, turns on/off the different gamepad LEDs
    report_set_player_leds_fn_t set_player_leds;
    // If implemented, changes the lightbar color (e.g.: in DS4 and DualSense)
    report_set_lightbar_color_fn_t set_lightbar_color;
    // If implemented, applies trigger effects on triggers (e.g.: in DS5)
    report_set_trigger_effect_fn_t set_trigger_effect;
    // If implemented, activates rumble in the gamepad
    report_set_rumble_fn_t set_rumble;
    // If implemented, it dumps device info
    report_device_dump_t device_dump;
} uni_report_parser_t;

void uni_hid_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t report_len);
int32_t uni_hid_parser_process_axis(hid_globals_t* globals, uint32_t value);
int32_t uni_hid_parser_process_pedal(hid_globals_t* globals, uint32_t value);
uint8_t uni_hid_parser_process_hat(hid_globals_t* globals, uint32_t value);
void uni_hid_parser_process_dpad(uint16_t usage, uint32_t value, uint8_t* dpad);
uint8_t uni_hid_parser_hat_to_dpad(uint8_t hat);

#endif  // UNI_HID_PARSER_H
