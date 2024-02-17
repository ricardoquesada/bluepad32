# Bluepad32 firmware for AirLift

## What is AirLift

AirLift is an [ESP32][esp32] module. This module is a "co-processor",
usually used to bring WiFi or BLE to the main processor.

AirLift modules are present in some [Adafruit][adafruit] boards, like in:

* [MatrixPortal M4][matrixportal-m4]
* [PyPortal][pyportal]
* [PyBadge][pybadge]
* [Metro M4 Express AirLift][metro-m4-airlift]
* [Metro M7 1011 with Airlift][metro-m7-airlift]

Or it can be a standalone board:

* [AirLift module][airlift-module]

AirLift modules come pre-installed with [Adafruit's NINA firmware][nina-fw].

**NOTE**: [Adafruit's NINA][nina-fw] firmware is a fork of [Arduino's NINA][arduino-nina] firmware.
They are pretty similar, but they are not compatible since they use different SPI pins.

To have gamepad support, the original AirLift firmware must be replaced
with Bluepad32 firmware. This is a simple step that needs to be done just once
and can be "undone" at any time.

![how-does-it-work](images/bluepad32-airlift-how-does-it-work.png)

This is how it works:

* Gamepad (A) talks to AirLift module (B)
* AirLift module (B) talks to the main processor (C)

Bluepad32 firmware is "compatible-enough" with the original firmware:

* Uses SPI, and the same GPIOs to talk to the main processor
* Uses the same protocol that runs on top of SPI
* But not all messages are implemented. Only the ones that are needed
  to have gamepad support working.

[adafruit]: https://www.adafruit.com

[airlift-module]: https://www.adafruit.com/product/4201

[arduino-nina]: https://github.com/arduino/nina-fw

[esp32]: https://www.espressif.com/en/products/socs/esp32

[matrixportal-m4]: https://www.adafruit.com/product/4745

[metro-m4-airlift]: https://www.adafruit.com/product/4000

[metro-m7-airlift]: https://www.adafruit.com/product/4950

[nina-fw]: https://github.com/adafruit/nina-fw

[pybadge]: https://www.adafruit.com/product/4200

[pyportal]: https://www.adafruit.com/product/4116

## Flashing Bluepad32 firmware

To flash Bluepad32 firmware, you have to:

1. Put the Adafruit board in "pass-through" mode
2. Flash pre-compiled version
3. Or compile it yourself and flash it.

### 1. Put Adafruit board in "passthrough" mode

It might slightly vary from board to board, but basically what you have to do is:

1. Put the board in "boot" mode, usually by double pressing the "reset" button.
2. Flash the right "Passthrough" firmware for your board.
    * Details here: [Adafruit's Upgrade AirLift firmware][adafruit-airlift-upgrade]

[adafruit-airlift-upgrade]: https://learn.adafruit.com/upgrading-esp32-firmware/upgrade-all-in-one-esp32-airlift-firmware

### 2. Flash pre-compiled version

Download the latest pre-compiled version from here:

* <https://github.com/ricardoquesada/bluepad32/tags>

Unzip it, and follow the instructions described in the `README.md` file.

### 3. Or compile it yourself

Install the requirements described here: [README.md][readme].

Chose `airlift` as the target platform:

```sh
cd {BLUEPAD32}/src/

# Select AirLift platform:
# Components config -> Bluepad32 -> Target Platform -> AirLift
idf.py menuconfig

# And then compile it!
idf.py build
```

### 4. Flash it using esptool

To flash it, you have to use the `--before no_reset` option:

```sh
# Flash it!

# Port might be different
export ESPPORT=/dev/ttyACM0

esptool.py --port ${ESPPORT} --baud 115200 --before no_reset write_flash 0x1000 ./build/bootloader/bootloader.bin 0x10000 ./build/bluepad32-airlift.bin 0x8000 ./build/partitions_singleapp.bin
```

### 5. (OPTIONAL) Flash it using CircuitPython code

If step 4a) failed, you can try by using this CircuitPython code.

Copy & paste this file in your `CIRCUITPYTHON` folder with the name `code.py`.
And also copy the `bluepad32-airlift-full-xxx.bin` to the `CIRCUITPYTHON` folder.

```python
import time
import board
import busio
from digitalio import DigitalInOut, Direction
import adafruit_miniesptool

print("ESP32 Bluepad32-FW")

# Override these if you are manually wiring. Otherwise, this will use ESP pins from board.
tx = getattr(board, "ESP_TX", board.TX)
rx = getattr(board, "ESP_RX", board.RX)
resetpin = getattr(board, "ESP_RESET", board.D12)
gpio0pin = getattr(board, "ESP_GPIO0", board.D10)

uart = busio.UART(rx, tx, baudrate=115200, timeout=1)

esptool = adafruit_miniesptool.miniesptool(
    uart, DigitalInOut(gpio0pin), DigitalInOut(resetpin), flashsize=4 * 1024 * 1024
)
esptool.sync()

print("Synced")
print("Found:", esptool.chip_name)
if esptool.chip_name != "ESP32":
    raise RuntimeError("This example is for ESP32 only")

esptool.baudrate = 115200 * 4  # speed up so we can wrap it in under 2 minutes

# Test connection again by asking for the MAC address
print("MAC ADDR: ", [hex(i) for i in esptool.mac_addr])

# Note: Make sure to use the LATEST nina-fw binary release! update the md5sum as well
# esptool.flash_file("NINA_W102-1.7.4.bin", 0x0, "80c2dfd8ad2e97b2c32899382860acb1")
esptool.flash_file("bluepad32-airlift-full-v3.7.1.bin", 0x0, "a8ae34c6d7cc7f08e0cafc468e66e0af")

esptool.reset()
time.sleep(0.5)
```

[readme]: https://github.com/ricardoquesada/bluepad32/blob/main/README.md

[matrix_portal_m4]: https://learn.adafruit.com/adafruit-matrixportal-m4

[passthrough firmware]: https://learn.adafruit.com/adafruit-airlift-breakout/upgrade-external-esp32-airlift-firmware

## CircuitPython example

The Bluepad32 library for CircuitPython, including a working example, is available here:

* <https://github.com/ricardoquesada/bluepad32-circuitpython>

## How to debug Bluepad32 for AirLift

**ADVANCED**: Normally you wouldn't need this.

Assuming that the ESP32 UART pins are not exposed (true for all AirLift modules),
the recommended way to debug the Bluepad32 firmware is:

* Get any ESP32 breakout module. It could be an [Adafruit HUZZAH32][esp32-adafruit]
  or [any other ESP32 breakout][amazon-esp32].
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

NOTE: If you use a HUZZAH32, pay attention to the GPIO labels. The MOSI/MISO/SCK lables that are in the front of board
are not the ones that you should use. See the back of the board for the correct GPIO numbers.

In real life, it might look more messy:

![wiring](https://lh3.googleusercontent.com/pw/ACtC-3dutrQXEj9I5zicNFW3K3PBbfge7MdwgB8dyi-wPSrtSp8zku3Y4c9WtBqQ9Bfa92xOjgSkZncAuzAZyc5F392tFkzkqWUl4YkfrKrM4e8TGP-B_7I7G_fRvFbIYbEQQIi-LlOnPU5SdGYYeW6hxxpJ_w=-no)

The benefits of using two separate modules (SAMD51 + ESP32) are:

* You can see the console of both the ESP32 and SAM51 at the same time
* Faster flashing: it is > 2x faster to flash Bluepad32 directly to the ESP32 than
  to flash it with "PassThrough" method required for AirLift modules.
* If needed, you can inspect the SPI protocol with a logic analyzer

[esp32-adafruit]: https://www.adafruit.com/product/4172?gclid=EAIaIQobChMI-eeixraV7QIVED2tBh2qywzJEAQYASABEgLsTfD_BwE

[amazon-esp32]: https://www.amazon.com/s?k=esp32+module+breakout

[feather_m4]: https://www.adafruit.com/product/3857
