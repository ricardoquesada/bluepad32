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

#ifndef UNI_PLATFORM_H
#define UNI_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "uni_hid_device.h"
#include "uni_joystick.h"

typedef enum {
    // The gamepad "System" button was pressed
    UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON,
    UNI_PLATFORM_OOB_BLUETOOTH_ENABLED,
} uni_platform_oob_event_t;

typedef enum {
    // Whether or not the Bluetooth stored keys should be deleted at boot time
    UNI_PLATFORM_PROPERTY_DELETE_STORED_KEYS,
} uni_platform_property_t;

// uni_platform must be defined for each new platform that is implemented.
// It contains callbacks and other init functions that each "platform" must
// implement.
// For example, in the case for the Unijosyticle2, it contains the GPIOs that
// must be enable/disable to emulate a C64 joystick.
struct uni_platform {
    // The name of the platform
    char* name;

    // Platform "callbacks".

    // init is called just once, just after boot time, and before Bluetooth
    // gets initialized.
    void (*init)(int argc, const char** argv);
    // on_init_complete is called when initialization finishes
    void (*on_init_complete)(void);

    // When a device (gamepad) connects. But probably it is not ready to use.
    // HID and/or other things might not have been parsed/init yet.
    void (*on_device_connected)(uni_hid_device_t* d);
    // When a device (gamepad) disconnects.
    void (*on_device_disconnected)(uni_hid_device_t* d);
    // When a device (gamepad) is ready to be used. Each platform can override
    // whether the device should be ready by returning a non-zero value.
    int (*on_device_ready)(uni_hid_device_t* d);

    // Indicates that a gamepad button and/or stick was pressed and/or released.
    // Deprecated. Use on_controller_data instead
    void (*on_gamepad_data)(uni_hid_device_t* d, uni_gamepad_t* gp);

    // Indicates that a controller button, stick, gyro, etc. has changed.
    void (*on_controller_data)(uni_hid_device_t* d, uni_controller_t* ctl);

    // Return -1 if property is not supported
    int32_t (*get_property)(uni_platform_property_t key);

    // Events that Bluepad32 sends to the platforms
    void (*on_oob_event)(uni_platform_oob_event_t event, void* data);

    // Print debug info about a device.
    void (*device_dump)(uni_hid_device_t* d);

    // Register console commands. Optional
    void (*register_console_cmds)(void);
};

void uni_platform_init(int argc, const char** argv);

struct uni_platform* uni_get_platform(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_PLATFORM_H
