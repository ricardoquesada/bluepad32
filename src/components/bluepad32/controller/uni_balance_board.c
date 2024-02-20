// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "controller/uni_balance_board.h"

#include "sdkconfig.h"

// Don't compile it on when console is not present
#ifdef CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE

#include <argtable3/argtable3.h>
#include <esp_console.h>

#endif  // CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE

#include "uni_log.h"
#include "uni_property.h"

// Gets initialized at platform_init time.
static uni_balance_board_threshold_t bb_threshold = {
    .move = UNI_BALANCE_BOARD_MOVE_THRESHOLD_DEFAULT,
    .fire = UNI_BALANCE_BOARD_FIRE_THRESHOLD_DEFAULT,
};

#ifdef CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
static struct {
    struct arg_int* value;
    struct arg_end* end;
} bb_move_threshold_args;

static struct {
    struct arg_int* value;
    struct arg_end* end;
} bb_fire_threshold_args;

static void set_bb_move_threshold_to_nvs(int threshold) {
    uni_property_value_t value;
    value.u32 = threshold;

    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD, value);
    logi("Done\n");
}

static int get_bb_move_threshold_from_nvs(void) {
    uni_property_value_t value;

    value = uni_property_get(UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD);
    return value.u32;
}

static void set_bb_fire_threshold_to_nvs(int threshold) {
    uni_property_value_t value;
    value.u32 = threshold;

    uni_property_set(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD, value);
    logi("Done\n");
}

static int get_bb_fire_threshold_from_nvs(void) {
    uni_property_value_t value;

    value = uni_property_get(UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD);
    return value.u32;
}

static int cmd_bb_move_threshold(int argc, char** argv) {
    int nerrors = arg_parse(argc, argv, (void**)&bb_move_threshold_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, bb_move_threshold_args.end, argv[0]);

        // Don't treat as error, just print current value.
        int threshold = get_bb_move_threshold_from_nvs();
        logi("%d\n", threshold);
        return 0;
    }
    int threshold = bb_move_threshold_args.value->ival[0];
    set_bb_move_threshold_to_nvs(threshold);

    // Update static value
    bb_threshold.move = threshold;

    logi("New Balance Board Move threshold: %d\n", threshold);
    return 0;
}

static int cmd_bb_fire_threshold(int argc, char** argv) {
    int nerrors = arg_parse(argc, argv, (void**)&bb_fire_threshold_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, bb_fire_threshold_args.end, argv[0]);

        // Don't treat as error, just print current value.
        int threshold = get_bb_fire_threshold_from_nvs();
        logi("%d\n", threshold);
        return 0;
    }
    int threshold = bb_fire_threshold_args.value->ival[0];
    set_bb_fire_threshold_to_nvs(threshold);

    // Update static value
    bb_threshold.fire = threshold;

    logi("New Balance Board Fire threshold: %d\n", threshold);
    return 0;
}
#endif  // CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE

void uni_balance_board_register_cmds(void) {
#ifdef CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
    bb_move_threshold_args.value = arg_int1(NULL, NULL, "<threshold>", "balance board 'move weight' threshold");
    bb_move_threshold_args.end = arg_end(2);

    bb_fire_threshold_args.value = arg_int1(NULL, NULL, "<threshold>", "balance board 'fire weight' threshold");
    bb_fire_threshold_args.end = arg_end(2);

    const esp_console_cmd_t bb_move_threshold = {
        .command = "bb_move_threshold",
        .help =
            "Get/Set the Balance Board 'Move Weight' threshold\n"
            "Default: 1500",  // BB_MOVE_THRESHOLD_DEFAULT
        .hint = NULL,
        .func = &cmd_bb_move_threshold,
        .argtable = &bb_move_threshold_args,
    };

    const esp_console_cmd_t bb_fire_threshold = {
        .command = "bb_fire_threshold",
        .help =
            "Get/Set the Balance Board 'Fire Weight' threshold\n"
            "Default: 5000",  // BB_FIRE_THRESHOLD_DEFAULT
        .hint = NULL,
        .func = &cmd_bb_fire_threshold,
        .argtable = &bb_fire_threshold_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&bb_move_threshold));
    ESP_ERROR_CHECK(esp_console_cmd_register(&bb_fire_threshold));
#endif  // CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
}

void uni_balance_board_init(void) {
#ifdef CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
    // Update Balance Board threshold
    bb_threshold.move = get_bb_move_threshold_from_nvs();
    bb_threshold.fire = get_bb_fire_threshold_from_nvs();
#endif  // CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
}

void uni_balance_board_dump(const uni_balance_board_t* bb) {
    // Don't add "\n"
    logi("tl=%d, tr=%d, bl=%d, br=%d, temperature=%d", bb->tl, bb->tr, bb->bl, bb->br, bb->temperature);
}

uni_balance_board_threshold_t uni_balance_board_get_threshold(void) {
    return bb_threshold;
}
