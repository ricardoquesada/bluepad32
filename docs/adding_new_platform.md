# Adding a new platform

Adding a new platform is easy:

1. create your `src/main/uni_platform_yourplatform.c` file
2. add you `src/configs/yourplatform-config.mk` makefile config file
3. edit `src/main/uni_platform.c` file
4. add documentation in `docs/plat_yourplatform.md`

## 1. Platform file

Use the existing platform as an exmaple:

* [uni_platform_airlift.c]

What you need to do is to implement the callbacks:

```c
//
// AirLift platform entry point
//
struct uni_platform* uni_platform_airlift_create(void) {
  static struct uni_platform plat;

  plat.name = "Adafruit AirLift";
  plat.init = airlift_init;
  plat.on_init_complete = airlift_on_init_complete;
  plat.on_device_connected = airlift_on_device_connected;
  plat.on_device_disconnected = airlift_on_device_disconnected;
  plat.on_device_ready = airlift_on_device_ready;
  plat.on_device_oob_event = airlift_on_device_oob_event;
  plat.on_gamepad_data = airlift_on_gamepad_data;
  plat.get_property = airlift_get_property;

  return &plat;
}

```

Bluepad32 only uses one core of the ESP32 (CPU0). The remaining one (CPU1) is
free to use. As an example, AirLift uses it to read from SPI. See [uni_platform_airlift.c] for details.

[uni_platform_airlift.c]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/src/main/uni_platform_airlift.c

## 2. Add a makefile config file

Again, use existing code as example:

* [airlift-config.mk]

The most important thing here is to have a "define" for your platform. E.g:

```
CFLAGS += -DUNI_PLATFORM_YOURPLATFORM
```

[airlift-config.mk]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/src/configs/airlift-config.mk

## 3. Edit uni_platform.c

Finally in [uni_platform.c] add support for your platform. E.g:

```c
void uni_platform_init(int argc, const char** argv) {
  // Only one for the moment. Each vendor must create its own.
  // These UNI_PLATFORM_ defines are defined in the Makefile and CMakeLists.txt

#ifdef UNI_PLATFORM_UNIJOYSTICLE
  _platform = uni_platform_unijoysticle_create();
#elif defined(UNI_PLATFORM_PC_DEBUG)
  _platform = uni_platform_pc_debug_create();
#elif defined(UNI_PLATFORM_AIRLIFT)
  _platform = uni_platform_airlift_create();
#elif defined(UNI_PLATFORM_YOURPLATFORM)
 // Here goes your code
  _platform = uni_platform_yourplatform_create();
#endif
}

```

[uni_platform.c]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/src/main/uni_platform.c


## 4. Add documentation

Add a `docs/plat_yourplatform.md` file that, at least, describes:

* How to compile it
* Where to find more info about your platform.

Use the following file as reference:

* [plat_airlift.md]

[plat_airlift.md]:  https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/docs/plat_airlift.md