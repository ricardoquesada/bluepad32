# Bluepad32 firmware for NINA

To flash Bluepad32 in NINA, there are two options

* Using `arduino-fwuploader` (recommended)
* or Using `esptool.py`

## Flash using arduino-fwuploader

### Download arduino-fwuploader

Download the latest binary from here: https://github.com/arduino/arduino-fwuploader/releases

### Select correct board name

* `arduino:samd:mkrwifi1010` for Arduino MKR WiFi 1010
* `arduino:samd:nano_33_iot` for Arduino NANO 33 IoT
* `arduino:samd:mkrvidor4000` for Arduino MKR Vidor 4000
* `arduino:megaavr:uno2018` for Arduino Uno WiFi Rev2
* `arduino:mbed_nano:nanorp2040connect` for Arduino Nano RP2040 Connect

You can see all boards names by doing:
```shell
$ arduino-fwuploader firmware list
```

### Flash it

```shell
# Replace name and address with the correct ones
export BOARD=arduino:samd:nano_33_iot
export ADDRESS=/dev/ttyACM0
$ arduino-fwuploader firmware flash -b $BOARD -a $ADDRESS -i bluepad32-nina-full.bin
```

### Verify

To verify that the flash was successful, do:

```shell
$ arduino-fwuploader firmware get-version -b $BOARD -a $ADDRESS
```

And you should see:

```
...

Firmware version installed: Bluepad32 for NINA v3.6.0-rc0
```

## or Flash using esptool.py

### Download `esptool.py`

- Linux: `sudo apt install esptool`
- macOS: `brew install esptool`
- Windows and other OSs: `pip install esptool`
- or download Source Code: https://github.com/espressif/esptool

### Flash SerialNINAPassthrough sketch

Open Arduino IDE, make sure `WiFiNINA` library is installed,
and open the `SerialNINAPassthrough` sketch:

- File -> Examples -> WiFiNINA -> Tools -> SerialNINAPassthrough

Compile it and flash it.

### Flash the firmware

```sh
# Linux
$ export ESPPORT=/dev/ttyACM0
# macOS
$ export ESPPORT=/dev/cu.usbmodem14121301
# Windows
$ export ESPPORT=COM??  #??? Try different ones

# Only valid for:
#   * the Nano 33 IoT
#   * MKR WIFI 1010
$ esptool.py --port ${ESPPORT} --baud 115200 --before default_reset write_flash 0x0000 bluepad32-nina-full.bin

# Only valid for:
#   * Nano RP2040 Connect
#   * UNO WiFi Rev.2
$ esptool.py --port ${ESPPORT} --baud 115200 --before no_reset write_flash 0x0000 bluepad32-nina-full.bin
```

## Further info

- [Bluepad32 for NINA documentation][bluepad32-nina]
- [Nina-W10 firmware][nina-fw]

[bluepad32-nina]: https://bluepad32.readthedocs.io/en/latest/plat_nina/
[nina-fw]: https://github.com/arduino/nina-fw
