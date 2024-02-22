# Adding your own platform

!!! Note

    Only for users that want to use Pico SDK / ESP-IDF / Posix raw API directly. 
    E.g.: **Not** for Arduino users.

You will need to add your own platform when using the raw (low-level) ESP-IDF / Pico SDK API.

By default, Bluepad32 is configured to use the `CONFIG_BLUEPAD32_PLATFORM_CUSTOM`,
and that's the one that you need to use.

You need to configure your "custom platform".

You `main.c` file should look like this.

```c
// main.c
int app_main(void) {
    btstack_init();

    // Must be called before uni_init()
    uni_platform_set_custom(get_my_platform());

    // Init Bluepad32.
    uni_init(0 /* argc */, NULL /* argv */);

    // Does not return.
    btstack_run_loop_execute();

    return 0;
}
```

And `my_platform.c` should look like this:

```c
// my_platform.c
#include <uni.h>

//
// Platform Overrides
//
static void my_platform_init(int argc, const char** argv) {
    // Called when the starts the initialization.
    // Bluetooth is not ready yet.
    logi("custom: my_platform_init()\n");
}

static void my_platform_on_init_complete(void) {
    // Called when initiazation is complete.
    // Safe to make Bluetooth calls.
    logi("custom: on_init_complete()\n");

    // Start scanning
    uni_bt_enable_new_connections_unsafe(true);
}

static void my_platform_on_device_connected(uni_hid_device_t* d) {
    // Called when a controller is connected. But the controller is not ready yet.
    logi("custom: device connected: %p\n", d);
}

static void my_platform_on_device_disconnected(uni_hid_device_t* d) {
    // Called when a controller disconnected.
    logi("custom: device disconnected: %p\n", d);
}

static uni_error_t my_platform_on_device_ready(uni_hid_device_t* d) {
    // Called when the controller that recently connected, is ready to use.
    // If you want to "accept it", just return UNI_ERROR_SUCCESS.
    // Otherwise return any error, like UNI_ERROR_NO_SLOTS.

    logi("custom: device ready: %p\n", d);

    // Return UNI_ERROR_SUCESS if you accept the connection
    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    // Called when the controller has data.
    // You have to parse it and do something with it.
    switch (ctl->klass) {
        case UNI_CONTROLLER_CLASS_GAMEPAD:
            // Do something
            uni_gamepad_dump(&ctl->gamepad);
            break;
        case UNI_CONTROLLER_CLASS_BALANCE_BOARD:
            // Do something
            uni_balance_board_dump(&ctl->balance_board);
            break;
        case UNI_CONTROLLER_CLASS_MOUSE:
            // Do something
            uni_mouse_dump(&ctl->mouse);
            break;
        case UNI_CONTROLLER_CLASS_KEYBOARD:
            // Do something
            uni_keyboard_dump(&ctl->keyboard);
            break;
        default:
            loge("Unsupported controller class: %d\n", ctl->klass);
            break;
    }
}

static const uni_property_t* my_platform_get_property(uni_property_idx_t idx) {
    // This is sort of Key/Value storage.
    // Return a property entry, or NULL if not supported.
    // The supported properties are defined in uni_property.h
    ARG_UNUSED(idx);
    return NULL;
}

static void my_platform_on_oob_event(uni_platform_oob_event_t event, void* data) {
    // Events that Bluepad32 sends to the platforms
    // At the moment, the supported events are:
    // UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON: When the gamepad "system" button was pressed
    // UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:     When Bluetooth is "scanning"
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

Real world examples:

- [Pico SDK example][pico_sdk_example]
- [ESP-IDF example][esp_idf_example]
- [Posix example][posix_example]

[pico_sdk_example]: https://github.com/ricardoquesada/bluepad32/tree/develop/examples/pico_w

[esp_idf_example]: https://github.com/ricardoquesada/bluepad32/tree/develop/examples/esp32

[posix_example]: https://github.com/ricardoquesada/bluepad32/tree/develop/examples/posix
