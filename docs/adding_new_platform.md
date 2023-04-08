# Adding a new platform

Adding a new platform is easy:

1. Create your `src/components/bluepad32/uni_platform_yourplatform.c` file
2. Update `src/components/bluepad32/Kconfig` file
3. Update`src/components/bluepad32/uni_platform.c` file
4. Add documentation in `docs/plat_yourplatform.md`

## 1. Platform file

Use the existing platform as an exmaple:

* [uni_platform_nina.c]

What you need to do is to implement the callbacks:

```c
//
// NINA platform entry point
//
struct uni_platform* uni_platform_nina_create(void) {
    static struct uni_platform plat;

    plat.name = "Arduino NINA";
    plat.init = nina_init;
    plat.on_init_complete = nina_on_init_complete;
    plat.on_device_connected = nina_on_device_connected;
    plat.on_device_disconnected = nina_on_device_disconnected;
    plat.on_device_ready = nina_on_device_ready;
    plat.on_device_oob_event = nina_on_device_oob_event;
    plat.on_gamepad_data = nina_on_gamepad_data;
    plat.get_property = nina_get_property;

    return &plat;
}
```

Bluepad32 only uses one core of the ESP32 (CPU0). The remaining one (CPU1) is
free to use. As an example, NINA uses it to read from SPI. See [uni_platform_nina.c] for details.

[uni_platform_nina.c]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/main/src/components/bluepad32/uni_platform_nina.c

## 2. Update Kconfig file

Update [src/components/bluepad32/Kconfig] file, and add your platform there. Just copy & paste another platform and update it accordingly.

It should look something like the following:

```
    config BLUEPAD32_PLATFORM_YOURPLATFORM
        bool "YourPlatform"
        help
            Targets ESP32 devices that ... (complete the sentence).
```

For further info about Kconfig, see: [Kconfig language][kconfig_doc]

Once it is updated, you can select your platform by doing:

```
idf.py menuconfig
```

And then select `Component config` -> `Bluepad32` -> `Target platform`


[src/components/bluepad32/Kconfig]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/main/src/components/bluepad32/Kconfig
[kconfig_doc]: https://www.kernel.org/doc/html/latest/kbuild/kconfig-language.html


## 3. Edit uni_platform.c

Finally in [uni_platform.c] add support for your platform. E.g:

```c
void uni_platform_init(int argc, const char** argv) {
#ifdef CONFIG_BLUEPAD32_PLATFORM_UNIJOYSTICLE
    _platform = uni_platform_unijoysticle_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_PC_DEBUG)
    _platform = uni_platform_pc_debug_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_AIRLIFT)
    _platform = uni_platform_airlift_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_MIGHTYMIGGY)
    _platform = uni_platform_mightymiggy_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_NINA)
    _platform = uni_platform_nina_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_ARDUINO)
    _platform = uni_platform_arduino_create();
#elif defined(CONFIG_BLUEPAD32_PLATFORM_YOURPLATFORM)
    // Here goes your code
    _platform = uni_platform_yourplatform_create();
#else
#error "Platform not defined. Set PLATFORM environment variable"
#endif
}

```

[uni_platform.c]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/src/main/uni_platform.c


## 5. Add documentation

Add a `docs/plat_yourplatform.md` file that, at least, describes:

* How to compile it
* Where to find more info about your platform.

Use the following file as reference:

* [plat_nina.md]

[plat_nina.md]:  https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/docs/plat_nina.md
