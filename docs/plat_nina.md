# Bluepad32 firmware for NINA

## What is NINA

NINA is a family of [ESP32 modules][nina-esp32].
These modules are present on some Arduino boards like:

- Arduino Nano RP2040 Connect
- Arduino Nano 33 IoT
- Arduino MKR WiFi 1010

NINA modules are co-processors, usually used only to bring WiFi or BLE to the main processor.

NINA and the main processor talk to each other using the SPI protocol using the [official NINA firmware][nina-fw].

Bluepad32 replaces the the official firmware, which is "compatible enough" with the official one:

- Uses SPI, and the same GPIOs to talk to the main processor
- Uses the same Nina-fw protocol that runs on top of SPI
- But not all Nina-fw messages are implemented. Only the ones that are needed
  to have gamepad support working. For example WiFi is not available.

[nina-esp32]: https://www.u-blox.com/en/product/nina-w10-series-open-cpu
[nina-fw]: https://github.com/arduino/nina-fw

## Flashing Bluepad32 firmware

To flash Bluepad32 firmware, you have to:

1. Put the Arduino board in "pass-through" mode
2. Flash pre-compiled version
3. Or compile it yourself and flash it.

### 1. Put Arduino board in "passthrough" mode

Before flash Bluepad32 firmware, you have to put the Arduino board in "pass-through" mode:

1. Open Arduino IDE
2. Install the WiFiNINA library (just do it once)
3. And finally open the `SerialNINAPassthrough` sketch:

- File -> Examples -> WiFiNINA -> Tools -> SerialNINAPassthrough

Compile it and flash it to the Arduino board.

### 2. Flash a pre-compiled firmware

You can grab a precompiled firmware from here (choose latest version):

* https://gitlab.com/ricardoquesada/bluepad32/tags

And download the `bluepad32-nina-xxx.zip` file.

Unzip it, and follow the instructions described in the `README.md` file.

### 3. Or compile it yourself and flash it

Make sure you have installed the requirements described here: [README.md][readme].

Chose `nina` as the target platform:

```sh
$ cd ${BLUEPAD32}/src

# Set correct platform
$ export PLATFORM=nina

# And then compile it!
$ make -j
```

On Nano 32 IoT / MKR WIFI 1010, doing `make flash` will just work.

```sh
# Only valid for:
#   * the Nano 33 IoT
#   * MKR WIFI 1010

# Port might be different
$ export ESPPORT=/dev/ttyACM0

$ make flash
```

But on NANO RP2040 Connect, you have to flash it using the `--before no_reset` option,
and **NOT** `--before default_reset`. E.g:

```sh
# Only valid for:
#   * Nano RP2040 Connect

# Port might be different
$ export ESPPORT=/dev/ttyACM0

$ esptool.py --port ${ESPPORT} --baud 115200 --before no_reset write_flash 0x1000 ./build/bootloader/bootloader.bin 0x10000 ./build/bluepad32-airlift.bin 0x8000 ./build/partitions_singleapp.bin
```

[readme]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/README.md

## Example

The Bluepad32 library for Arduino with examples is available here:

* http://gitlab.com/ricardoquesada/bluepad32-arduino
