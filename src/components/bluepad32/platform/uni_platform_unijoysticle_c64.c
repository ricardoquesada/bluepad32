// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

// Unijoysticle C64 platform
// Paddle code from:
// https://github.com/LeifBloomquist/JoystickEmulator/blob/master/Arduino/PaddleEmulator/PaddleEmulator.ino

#include "platform/uni_platform_unijoysticle_c64.h"

#include <stdbool.h>
#include <sys/cdefs.h>

#include <argtable3/argtable3.h>
#include <esp_console.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include "sdkconfig.h"

#include "platform/uni_platform_unijoysticle.h"
#include "uni_common.h"
#include "uni_gpio.h"
#include "uni_log.h"
#include "uni_property.h"

#define TASK_SYNC_IRQ_PRIO (9)
#define uS_MIN 3
#define uS_MAX 243  // Larger values cause the interrupt to take too long

// CPU where the Pot task runs
#define POT_TASK_CPU 1

enum {
    EVENT_SYNC_IRQ_0,
    EVENT_SYNC_IRQ_1,
};

// --- Function declaration
static int get_c64_pot_mode_from_nvs(void);

// GPIO Interrupt handlers
_Noreturn static void sync_irq_event_task(void* arg);

static volatile bool pot_x_leads = true;  // if false, y leads
static volatile uint16_t pot_x_delay_us = uS_MAX;
static volatile uint16_t pot_y_delay_us = uS_MAX;

// --- Consts (ROM)

// Unijoysticle v2 C64 / Flash Party edition
static const struct uni_platform_unijoysticle_gpio_config gpio_config_univ2c64 = {
    .port_a = {GPIO_NUM_26, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23, GPIO_NUM_14, GPIO_NUM_33, GPIO_NUM_16},
    .port_b = {GPIO_NUM_27, GPIO_NUM_25, GPIO_NUM_32, GPIO_NUM_17, GPIO_NUM_13, GPIO_NUM_21, GPIO_NUM_22},
    .leds = {GPIO_NUM_5, GPIO_NUM_12, GPIO_NUM_15},
    .push_buttons = {{
                         .gpio = GPIO_NUM_34,
                         .callback = uni_platform_unijoysticle_on_push_button_mode_pressed,
                     },
#ifdef CONFIG_BLUEPAD32_UNIJOYSTICLE_ENABLE_SWAP_FOR_C64
                     // Flash Party edition should have this disabled
                     {
                         .gpio = GPIO_NUM_35,
                         .callback = uni_platform_unijoysticle_on_push_button_swap_pressed,
                     }},
#else
                     {
                         .gpio = -1,
                         .callback = NULL,
                     }},
#endif
    .sync_irq = {GPIO_NUM_36, GPIO_NUM_39},
};

// Keep them in the order of the defines
static const char* c64_pot_modes[] = {
    "invalid",   // UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_INVALID,
    "3buttons",  // UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_3BUTTONS
    "5buttons",  // UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_5BUTTONS
    "rumble",    // UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_RUMBLE
};

// Globals to the file (RAM)
static EventGroupHandle_t _sync_irq_group;
static TaskHandle_t _sync_task;
uni_platform_unijoysticle_c64_pot_mode_t _pot_mode = UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_INVALID;

static struct {
    struct arg_str* value;
    struct arg_end* end;
} c64_pot_mode_args;

static btstack_context_callback_registration_t syncirq_callback_registration;

//
// Helpers
//

static void set_c64_pot_mode_to_nvs(int mode) {
    uni_property_value_t value;
    value.u8 = mode;

    uni_property_set(UNI_PROPERTY_IDX_UNI_C64_POT_MODE, value);
    logi("Done\n");
}

static int get_c64_pot_mode_from_nvs(void) {
    uni_property_value_t value;

    value = uni_property_get(UNI_PROPERTY_IDX_UNI_C64_POT_MODE);
    return value.u8;
}

