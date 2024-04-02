// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_console.h"

#include <argtable3/argtable3.h>
#include <cmd_system.h>
#include <esp_console.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "sdkconfig.h"

#include "bt/uni_bt.h"
#include "bt/uni_bt_allowlist.h"
#include "bt/uni_bt_le.h"
#include "platform/uni_platform.h"
#include "uni_common.h"
#include "uni_gpio.h"
#include "uni_log.h"
#include "uni_mouse_quadrature.h"
#include "uni_property.h"
#include "uni_virtual_device.h"

static const char* TAG = "console";
#define PROMPT_STR "bp32"

static char buf_disconnect[16];

static struct {
    struct arg_dbl* value;
    struct arg_end* end;
} mouse_scale_args;

static struct {
    struct arg_int* value;
    struct arg_end* end;
} gap_security_level_args;

static struct {
    struct arg_int* max;
    struct arg_int* min;
    struct arg_int* len;
    struct arg_end* end;
} gap_periodic_inquiry_args;

static struct {
    struct arg_int* enabled;
    struct arg_end* end;
} incoming_connections_enable_args;

static struct {
    struct arg_int* enabled;
    struct arg_end* end;
} ble_enable_args;

static struct {
    struct arg_int* idx;
    struct arg_end* end;
} disconnect_device_args;

static struct {
    struct arg_str* addr;
    struct arg_end* end;
} allowlist_addr_args;

static struct {
    struct arg_int* enabled;
    struct arg_end* end;
} allowlist_enable_args;

static struct {
    struct arg_int* enabled;
    struct arg_end* end;
} virtual_device_enable_args;

static struct {
    struct arg_str* prop;
    struct arg_end* end;
} getprop_args;

static int list_devices(int argc, char** argv) {
    // FIXME: Should not belong to "bluetooth"
    uni_bt_dump_devices_safe();

    // This function prints to console. print bp32> after a delay
    TickType_t ticks = pdMS_TO_TICKS(250);
    vTaskDelay(ticks);
    return 0;
}

static void print_mouse_scale(void) {
    char buf[32];
    float scale = uni_mouse_quadrature_get_scale_factor();

    // ets_printf() doesn't support "%f"
    sprintf(buf, "%f\n", scale);
    logi(buf);
}

static int mouse_scale(int argc, char** argv) {
    float scale;

    int nerrors = arg_parse(argc, argv, (void**)&mouse_scale_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mouse_scale_args.end, argv[0]);

        // Don't treat as error, just print current value.
        print_mouse_scale();
        return 0;
    }

    scale = mouse_scale_args.value->dval[0];
    uni_mouse_quadrature_set_scale_factor(scale);
    logi("Done\n");
    return 0;
}

static int set_gap_security_level(int argc, char** argv) {
    int gap;

    int nerrors = arg_parse(argc, argv, (void**)&gap_security_level_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, gap_security_level_args.end, argv[0]);

        // Don't treat as error, just print current value.
        int gap = uni_bt_get_gap_security_level();
        logi("%d\n", gap);
        return 0;
    }

    gap = gap_security_level_args.value->ival[0];
    uni_bt_set_gap_security_level(gap);
    logi("Done. Restart required. Type 'restart' + Enter\n");
    return 0;
}

static void print_gap_periodic_inquiry(void) {
    int max = uni_bt_get_gap_max_periodic_length();
    int min = uni_bt_get_gap_min_periodic_length();
    int len = uni_bt_get_gap_inquiry_length();
    logi("GAP max periodic len: %d, min periodic len: %d, inquiry len: %d\n", max, min, len);
}

static int gap_periodic_inquiry(int argc, char** argv) {
    int min, max, len;

    int nerrors = arg_parse(argc, argv, (void**)&gap_periodic_inquiry_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, gap_periodic_inquiry_args.end, argv[0]);

        // Don't treat as error, just print current value.
        print_gap_periodic_inquiry();
        return 0;
    }

    max = gap_periodic_inquiry_args.max->ival[0];
    min = gap_periodic_inquiry_args.min->ival[0];
    len = gap_periodic_inquiry_args.len->ival[0];
    uni_bt_set_gap_max_peridic_length(max);
    uni_bt_set_gap_min_peridic_length(min);
    uni_bt_set_gap_inquiry_length(len);
    logi("Done. Restart required. Type 'restart' + Enter\n");
    return 0;
}

