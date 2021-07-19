# Bluepad32 for NINA

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

## Getting Bluepad32 for NINA firwmare

### 1. Get a pre-compiled firmware

You can grab a precompiled firmware from here (choose latest version):

* https://gitlab.com/ricardoquesada/bluepad32/tags

And download the `bluepad32-nina.zip`. It includes a README with instructions.

### 2. Or compile it yourself

Read [README.md][readme] for the ESP-IDF requirements. And choose `nina` as
the target platform:

```sh
$ export PLATFORM=nina

# And then compile it

$ make -j
```

[readme]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/README.md

## Install Bluepad32 for NINA

### Arduino board in "passthrough" mode

Open Arduino IDE, and open the `SerialNINAPassthrough` sketch:

- File -> Examples -> WiFiNINA -> Tools -> SerialNINAPassthrough

Compile it and flash it to the Arduino board.

### Flash Bluepad32 to NINA module

```sh
# Linux
$ export ESPPORT=/dev/ttyACM0
# macOS
$ export ESPPORT=/dev/cu.usbmodem14121301
# Windows
$ export ESPPORT=COM??  #??? Try different ones

$ esptool.py --port ${ESPPORT} --baud 115200 --before no_reset write_flash 0x0000 bluepad32-nina-full.bin
```

And only now, you can do `make flash`, with just one exception: by default
`make flash` will try to reset ESP32, but it just won't work. Just use
`--before no_reset`. E.g:

```sh
# Double check port. Might be different on differente OSs
$ esptool.py --port /dev/ttyACM0 --baud 115200 --before no_reset write_flash 0x1000 ./build/bootloader/bootloader.bin 0x10000 ./build/bluepad32-airlift.bin 0x8000 ./build/partitions_singleapp.bin
```

## Example

The Bluepad32 library for Arduino with examples is available here:

* http://gitlab.com/ricardoquesada/bluepad32-arduino
