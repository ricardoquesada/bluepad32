# Add a custom platform

!!! Note

    This guide applies only to the low-level Pico SDK, ESP-IDF, and POSIX C
    APIs. Do **not** use this guide for Arduino projects.

Create a custom platform when you use the low-level ESP-IDF, Pico SDK, or POSIX
API. Bluepad32 uses `CONFIG_BLUEPAD32_PLATFORM_CUSTOM` by default.

Configure `main.c` to register your custom platform before you initialize
Bluepad32:

```c
// main.c
int app_main(void) {
    btstack_init();

    // Call uni_platform_set_custom() before uni_init()
    uni_platform_set_custom(get_my_platform());

    // Initialize Bluepad32
    uni_init(0 /* argc */, NULL /* argv */);

    // Start the BTstack run loop (does not return)
    btstack_run_loop_execute();

    return 0;
}
```

Implement the `uni_platform` callbacks in `my_platform.c`:

```c
// my_platform.c
#include <uni.h>

//
// Platform Overrides
//
static void my_platform_init(int argc, const char** argv) {
    // Runs when Bluepad32 starts initialization.
    // Bluetooth is not ready yet.
    logi("custom: my_platform_init()\n");
}

static void my_platform_on_init_complete(void) {
    // Runs when initialization finishes.
    // Safe to call Bluetooth APIs here.
    logi("custom: on_init_complete()\n");

    // Start scanning
    uni_bt_enable_new_connections_unsafe(true);
}

static void my_platform_on_device_connected(uni_hid_device_t* d) {
    // Runs when a controller connects, before it is ready.
    logi("custom: device connected: %p\n", d);
}

static void my_platform_on_device_disconnected(uni_hid_device_t* d) {
    // Runs when a controller disconnects.
    logi("custom: device disconnected: %p\n", d);
}

static uni_error_t my_platform_on_device_ready(uni_hid_device_t* d) {
    // Runs when the connected controller is ready for use.
    // Return UNI_ERROR_SUCCESS to accept the connection.
    // Otherwise return an error code such as UNI_ERROR_NO_SLOTS.

    logi("custom: device ready: %p\n", d);

    // Return UNI_ERROR_SUCCESS to accept the connection
    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    // Runs when new controller data arrives.
    // Parse and process the controller report.
    switch (ctl->klass) {
        case UNI_CONTROLLER_CLASS_GAMEPAD:
            // Process gamepad input
            uni_gamepad_dump(&ctl->gamepad);
            break;
        case UNI_CONTROLLER_CLASS_BALANCE_BOARD:
            // Process balance board input
            uni_balance_board_dump(&ctl->balance_board);
            break;
        case UNI_CONTROLLER_CLASS_MOUSE:
            // Process mouse input
            uni_mouse_dump(&ctl->mouse);
            break;
        case UNI_CONTROLLER_CLASS_KEYBOARD:
            // Process keyboard input
            uni_keyboard_dump(&ctl->keyboard);
            break;
        default:
            loge("Unsupported controller class: %d\n", ctl->klass);
            break;
    }
}

static const uni_property_t* my_platform_get_property(uni_property_idx_t idx) {
    // Key-value property lookup.
    // Return a property entry or NULL if unsupported.
    // See uni_property.h for supported properties.
    ARG_UNUSED(idx);
    return NULL;
}

static void my_platform_on_oob_event(uni_platform_oob_event_t event, void* data) {
    // Handles out-of-band events from Bluepad32:
    // UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON: User pressed the system button
    // UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:     Bluetooth scanning state changed
}

struct uni_platform* get_my_platform(void) {
    // Entry point
    static struct uni_platform plat = {
        .name = "custom",
        .init = my_platform_init,
        .on_init_complete = my_platform_on_init_complete,
        .on_device_connected = my_platform_on_device_connected,
        .on_device_disconnected = my_platform_on_device_disconnected,
        .on_device_ready = my_platform_on_device_ready,
        .on_oob_event = my_platform_on_oob_event,
        .on_controller_data = my_platform_on_controller_data,
        .get_property = my_platform_get_property,
    };

    return &plat;
}
```

See these complete platform implementations:

- [Pico SDK example][pico_sdk_example]
- [ESP-IDF example][esp_idf_example]
- [Posix example][posix_example]

[pico_sdk_example]: https://github.com/ricardoquesada/bluepad32/tree/develop/examples/pico_w

[esp_idf_example]: https://github.com/ricardoquesada/bluepad32/tree/develop/examples/esp32

[posix_example]: https://github.com/ricardoquesada/bluepad32/tree/develop/examples/posix