static int incoming_connections_enable(int argc, char** argv) {
    int enabled;

    int nerrors = arg_parse(argc, argv, (void**)&incoming_connections_enable_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, incoming_connections_enable_args.end, argv[0]);
        return 0;
    }

    enabled = incoming_connections_enable_args.enabled->ival[0];
    uni_bt_enable_new_connections_safe(!!enabled);
    return 0;
}

static int ble_enable(int argc, char** argv) {
    int enabled;

    int nerrors = arg_parse(argc, argv, (void**)&ble_enable_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, ble_enable_args.end, argv[0]);

        // Don't treat as error, just print current value.
        logi("BLE: %s\n", uni_bt_le_is_enabled() ? "Enabled" : "Disabled");
        return 0;
    }

    enabled = ble_enable_args.enabled->ival[0];
    uni_bt_le_set_enabled(!!enabled);
    logi("Done. Restart required. Type 'restart' + Enter\n");
    return 0;
}

static int list_bluetooth_keys(int argc, char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    uni_bt_list_keys_safe();

    // This function prints to console. print bp32> after a delay
    TickType_t ticks = pdMS_TO_TICKS(250);
    vTaskDelay(ticks);
    return 0;
}

static int del_bluetooth_keys(int argc, char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    uni_bt_del_keys_safe();
    // This function prints to console. print bp32> after a delay
    TickType_t ticks = pdMS_TO_TICKS(250);
    vTaskDelay(ticks);
    return 0;
}

static int disconnect_device(int argc, char** argv) {
    int idx;
    int nerrors = arg_parse(argc, argv, (void**)&disconnect_device_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, disconnect_device_args.end, argv[0]);
        return 1;
    }

    idx = disconnect_device_args.idx->ival[0];
    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
        return 1;

    uni_bt_disconnect_device_safe(idx);
    return 0;
}

static int allowlist_list(int argc, char** argv) {
    uni_bt_allowlist_list();
    return 0;
}

static int allowlist_add_addr(int argc, char** argv) {
    bd_addr_t addr;

    int nerrors = arg_parse(argc, argv, (void**)&allowlist_addr_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, allowlist_addr_args.end, argv[0]);
        return 1;
    }

    sscanf_bd_addr(allowlist_addr_args.addr->sval[0], addr);
    uni_bt_allowlist_add_addr(addr);
    return 0;
}

static int allowlist_remove_addr(int argc, char** argv) {
    bd_addr_t addr;

    int nerrors = arg_parse(argc, argv, (void**)&allowlist_addr_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, allowlist_addr_args.end, argv[0]);
        return 1;
    }

    sscanf_bd_addr(allowlist_addr_args.addr->sval[0], addr);
    uni_bt_allowlist_remove_addr(addr);
    return 0;
}

static int allowlist_enable(int argc, char** argv) {
    int enabled;

    int nerrors = arg_parse(argc, argv, (void**)&allowlist_enable_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, allowlist_enable_args.end, argv[0]);

        // Don't treat it as error, just report the current value
        logi("Bluetooth Allowlist: %s\n", uni_bt_allowlist_is_enabled() ? "Enabled" : "Disabled");
        return 0;
    }

    enabled = allowlist_enable_args.enabled->ival[0];

    uni_bt_allowlist_set_enabled(enabled);
    return 0;
}

static int virtual_device_enable(int argc, char** argv) {
    int enabled;

    int nerrors = arg_parse(argc, argv, (void**)&virtual_device_enable_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, virtual_device_enable_args.end, argv[0]);

        // Don't treat it as error, just report the current value
        logi("Virtual Device: %s\n", uni_virtual_device_is_enabled() ? "Enabled" : "Disabled");
        return 0;
    }

    enabled = virtual_device_enable_args.enabled->ival[0];

    uni_virtual_device_set_enabled(enabled);
    return 0;
}

