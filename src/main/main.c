// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include <stdlib.h>

#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>

#include <uni.h>

#include "sdkconfig.h"

#ifndef CONFIG_BLUEPAD32_PLATFORM_ARDUINO

// Not needed in Arduino platform since it has "autostart" code defined in
// uni_platform_arduino.

// Defined in my_platform.c
struct uni_platform* get_my_platform(void);

int app_main(void) {
    // hci_dump_open(NULL, HCI_DUMP_STDOUT);

    // Configure BTstack for ESP32 VHCI Controller
    btstack_init();

    // hci_dump_init(hci_dump_embedded_stdout_get_instance());

    // Init Bluepad32.
    uni_init(0 /* argc */, NULL /* argv */);

    // Does not return.
    btstack_run_loop_execute();

    return 0;
}

#endif  //  CONFIG_BLUEPAD32_PLATFORM_ARDUINO
