// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_PLATFORM_H
#define UNI_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "uni_error.h"
#include "uni_hid_device.h"
#include "uni_joystick.h"
#include "uni_property.h"

typedef enum {
    UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON,  // When the gamepad "system" button was pressed
    UNI_PLATFORM_OOB_BLUETOOTH_ENABLED,      // When Bluetooth is "scanning"
} uni_platform_oob_event_t;

// uni_platform must be defined for each new platform that is implemented.
// It contains callbacks and other init functions that each "platform" must
// implement.
struct uni_platform {
    // The name of the platform
    const char* name;

    // Platform "callbacks".

    // init is called just once, just after boot time, and before Bluetooth
    // gets initialized.
    void (*init)(int argc, const char** argv);

    // on_init_complete is called when initialization finishes
    void (*on_init_complete)(void);

    // A new device has been discovered while scanning.
    // To tell Bluepad32 that connection should be established, return UNI_ERROR_SUCCESS.
    // @param addr: the Bluetooth address
    // @param name: could be NULL, could be zero-length, or might contain the name.
    // @param cod: Class of Device. See "uni_bt_defines.h" for possible values.
    // @param rssi: Received Signal Strength Indicator (RSSI) measured in dBms. The higher (255) the better.
    // @return UNI_ERROR_SUCCESS if Bluepad32 should try to connect to it. Otherwise Bluepad32 will ignore it.
    uni_error_t (*on_device_discovered)(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi);

    // When a device (controller) connects. But probably it is not ready to use.
    // HID and/or other things might not have been parsed/init yet.
    void (*on_device_connected)(uni_hid_device_t* d);

    // When a device (controller) disconnects.
    void (*on_device_disconnected)(uni_hid_device_t* d);

    // When a device (controller) is ready to be used.
    // Platform can accept/reject the connection.
    // To accept it return UNI_ERROR_SUCCESS.
    uni_error_t (*on_device_ready)(uni_hid_device_t* d);

    // Indicates that a gamepad button and/or stick was pressed and/or released.
    // Deprecated. Use on_controller_data instead
    void (*on_gamepad_data)(uni_hid_device_t* d, uni_gamepad_t* gp);

    // Indicates that a controller button, stick, gyro, etc. has changed.
    void (*on_controller_data)(uni_hid_device_t* d, uni_controller_t* ctl);

    // Return a property entry, or NULL if not supported.
    const uni_property_t* (*get_property)(uni_property_idx_t idx);

    // Events that Bluepad32 sends to the platforms
    void (*on_oob_event)(uni_platform_oob_event_t event, void* data);

    // Print debug info about a device.
    void (*device_dump)(uni_hid_device_t* d);

    // Register console commands. Optional
    void (*register_console_cmds)(void);
};

void uni_platform_init(int argc, const char** argv);

struct uni_platform* uni_get_platform(void);

void uni_platform_set_custom(struct uni_platform* platform);

#ifdef __cplusplus
}
#endif

#endif  // UNI_PLATFORM_H