static int getprop(int argc, char** argv) {
    int nerrors = arg_parse(argc, argv, (void**)&getprop_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, getprop_args.end, argv[0]);

        // Don't treat it as error, report the current value
        uni_property_dump_all();
        return 0;
    }

    if (!getprop_args.prop->sval[0])
        return -1;

    for (int i = 0; i < UNI_PROPERTY_IDX_COUNT; i++) {
        const uni_property_t* p = uni_property_get_property_by_name(getprop_args.prop->sval[0]);
        if (!p)
            continue;
        if (strcmp(p->name, getprop_args.prop->sval[0]) == 0) {
            uni_property_dump_property(p);
            break;
        }
    }
    return 0;
}

#ifdef CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE

static void register_bluepad32() {
    mouse_scale_args.value = arg_dbl1(NULL, NULL, "<value>", "Global mouse scale factor. Higher means faster");
    mouse_scale_args.end = arg_end(2);

    gap_security_level_args.value = arg_int1(NULL, NULL, "<value>", "GAP security level");
    gap_security_level_args.end = arg_end(2);

    gap_periodic_inquiry_args.max = arg_int1(NULL, NULL, "<max>", "Max periodic length. Must be bigger than <min>");
    gap_periodic_inquiry_args.min = arg_int1(NULL, NULL, "<min>", "Min periodic length. Must be less than <max>");
    gap_periodic_inquiry_args.len = arg_int1(NULL, NULL, "<len>", "Inquiry length. Must be less than <min>");
    gap_periodic_inquiry_args.end = arg_end(4);

    incoming_connections_enable_args.enabled =
        arg_int1(NULL, NULL, "<0 | 1>", "Whether to allow Bluetooth incoming connections");
    incoming_connections_enable_args.end = arg_end(2);

    ble_enable_args.enabled = arg_int1(NULL, NULL, "<0 | 1>", "Whether to enable Bluetooth Low Energy (BLE)");
    ble_enable_args.end = arg_end(2);

    snprintf(buf_disconnect, sizeof(buf_disconnect) - 1, "<0 - %d>", CONFIG_BLUEPAD32_MAX_DEVICES - 1);
    disconnect_device_args.idx = arg_int1(NULL, NULL, buf_disconnect, "Device index to disconnect");
    disconnect_device_args.end = arg_end(2);

    allowlist_addr_args.addr = arg_str1(NULL, NULL, "<address>", "format: 01:23:45:67:89:ab");
    allowlist_addr_args.end = arg_end(2);
    allowlist_enable_args.enabled = arg_int1(NULL, NULL, "<0 | 1>", "Whether allowlist should be enforced");
    allowlist_enable_args.end = arg_end(2);

    virtual_device_enable_args.enabled = arg_int1(NULL, NULL, "<0 | 1>", "Whether virtual devices are allowed");
    virtual_device_enable_args.end = arg_end(2);

    getprop_args.prop = arg_str1(NULL, NULL, "<property_name>", "Return property value");
    getprop_args.end = arg_end(2);

    const esp_console_cmd_t cmd_list_devices = {
        .command = "list_devices",
        .help = "List info about connected devices",
        .hint = NULL,
        .func = &list_devices,
    };

    const esp_console_cmd_t cmd_mouse_scale = {
        .command = "mouse_scale",
        .help =
            "Get/Set global mouse scale factor. Default: 1.0\n"
            "  Example: mouse_scale 0.5",
        .hint = NULL,
        .func = &mouse_scale,
        .argtable = &mouse_scale_args,
    };

    const esp_console_cmd_t cmd_gap_security_level = {
        .command = "gap_security_level",
        .help =
            "Get/Set GAP security level. Default: 2\n"
            "  Recommended values: 0, 1 or 2",
        .hint = NULL,
        .func = &set_gap_security_level,
        .argtable = &gap_security_level_args,
    };

    const esp_console_cmd_t cmd_gap_periodic_inquiry = {
        .command = "gap_periodic_inquiry",
        .help =
            "Get/Set GAP periodic inquiry mode. Default: 5 4 3.\n"
            "  Used for new connections / reconnections.\n"
            "  1 unit == 1.28 seconds\n"
            "  See Section 7.1.3 'Periodic Inquiry Mode Command' from Bluetooth spec",
        .hint = NULL,
        .func = &gap_periodic_inquiry,
        .argtable = &gap_periodic_inquiry_args,
    };

    const esp_console_cmd_t cmd_incoming_connections_enable = {
        .command = "incoming_connections_enable",
        .help = "Get/Set whether Bluetooth incoming connections are enabled",
        .hint = NULL,
        .func = &incoming_connections_enable,
        .argtable = &incoming_connections_enable_args,
    };

    const esp_console_cmd_t cmd_ble_enable = {
        .command = "ble_enable",
        .help = "Get/Set whether Bluetooth Low Energy (BLE) is enabled",
        .hint = NULL,
        .func = &ble_enable,
        .argtable = &ble_enable_args,
    };

    const esp_console_cmd_t cmd_list_bluetooth_keys = {
        .command = "list_bluetooth_keys",
        .help = "List stored Bluetooth keys",
        .hint = NULL,
        .func = &list_bluetooth_keys,
    };

    const esp_console_cmd_t cmd_del_bluetooth_keys = {
        .command = "del_bluetooth_keys",
        .help = "Delete stored Bluetooth keys. 'Unpairs' devices",
        .hint = NULL,
        .func = &del_bluetooth_keys,
    };

    const esp_console_cmd_t cmd_disconnect_device = {
        .command = "disconnect",
        .help = "Disconnects a gamepad/mouse/etc.",
        .hint = NULL,
        .func = &disconnect_device,
        .argtable = &disconnect_device_args,
    };

    const esp_console_cmd_t cmd_allowlist_list = {
        .command = "allowlist_list",
        .help = "List allowlist addresses",
        .hint = NULL,
        .func = &allowlist_list,
    };

    const esp_console_cmd_t cmd_allowlist_add = {
        .command = "allowlist_add",
        .help = "Add address to allowlist list",
        .hint = NULL,
        .func = &allowlist_add_addr,
        .argtable = &allowlist_addr_args,
    };

    const esp_console_cmd_t cmd_allowlist_remove = {
        .command = "allowlist_remove",
        .help = "Remove address from allowlist list",
        .hint = NULL,
        .func = &allowlist_remove_addr,
        .argtable = &allowlist_addr_args,
    };

    const esp_console_cmd_t cmd_allowlist_enable = {
        .command = "allowlist_enable",
        .help = "Enables/Disables allowlist addresses",
        .hint = NULL,
        .func = &allowlist_enable,
        .argtable = &allowlist_enable_args,
    };

    const esp_console_cmd_t cmd_virtual_device_enable = {
        .command = "virtual_device_enable",
        .help = "Enables/Disables virtual devices",
        .hint = NULL,
        .func = &virtual_device_enable,
        .argtable = &virtual_device_enable_args,
    };

    const esp_console_cmd_t cmd_getprop = {
        .command = "getprop",
        .help = "Get property or all properties",
        .hint = NULL,
        .func = &getprop,
        .argtable = &getprop_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_list_devices));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_disconnect_device));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_gap_security_level));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_gap_periodic_inquiry));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_list_bluetooth_keys));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_del_bluetooth_keys));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_incoming_connections_enable));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_ble_enable));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_allowlist_list));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_allowlist_add));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_allowlist_remove));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_allowlist_enable));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_mouse_scale));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_virtual_device_enable));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_getprop));
}
#endif  // CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE

void uni_console_init(void) {
#ifdef CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
    esp_console_repl_t* repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 80;  // CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;

#if CONFIG_CONSOLE_STORE_HISTORY
    initialize_filesystem();
    repl_config.history_save_path = HISTORY_PATH;
    ESP_LOGI(TAG, "Command history enabled");
#else
    ESP_LOGI(TAG, "Command history disabled");
#endif

    /* Register commands */
    esp_console_register_help_command();
    register_system_common();
#if CONFIG_BLUEPAD32_CONSOLE_NVS_COMMAND_ENABLE
    register_nvs();
#endif  // CONFIG_BLUEPAD32_CONSOLE_NVS_COMMAND_ENABLE

    register_bluepad32();
    uni_gpio_register_cmds();

    if (uni_get_platform()->register_console_cmds)
        uni_get_platform()->register_console_cmds();

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    // Should not happen
    // vTaskDelete(NULL);
#endif  // CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
}
