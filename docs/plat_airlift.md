# Bluepad32 for AirLift

## What is AirLift

AirLift is a family of [ESP32 modules][airlift-esp32] created by [Adafruit][adafruit].
These modules are co-processors, usually used only to bring WiFi or BLE to the main processor.

The AirLift module and the main processor talk to each other using the SPI protocol.
AirLift modules come with the [official nina-fw firmware][nina-fw].

Bluepad32 replaces the [official ESP32 firmware][nina-fw] that comes with AirLift
modules. Bluepad32 is "compatible-enough" with Nina-fw:

* Uses SPI, and the same GPIOs to talk to the main processor
* Uses the same Nina-fw protocol that runs on top of SPI
* But not all Nina-fw messages are implemented. Only the ones that are needed
  to have gamepad support working.

[adafruit]: https://www.adafruit.com
[airlift-esp32]: https://www.adafruit.com/product/4201

## Getting Bluepad32 for Airlift firwmare

### 1. Get a pre-compiled firmware

You can grab a precompiled firmware from here (choose latest version):

* https://github.com/ricardoquesada/bluepad32/tags

And download the `bluepad32-airlift.zip`. It includes a README with instructions.

### 2. Or compile it yourself

Read [README.md][readme] for the ESP-IDF requirements. And choose `airlift` as
the target platform:

```sh
$ export PLATFORM=airlift

# And then compile it

$ make -j
```

[readme]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/README.md

## Install Bluepad32 for AirLift

In theory, flashing Bluepad32 is as easy as flahsing any firmware to any ESP32 module.
The problem is that AirLift modules were designed to be minimal, and not all
GPIOs are exposed, and might not be possible to flash the firmware directly using
`make flash`.

It depends from AirLift module to AirLift module. But to give a concrete example,
let's see how to install it on a [MatrixPortal M4][matrix_portal_m4]
(which has an AirLift module):

* Put MatrixPortal M4 in "boot" mode
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

## CircuitPython example

The Bluepad32 library for CircuitPython, including a working example can be found here:

* https://gitlab.com/ricardoquesada/bluepad32-circuitpython


## How to debug Bluepad32 for AirLift

Assuming that the ESP32 UART pins are not exposed (true for all AirLift modules),
the recommended way to debug the Bluepad32 firmware is:

* Get any ESP32 breakout module. It could be an [Adafruit HUZZAH32][esp32-adafruit] or [any other ESP32 breakout][amazon-esp32].
  * JUST DON'T GET A ESP32-S2 (doesn't have Bluetooth!). JUST "ESP32"
* Get a SAMD51 module, like the [Adafruit Feather M4 Express][feather_m4]
* Wire it like this:

|       | ESP32 | SAMD51 |
|-------|-------|--------|
| MOSI  | 14    | MOSI   |
| MISO  | 23    | MISO   |
| SCK   | 18    | SCK    |
| CS    | 5     | D10    |
| READY | 33    | D9     |
| RESET | EN    | D6     |

Something like this (left: Feather M4 express, right: HUZZAH32)

![fritzing](https://lh3.googleusercontent.com/pw/ACtC-3fNxNMUdaoBg7DGB6OPPDDnu_DQ15fmJS_I3crWjFKg7k3DA4HDeI8I_SUicSFamGuIVsHpM-myo5h-v1YOOFUU7lz6mU5tyExXDWZXedaYbUxhgf-GXfeZhMCdJCt1nZ04zFb1nyH86-pvZqc8yG9Y4A=-no)

NOTE: If you use a HUZZAH32, pay attention to the GPIO labels. The MOSI/MISO/SCK lables that are in the front of board, are not the ones that you should use. See the back of the board for the correct GPIO numbers.

In real life it might look more messy:

![wiring](https://lh3.googleusercontent.com/pw/ACtC-3dutrQXEj9I5zicNFW3K3PBbfge7MdwgB8dyi-wPSrtSp8zku3Y4c9WtBqQ9Bfa92xOjgSkZncAuzAZyc5F392tFkzkqWUl4YkfrKrM4e8TGP-B_7I7G_fRvFbIYbEQQIi-LlOnPU5SdGYYeW6hxxpJ_w=-no)

The benefits of using two separate modules (SAMD51 + ESP32) are:

* You can see the console of both the ESP32 and SAM51 at the same time
* Faster flashing: it is > 2x faster to flash Bluepad32 directly to the ESP32 than
  to flash it with "PassThrough" method required for AirLift modules.
* If needed, you can inspect the SPI protocol with a logic analizer

[esp32-adafruit]: https://www.adafruit.com/product/4172?gclid=EAIaIQobChMI-eeixraV7QIVED2tBh2qywzJEAQYASABEgLsTfD_BwE
[amazon-esp32]: https://www.amazon.com/s?k=esp32+module+breakout
[feather_m4]: https://www.adafruit.com/product/3857
