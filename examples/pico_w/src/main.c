// Example file - Public Domain
// Need help? http://bit.ly/bluepad32-help

#include <btstack_run_loop.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <uni.h>

#include "sdkconfig.h"

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

    // Must be called before uni_init()
    uni_platform_set_custom(get_my_platform());

    // Initialize BP32
    uni_init(0, NULL);

    // Does not return.
    btstack_run_loop_execute();

    return 0;
}
