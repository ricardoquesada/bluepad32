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
#include <stdlib.h>

#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>

#include "sdkconfig.h"
#include "uni_init.h"
#include "uni_platform.h"

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
