# Bluepad32 firmware for NINA

## How to flash the firmware

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
$ esptool.py --port ${ESPPORT} --baud 115200 --before no_reset write_flash 0x0000 bluepad32-nina-full.bin
```

## Further info

- [Bluepad32 for NINA documentation][bluepad32-nina]
- [Nina-W10 firmware][nina-fw]

[bluepad32-nina]: https://gitlab.com/ricardoquesada/bluepad32/blob/master/docs/plat_nina.md
[nina-fw]: https://github.com/arduino/nina-fw