static void enable_rumble_callback(void* context) {
    int seat = (int)context;
    uni_hid_device_t* d;

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        d = uni_hid_device_get_instance_for_idx(i);
        if (!uni_bt_conn_is_connected(&d->conn))
            continue;
        uni_platform_unijoysticle_instance_t* ins = uni_platform_unijoysticle_get_instance(d);
        // Use mask instead of == since Rumble should be active when the gamepad is in Twin Stick Mode.
        if ((ins->seat & seat) == 0)
            continue;
        if (d->report_parser.play_dual_rumble != NULL)
            d->report_parser.play_dual_rumble(d, 0 /* delayed start ms */, 30 /* duration ms */,
                                              0x80 /* weak magnitude */, 0x40 /* strong magnitude */);
    }
}

_Noreturn static void sync_irq_event_task(void* arg) {
    // timeout of 100s
    const TickType_t xTicksToWait = pdMS_TO_TICKS(100000);
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(_sync_irq_group, BIT(EVENT_SYNC_IRQ_0) | BIT(EVENT_SYNC_IRQ_1), pdTRUE,
                                               pdFALSE, xTicksToWait);

        // timeout ?
        if (bits == 0)
            continue;

        // EVENT_SYNC_IRQ_ events come from the C64.
        // They should be considered "hi" events.
        if (bits & BIT(EVENT_SYNC_IRQ_0)) {
            // gpio_set_level(g_gpio_config->leds[LED_J1], 1);
            syncirq_callback_registration.callback = &enable_rumble_callback;
            syncirq_callback_registration.context = (void*)(GAMEPAD_SEAT_A);
            btstack_run_loop_execute_on_main_thread(&syncirq_callback_registration);
        }

        if (bits & BIT(EVENT_SYNC_IRQ_1)) {
            // gpio_set_level(g_gpio_config->leds[LED_J2], 1);
            syncirq_callback_registration.callback = &enable_rumble_callback;
            syncirq_callback_registration.context = (void*)(GAMEPAD_SEAT_B);
            btstack_run_loop_execute_on_main_thread(&syncirq_callback_registration);
        }
    }
}

static IRAM_ATTR void gpio_isr_handler_sync(void* arg) {
    int sync_idx = (int)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xEventGroupSetBitsFromISR(_sync_irq_group, BIT(sync_idx), &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE)
        portYIELD_FROM_ISR();
}

inline void delay_us(uint32_t delay) {
#if ESP_IDF_VERSION_MAJOR == 4
    ets_delay_us(delay);
#else
    esp_rom_delay_us(delay);
#endif
}

static IRAM_ATTR void gpio_isr_handler_paddle(void* arg) {
    // From:
    // https://github.com/LeifBloomquist/JoystickEmulator/blob/master/Arduino/PaddleEmulator/PaddleEmulator.ino
    gpio_set_level(GPIO_NUM_16, 1);
    gpio_set_level(GPIO_NUM_33, 1);

    // Wait while the SID discharges the capacitor.
    // According to the spec, this should be 320 microseconds.
    // With function overhead and so on, this seems about right.
    delay_us(220);

    // Now, delay the amount required to represent the desired values.
    if (pot_x_leads) {
        delay_us(pot_x_delay_us);
        gpio_set_level(GPIO_NUM_16, 0);
        delay_us(pot_y_delay_us);  // This is a delta!
        gpio_set_level(GPIO_NUM_33, 0);
    } else {
        delay_us(pot_y_delay_us);
        gpio_set_level(GPIO_NUM_33, 0);
        delay_us(pot_x_delay_us);  // This is a delta!
        gpio_set_level(GPIO_NUM_16, 0);
    }
}

static void print_c64_pot_mode(void) {
    int mode = get_c64_pot_mode_from_nvs();

    if (mode >= UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_COUNT) {
        logi("Invalid C64 Pot mode: %d\n", mode);
        return;
    }

    logi("%s\n", c64_pot_modes[mode]);
}

