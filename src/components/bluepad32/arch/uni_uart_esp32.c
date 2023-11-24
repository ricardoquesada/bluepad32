// SPDX-License-Identifier: Apache-2.0
// Copyright 2019-2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_uart.h"

#include "sdkconfig.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#include <esp32/rom/uart.h>
#endif  // CONFIG_IDF_TARGET_ESP32

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/gpio_periph.h>
#include <soc/io_mux_reg.h>
#include <stddef.h>

#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <hci_dump.h>
#include <hci_dump_embedded_stdout.h>

#include "uni_config.h"

#ifdef UNI_ENABLE_BREDR
_Static_assert(CONFIG_BTDM_CTRL_BR_EDR_MAX_ACL_CONN >= 2, "Max ACL must be >= 2");
#endif  // UNI_ENABLE_BREDR

// Code taken from nina-fw
// https://github.com/adafruit/nina-fw/blob/master/main/sketch.ino.cpp
//
// Bluepad32 is compiled with UART RX/TX disabled by default.
// But can be enabled/disabled in runtime by calling this function.
void uni_uart_enable_output(bool enabled) {
#ifdef CONFIG_IDF_TARGET_ESP32
    if (enabled) {
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[1], 0);
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[3], 0);

        const char* default_uart_dev = "/dev/uart/0";
        _GLOBAL_REENT->_stdin = fopen(default_uart_dev, "r");
        _GLOBAL_REENT->_stdout = fopen(default_uart_dev, "w");
        _GLOBAL_REENT->_stderr = fopen(default_uart_dev, "w");

        uart_div_modify(CONFIG_ESP_CONSOLE_UART_NUM, (APB_CLK_FREQ << 4) / 115200);

        // uartAttach();
        ets_install_uart_printf();
        uart_tx_switch(CONFIG_ESP_CONSOLE_UART_NUM);
    } else {
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[1], PIN_FUNC_GPIO);
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[3], PIN_FUNC_GPIO);

        _GLOBAL_REENT->_stdin = (FILE*)&__sf_fake_stdin;
        _GLOBAL_REENT->_stdout = (FILE*)&__sf_fake_stdout;
        _GLOBAL_REENT->_stderr = (FILE*)&__sf_fake_stderr;

        ets_install_putc1(NULL);
        ets_install_putc2(NULL);
    }
#endif  // CONFIG_IDF_TARGET_ESP32
}

void uni_uart_init(void) {
#ifdef CONFIG_BLUEPAD32_UART_OUTPUT_ENABLE
    uni_uart_enable_output(1);
#else
    // Adafruit Airlift modules have the UART RX/TX (GPIO 1 / 3) wired with the
    // controller so they can't be used for logging. In fact they can generate
    // noise and can break the communication with the controller.
    uni_uart_enable_output(0);
#endif
}
