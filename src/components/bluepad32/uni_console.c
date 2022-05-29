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

#include "uni_console.h"

#include <argtable3/argtable3.h>
#include <cmd_nvs.h>
#include <cmd_system.h>
#include <esp_console.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "uni_debug.h"
#include "uni_hid_device.h"
#include "uni_mouse_quadrature.h"

#ifdef CONFIG_ESP_CONSOLE_USB_CDC
#error This example is incompatible with USB CDC console. Please try "console_usb" example instead.
#endif  // CONFIG_ESP_CONSOLE_USB_CDC

static const char* TAG = "console";
#define PROMPT_STR "bp32"

static struct {
    struct arg_str* value;
    struct arg_end* end;
} mouse_set_args;

static void initialize_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static int devices_info(int argc, char** argv) {
    /* FIXME: Should be called from btstack thread */
    uni_hid_device_dump_all();
    return 0;
}

static int mouse_get(int argc, char** argv) {
    char buf[32];
    float scale = uni_mouse_quadrature_get_scale_factor();

    // ets_printf() doesn't support "%f"
    sprintf(buf, "mouse scale = %f\n", scale);
    logi(buf);
    return 0;
}

static int mouse_set(int argc, char** argv) {
    float scale;

    int nerrors = arg_parse(argc, argv, (void**)&mouse_set_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mouse_set_args.end, argv[0]);
        return 1;
    }

    const char* value = mouse_set_args.value->sval[0];
    scale = atof(value);
    uni_mouse_quadrature_set_scale_factor(scale);
    return 0;
}

static void register_bluepad32() {
    mouse_set_args.value = arg_str1(NULL, NULL, "<value>", "a float that represents the mouse scale factor");
    mouse_set_args.end = arg_end(2);

    const esp_console_cmd_t cmd_devices = {
        .command = "devices",
        .help = "Get information about connected devices",
        .hint = NULL,
        .func = &devices_info,
    };

    const esp_console_cmd_t cmd_mouse_get = {
        .command = "mouse_scale_get",
        .help = "Get mouse scale factor",
        .hint = NULL,
        .func = &mouse_get,
    };

    const esp_console_cmd_t cmd_mouse_set = {
        .command = "mouse_scale_set",
        .help =
            "Set mouse scale factor. Bigger numbers mean slower movement\n"
            "Example:\n"
            " mouse_scale_set 1.52",
        .hint = NULL,
        .func = &mouse_set,
        .argtable = &mouse_set_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_devices));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_mouse_get));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_mouse_set));
}

void uni_console_init() {
    esp_console_repl_t* repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 80;  // CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;

    initialize_nvs();

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
    register_nvs();
    register_bluepad32();

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    // Should not happen
    // vTaskDelete(NULL);
}