static int cmd_c64_pot_mode(int argc, char** argv) {
    int nerrors = arg_parse(argc, argv, (void**)&c64_pot_mode_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, c64_pot_mode_args.end, argv[0]);

        // Don't treat as error, just print current value.
        print_c64_pot_mode();
        return 0;
    }

    int mode = 0;

    if (strcmp(c64_pot_mode_args.value->sval[0], "3buttons") == 0) {
        mode = UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_C64_POT_MODE_3BUTTONS;
    } else if (strcmp(c64_pot_mode_args.value->sval[0], "5buttons") == 0) {
        mode = UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_C64_POT_MODE_5BUTTONS;
    } else if (strcmp(c64_pot_mode_args.value->sval[0], "rumble") == 0) {
        mode = UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_C64_POT_MODE_RUMBLE;
    } else if (strcmp(c64_pot_mode_args.value->sval[0], "paddle") == 0) {
        mode = UNI_PLATFORM_UNIJOYSTICLE_CMD_SET_C64_POT_MODE_PADDLE;
    } else {
        loge("Invalid C64 Pot mode: : %s\n", c64_pot_mode_args.value->sval[0]);
        loge("Valid values: '3buttons', '5buttons', 'rumble' or 'paddle'\n");
        return 1;
    }

    uni_platform_unijoysticle_run_cmd(mode);
    return 0;
}

//
// Public
//
static void set_pot_mode_from_cpu(void* m) {
    // From ESP-IDF documentation:
    // "Register Timer interrupt handler, the handler is an ISR.
    // The handler will be attached to the same CPU core that this function is running on."

    // Change C64 Pot mode
    uni_platform_unijoysticle_c64_pot_mode_t mode = (uni_platform_unijoysticle_c64_pot_mode_t)m;

    if (_pot_mode == mode) {
        goto exit;
    }

    _pot_mode = mode;
    set_c64_pot_mode_to_nvs(mode);

    gpio_config_t io_conf = {0};

    if (mode == UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_3BUTTONS ||
        mode == UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_5BUTTONS) {
        if (_sync_task == NULL) {
            // Nothing to do. "rumble" was not initialized.
            goto exit;
            return;
        }
        for (int i = 0; i < UNI_PLATFORM_UNIJOYSTICLE_SYNC_IRQ_MAX; i++) {
            int sync_irq = gpio_config_univ2c64.sync_irq[i];
            if (sync_irq == -1)
                continue;
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            // GPIOs 34~39 don't have internal Pull-up resistors.
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pin_bit_mask = BIT64(sync_irq);
            ESP_ERROR_CHECK(gpio_config(&io_conf));

            // "i" must match EVENT_SYNC_IRQ_0, etc.
            gpio_isr_handler_remove(sync_irq);
        }
        vTaskDelete(_sync_task);
        _sync_task = NULL;

    } else if (mode == UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_RUMBLE) {
        if (_sync_task != NULL) {
            // Nothing to do. "rumble" / "paddle" already enabled.
            goto exit;
            return;
        }

        _sync_irq_group = xEventGroupCreate();
        xTaskCreatePinnedToCore(sync_irq_event_task, "bp.uni.sync_irq", 2048, NULL, TASK_SYNC_IRQ_PRIO, &_sync_task,
                                POT_TASK_CPU);

        // Sync IRQs
        for (int i = 0; i < UNI_PLATFORM_UNIJOYSTICLE_SYNC_IRQ_MAX; i++) {
            gpio_num_t gpio = gpio_config_univ2c64.sync_irq[i];
            if (gpio == -1)
                continue;

            // Set Interrupt handler
            io_conf.intr_type = GPIO_INTR_NEGEDGE;
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            // GPIOs 34~39 don't have internal Pull-up resistors.
            io_conf.pull_up_en = (gpio < GPIO_NUM_34) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
            io_conf.pin_bit_mask = BIT64(gpio);
            ESP_ERROR_CHECK(gpio_config(&io_conf));
            // "i" must match EVENT_SYNC_IRQ_0, etc.
            ESP_ERROR_CHECK(gpio_isr_handler_add(gpio, gpio_isr_handler_sync, (void*)i));
        }
    } else if (mode == UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_PADDLE) {
        // Sync IRQs
        for (int i = 0; i < 1; i++) {
            gpio_num_t gpio = gpio_config_univ2c64.sync_irq[i];
            if (gpio == -1)
                continue;

            // Set Interrupt handler
            io_conf.intr_type = GPIO_INTR_NEGEDGE;
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            // GPIOs 34~39 don't have internal Pull-up resistors.
            io_conf.pull_up_en = (gpio < GPIO_NUM_34) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
            io_conf.pin_bit_mask = BIT64(gpio);
            ESP_ERROR_CHECK(gpio_config(&io_conf));
            // "i" must match EVENT_SYNC_IRQ_0, etc.
            ESP_ERROR_CHECK(gpio_isr_handler_add(gpio, gpio_isr_handler_paddle, (void*)i));
        }
    } else {
        loge("unijoysticle: unsupported gamepad mode: %d\n", mode);
    }

exit:
    // Kill itself
    vTaskDelete(NULL);
}

