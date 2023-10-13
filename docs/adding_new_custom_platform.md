# Adding a new custom platform

Adding a new custom platform is easy:

1. Ensure that the file `uni_platform_custom.c` is compiled and linked with your project, an example implementation can be found under `rsc/main`
1. Create a new platform file such as `src/main/uni_platform_custom_2.c` file
2. Update `src/main/Kconfig` file
3. Update`src/main/uni_platform_custom.c` file so that the newly created platform can be selected in the function uni_platform_custom_create()

## 1. Custom platform file

Use the existing platform as an exmaple:

* [src/main/uni_platform_custom_1.c]

What you need to do is to implement the callbacks:

```c
struct uni_platform* uni_platform_custom_2_create(void) {
    static struct uni_platform plat;

    plat.name = "Custom-2";
    plat.init = custom_2_init;
    plat.on_init_complete = custom_2_on_init_complete;
    plat.on_device_connected = custom_2_on_device_connected;
    plat.on_device_disconnected = custom_2_on_device_disconnected;
    plat.on_device_ready = custom_2_on_device_ready;
    plat.on_device_oob_event = custom_2_on_device_oob_event;
    plat.on_gamepad_data = custom_2_on_gamepad_data;
    plat.get_property = custom_2_get_property;

    return &plat;
}
```

Bluepad32 only uses one core of the ESP32 (CPU0). The remaining one (CPU1) is
free to use. As an example, NINA uses it to read from SPI. See [uni_platform_nina.c] for details.

[uni_platform_nina.c]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/main/src/components/bluepad32/uni_platform_nina.c

## 2. Update Kconfig file

Update [src/main/Kconfig] file, and add your custom platform there. Just copy & paste another platform and update it accordingly.

It should look something like the following:

```
    config BLUEPAD32_PLATFORM_CUSTOM_2
        bool "Custom-2"
        help
            Targets ESP32 devices that ... (complete the sentence).
```

For further info about Kconfig, see: [Kconfig language][kconfig_doc]

Once it is updated, you can select your platform by doing:

```
idf.py menuconfig
```

And then select `Component config` -> `Bluepad32 Custom Platform` -> `Target platform`


[src/main/Kconfig]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/main/src/main/Kconfig
[kconfig_doc]: https://www.kernel.org/doc/html/latest/kbuild/kconfig-language.html


## 3. Edit uni_platform_custom.c

Finally in [uni_platform_custom.c] add support for your platform. E.g:

```c
void uni_platform_custom_create() {
#ifdef CONFIG_BLUEPAD32_PLATFORM_CUSTOM_1
    uni_platform_custom_1_create();
#elif defined(CONFIG_BLUEPAD_PLATFORM_CUSTOM_2)
	unit_platform_custom_2_create();
#else
#error "Custom Platform not defined."
#endif
}
```

[uni_platform_custom.c]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/src/main/uni_platform_custom.c
