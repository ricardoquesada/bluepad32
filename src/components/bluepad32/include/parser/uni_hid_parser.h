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
// Optional teardown hook invoked by uni_hid_device_delete() and uni_hid_device_setup()
// BEFORE uni_hid_device_init() zeroes the device struct with memset. Parsers that embed
// intrusive btstack_timer_source_t nodes inside parser_data[] (such as Switch setup_timer)
// must implement this callback to remove their timers from BTstack's run loop first.
typedef void (*report_deinit_fn_t)(struct uni_hid_device_s* d);
typedef void (*report_init_report_fn_t)(struct uni_hid_device_s* d);
typedef void (*report_parse_usage_fn_t)(struct uni_hid_device_s* d,
                                        const hid_globals_t* globals,
                                        uint16_t usage_page,
                                        uint16_t usage,
                                        int32_t value);
// "Parse_input_report" receives uni_hid_device_s instead of gamepad since it is needed
// for devices like Nintendo. If needed, the same thing should be done for
// "parse_usage".
typedef void (*report_parse_input_report_fn_t)(struct uni_hid_device_s* d, const uint8_t* report, uint16_t report_len);
typedef void (*report_parse_feature_report_fn_t)(struct uni_hid_device_s* d,
                                                 const uint8_t* report,
                                                 uint16_t report_len);
// Sets the controller's player indicator LEDs.
// `leds` is a 4-bit bitmask (`BIT(0)..BIT(3)`, `0x00..0x0f`), corresponding to `uni_gamepad_seat_t`
// (`GAMEPAD_SEAT_NONE = 0x00`, `GAMEPAD_SEAT_A = BIT(0)`, `GAMEPAD_SEAT_B = BIT(1)`,
// `GAMEPAD_SEAT_C = BIT(2)`, `GAMEPAD_SEAT_D = BIT(3)`) or multi-bit combinations
// (e.g. `GAMEPAD_SEAT_AB_MASK = 0x03`), NOT a sequential player index `0..4`.
typedef void (*report_set_player_leds_fn_t)(struct uni_hid_device_s* d, uint8_t leds);
typedef void (*report_set_lightbar_color_fn_t)(struct uni_hid_device_s* d, uint8_t r, uint8_t g, uint8_t b);
// start_delay_ms: a delayed start measured in milliseconds. Use 0 to start rumble immediately.
// duration_ms: duration of rumble in milliseconds. The controller might limit the max duration. 0 means stop rumble.
// weak_magnitude: The magnitude for the "weak motor".
// strong_magnitude: The magnitude for the "strong motor".
// If the controller has only one motor, then the max value between "weak" and "strong" is used.
typedef void (*report_play_dual_rumble_fn_t)(struct uni_hid_device_s* d,
                                             uint16_t start_delay_ms,
                                             uint16_t duration_ms,
                                             uint8_t weak_magnitude,
                                             uint8_t strong_magnitude);
typedef void (*report_device_dump_t)(struct uni_hid_device_s* d);

// Parsers should implement these optional functions:
typedef struct {
    // Called only once when the type of gamepad is known.
    report_setup_fn_t setup;
    // Called before a device slot is zeroed/deleted to clean up parser-owned timers/resources.
    report_deinit_fn_t deinit;
    // Called before starting a new report
    report_init_report_fn_t init_report;
    // Called for each usage in the report: usage page + usage + value
    report_parse_usage_fn_t parse_usage;
    // Called with the raw input report
    report_parse_input_report_fn_t parse_input_report;
    // Called with the feature report
    report_parse_feature_report_fn_t parse_feature_report;
    // If implemented, turns on/off the gamepad player LEDs using a 4-bit bitmask (`BIT(0)..BIT(3)` /
    // `uni_gamepad_seat_t`).
    report_set_player_leds_fn_t set_player_leds;
    // If implemented, changes the lightbar color (e.g.: in DS4 and DualSense)
    report_set_lightbar_color_fn_t set_lightbar_color;
    // If implemented, activates rumble in the gamepad
    report_play_dual_rumble_fn_t play_dual_rumble;
    // If implemented, it dumps device info
    report_device_dump_t device_dump;
} uni_report_parser_t;

void uni_hid_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t report_len);
int32_t uni_hid_parser_process_axis(const hid_globals_t* globals, uint32_t value);
int32_t uni_hid_parser_process_pedal(const hid_globals_t* globals, uint32_t value);
uint8_t uni_hid_parser_process_hat(const hid_globals_t* globals, uint32_t value);
void uni_hid_parser_process_dpad(uint16_t usage, uint32_t value, uint8_t* dpad);
uint8_t uni_hid_parser_hat_to_dpad(uint8_t hat);

#endif  // UNI_HID_PARSER_H