void uni_platform_unijoysticle_c64_set_pot_mode(uni_platform_unijoysticle_c64_pot_mode_t mode) {
    xTaskCreatePinnedToCore(set_pot_mode_from_cpu, "bp.uni.init_pot", 4096, (void*)mode, TASK_SYNC_IRQ_PRIO, NULL,
                            POT_TASK_CPU);
}

void uni_platform_unijoysticle_c64_version(void) {
    logi("\tPot mode: %s\n", c64_pot_modes[get_c64_pot_mode_from_nvs()]);
}

static void process_5button(uni_hid_device_t* d, uni_gamepad_seat_t seat, uint8_t misc_buttons) {
    if (misc_buttons & MISC_BUTTON_SELECT) {
        if (seat & GAMEPAD_SEAT_A) {
            uni_gpio_set_level(gpio_config_univ2c64.port_a[UNI_PLATFORM_UNIJOYSTICLE_JOY_UP], 1);
            uni_gpio_set_level(gpio_config_univ2c64.port_a[UNI_PLATFORM_UNIJOYSTICLE_JOY_DOWN], 1);
        }
        if (seat & GAMEPAD_SEAT_B) {
            uni_gpio_set_level(gpio_config_univ2c64.port_b[UNI_PLATFORM_UNIJOYSTICLE_JOY_UP], 1);
            uni_gpio_set_level(gpio_config_univ2c64.port_b[UNI_PLATFORM_UNIJOYSTICLE_JOY_DOWN], 1);
        }
    }

    // "Start" buttons
    if (misc_buttons & MISC_BUTTON_START) {
        if (seat & GAMEPAD_SEAT_A) {
            uni_gpio_set_level(gpio_config_univ2c64.port_a[UNI_PLATFORM_UNIJOYSTICLE_JOY_LEFT], 1);
            uni_gpio_set_level(gpio_config_univ2c64.port_a[UNI_PLATFORM_UNIJOYSTICLE_JOY_RIGHT], 1);
        }
        if (seat & GAMEPAD_SEAT_B) {
            uni_gpio_set_level(gpio_config_univ2c64.port_b[UNI_PLATFORM_UNIJOYSTICLE_JOY_LEFT], 1);
            uni_gpio_set_level(gpio_config_univ2c64.port_b[UNI_PLATFORM_UNIJOYSTICLE_JOY_RIGHT], 1);
        }
    }
}

static void process_paddle(uni_hid_device_t* d, uni_gamepad_seat_t seat, uint8_t misc_buttons) {
#if 0
    // Disabled

    // A 1:1 mapping actually works pretty well.  If tweaking/scaling is required, do it here.
    int delay_x = (1024 - gp->brake) / 4;
    int delay_y = (1024 - gp->throttle) / 4;

    // Now the trick to keep everything in one interrupt.  Use the other delay as a delta!
    if (delay_x < delay_y) {
        pot_x_leads = true;
        delay_y -= delay_x;
    } else {
        pot_x_leads = false;
        delay_x -= delay_y;
    }

    if (delay_x > uS_MAX)
        delay_x = uS_MAX;
    else if (delay_x < uS_MIN)
        delay_x = uS_MIN;

    if (delay_y > uS_MAX)
        delay_y = uS_MAX;
    else if (delay_y < uS_MIN)
        delay_y = uS_MIN;

    pot_x_delay_us = delay_x;
    pot_y_delay_us = delay_y;
#endif
}

