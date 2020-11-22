# Bluepad32 for AirLift

## What is AirLift

AirLift is a family of [ESP32 modules][airlift-esp32] created by [Adafruit][adafruit].
These modules are co-processors, usually used only to bring WiFi or BLE to the main processor.

The AirLift module and the main processor talk to each other using the SPI protocol.
AirLift modules come with the [official nina-fw firmware][nina-fw].

Bluepad32 replaces the [official ESP32 firmware][nina-fw] that comes with AirLift
modules. Bluepad32 is compatible-enough with Nina-fw.

* Uses SPI, and the same GPIOs to talk to the main processor
* Uses the some Nina-fw protocol that runs on top of SPI
* But not all Nina-fw messages are implemented. Only the ones that are needed
  to have gamepad support working.

[adafruit]: https://www.adafruit.com
[airlift-esp32]: https://www.adafruit.com/product/4201
[nina-fw]: https://github.com/adafruit/nina-fw

## Compile Bluepad32

Read [README.md][readme] for the ESP-IDF requirements, with this change:

```sh
$ export PLATFORM=airlift

# And then compile it

$ make -j
```

[readme]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/README.md

## Install Bluepad32

In theory, flashing Bluepad32 is as easy as flahsing any firmware to any ESP32 module.
The problem is that AirLift modules were designed to be minimal, and not all
GPIOs are exposed, and might not be possible to flash the firmware directly using
`make flash`.

It depends from AirLift module to AirLift module. But to give a concrete example,
let's see how to install it on a [MatrixPortal M4][matrix_portal_m4] (which has an AirLift module):

* Put MatrixPortal M4 in "boot" mode.
* Flash the MatrixPortal-M4 [passthrough firmware]

And only now, you can do `make flash`, with just one exception: by default
`make flash` will try to reset ESP32, but it just won't work. Just use
`--before no_reset`. E.g:

```sh
# Double check port. Might be different on differente OSs
$ esptool.py --port /dev/ttyACM0 --baud 115200 --before no_reset write_flash 0x1000 ./build/bootloader/bootloader.bin 0x10000 ./build/bluepad32-airlift.bin 0x8000 ./build/partitions_singleapp.bin
```

[matrix_portal_m4]: https://learn.adafruit.com/adafruit-matrixportal-m4
[passthrough firmware]: https://learn.adafruit.com/adafruit-airlift-breakout/upgrade-external-esp32-airlift-firmware

## How to debug Bluepad32

Assuming that the ESP32 UART pins are not exposed (like in the majority of the
AirLift modules), the recommended way to debug the Bluepad32 firmware is:

* Get any ESP32 breakout module. It could be an [Adafruit HUZZAH32][esp32-adafruit] or [any other ESP32 breakout][amazon-esp32].
  * JUST DON'T GET A ESP32-S2 (doesn't have Bluetooth!). JUST "ESP32"
* Get SAMD51 only module, like the [Adafruit Feather M4 Express][feather_m4]
* Wire it like this:

|       | ESP32 | SAMD51 |
|-------|-------|--------|
| MOSI  | 14    | MOSI   |
| MISO  | 23    | MISO   |
| SCK   | 18    | SCK    |
| CS    | 5     | D10    |
| READY | 33    | D9     |
| RESET | EN    | D6     |

Something like this:

![wiring](https://lh3.googleusercontent.com/pw/ACtC-3dutrQXEj9I5zicNFW3K3PBbfge7MdwgB8dyi-wPSrtSp8zku3Y4c9WtBqQ9Bfa92xOjgSkZncAuzAZyc5F392tFkzkqWUl4YkfrKrM4e8TGP-B_7I7G_fRvFbIYbEQQIi-LlOnPU5SdGYYeW6hxxpJ_w=-no)


Pros:

* Can see the console of both the ESP32 and SAM51 at the same time
* Can use Logic Analizer to inspect SPI (in case it is needed)


[esp32-adafruit]: https://www.adafruit.com/product/4172?gclid=EAIaIQobChMI-eeixraV7QIVED2tBh2qywzJEAQYASABEgLsTfD_BwE
[amazon-esp32]: https://www.amazon.com/s?k=esp32+module+breakout
[feather_m4]: https://www.adafruit.com/product/3857

## Testing it

### Patching adafruit_esp32spi

### Example

## FAQ

**Does Bluepad only run on AirLift modules?**

In general, **Bluepad32** can run on any ESP32 module and it is very easy to
adapt it to your own needs.

But in particular, **Bluepad32 for AirLift** can be used without custom changes
in any module that runs the Nina-fw firmware.
