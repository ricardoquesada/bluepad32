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

#include "uni_esp32.h"

#include "sdkconfig.h"

#include <esp32/rom/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/gpio_periph.h>
#include <soc/io_mux_reg.h>
#include <stddef.h>

_Static_assert(CONFIG_BTDM_CTRL_BR_EDR_MAX_ACL_CONN >= 2, "Max ACL must be >= 2");

// Code taken from nina-fw
// https://github.com/adafruit/nina-fw/blob/master/main/sketch.ino.cpp
//
// Bluepad32 is compiled with UART RX/TX disabled by default.
// But can be enabled/disabled in runtime by calling this function.
void uni_esp32_enable_uart_output(int enabled) {
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
}