//
// Variant overrides
//

static void set_gpio_level_for_pot_c64(gpio_num_t gpio_num, bool level) {
    if (_pot_mode == UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_3BUTTONS ||
        _pot_mode == UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_5BUTTONS) {
        // C64 uses pull-ups for Pot-x, Pot-y, so the value needs to be "inverted" to be off.
        uni_gpio_set_level(gpio_num, !level);
    } else if (_pot_mode == UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_RUMBLE) {
        // Leave it disabled to allow the SYNC to reach ESP32 without interference.
        uni_gpio_set_level(gpio_num, level);
    } else if (_pot_mode == UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_PADDLE) {
        // do nothing
    } else {
        loge("unijoysticle: unsupported pot_level mode: %d\n", _pot_mode);
    }
}

static void on_init_complete_c64(void) {
    int mode = get_c64_pot_mode_from_nvs();
    uni_platform_unijoysticle_c64_set_pot_mode(mode);

    // set_pot_mode runs from a different Core, so, wait until _pot_mode is defined
    // before calling set_pot_level, which requires in pot_mod
    while (_pot_mode == UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_INVALID)
        vTaskDelay(pdMS_TO_TICKS(50));

    set_gpio_level_for_pot_c64(gpio_config_univ2c64.port_a[UNI_PLATFORM_UNIJOYSTICLE_JOY_BUTTON2], 0);
    set_gpio_level_for_pot_c64(gpio_config_univ2c64.port_a[UNI_PLATFORM_UNIJOYSTICLE_JOY_BUTTON3], 0);

    set_gpio_level_for_pot_c64(gpio_config_univ2c64.port_b[UNI_PLATFORM_UNIJOYSTICLE_JOY_BUTTON2], 0);
    set_gpio_level_for_pot_c64(gpio_config_univ2c64.port_b[UNI_PLATFORM_UNIJOYSTICLE_JOY_BUTTON3], 0);
}

void register_console_cmds_c64(void) {
    c64_pot_mode_args.value =
        arg_str1(NULL, NULL, "<mode>", "valid options: '3buttons', '5buttons', 'rumble' or 'paddle'");
    c64_pot_mode_args.end = arg_end(2);

    const esp_console_cmd_t c64_pot_mode = {
        .command = "c64_pot_mode",
        .help =
            "Get/Set C64 Pot mode.\n"
            "  Default: 3buttons",
        .hint = NULL,
        .func = &cmd_c64_pot_mode,
        .argtable = &c64_pot_mode_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&c64_pot_mode));
}

static bool process_gamepad_misc_buttons_c64(uni_hid_device_t* d, uni_gamepad_seat_t seat, uint8_t misc_buttons) {
    int mode = get_c64_pot_mode_from_nvs();

    // Return true only if "select" and/or "start" where processed.
    if (mode == UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_5BUTTONS) {
        process_5button(d, seat, misc_buttons);
        return true;
    }

    if (mode == UNI_PLATFORM_UNIJOYSTICLE_C64_POT_MODE_PADDLE) {
        process_paddle(d, seat, misc_buttons);
    }

    return false;
}

const struct uni_platform_unijoysticle_variant* uni_platform_unijoysticle_c64_create_variant(void) {
    const static struct uni_platform_unijoysticle_variant variant = {
        .name = "2 C64",
        .gpio_config = &gpio_config_univ2c64,
        .flags = 0, /* Quadrant mouse not supported*/
        .supported_modes =
            UNI_PLATFORM_UNIJOYSTICLE_GAMEPAD_MODE_NORMAL | UNI_PLATFORM_UNIJOYSTICLE_GAMEPAD_MODE_TWINSTICK,
        .on_init_complete = on_init_complete_c64,
        .register_console_cmds = register_console_cmds_c64,
        .process_gamepad_misc_buttons = process_gamepad_misc_buttons_c64,
        .set_gpio_level_for_pot = set_gpio_level_for_pot_c64,
        .preferred_seat_for_mouse = GAMEPAD_SEAT_A,
        .preferred_seat_for_joystick = GAMEPAD_SEAT_B,
    };

    return &variant;
}