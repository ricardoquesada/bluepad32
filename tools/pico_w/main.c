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

#include <btstack_run_loop.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>

#include "sdkconfig.h"
#include "uni_log.h"
#include "uni_main.h"
#include "uni_platform.h"

// Sanity check
#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#error "Pico W must use BLUEPAD32_PLATFORM_CUSTOM"
#endif

// Defined in my_platform.c
struct uni_platform* get_my_platform(void);

int main() {
    stdio_init_all();

    // initialize CYW43 driver architecture (will enable BT if/because CYW43_ENABLE_BLUETOOTH == 1)
    if (cyw43_arch_init()) {
        loge("failed to initialise cyw43_arch\n");
        return -1;
    }

    // Turn-on LED. Turn it off once init is done.
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    // Must be called before uni_main()
    uni_platform_set_custom(get_my_platform());

    // Initialize BP32
    uni_main(0, NULL);

    // Does not return.
    btstack_run_loop_execute();

    return 0;
}
