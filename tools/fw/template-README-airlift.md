# Bluepad32 firmware for AirLift

## How to flash the firmware

### Download `esptool.py`

- Linux: `sudo apt install esptool`
- macOS: `brew install esptool`
- Windows and other OSs: `pip install esptool`
- or download Source Code: https://github.com/espressif/esptool

### Enable "Passthrough" AirLift module

Might slightly vary from board to board, but basically what you have to do is:

1. Put the board in "boot" mode, usually by double pressing the "reset" button.
2. Flash the right "Passthrough" firmware for your board.
   - Details here: [Adafruit's Upgrade AirLift firmware][adafruit-airlift-upgrade]

[adafruit-airlift-upgrade]: https://learn.adafruit.com/upgrading-esp32-firmware/upgrade-all-in-one-esp32-airlift-firmware

### Flash the firmware

```sh
# Linux
$ export ESPPORT=/dev/ttyACM0
# macOS
$ export ESPPORT=/dev/cu.usbmodem14121301
# Windows
$ export ESPPORT=COM??  #??? Try different ones

$ esptool.py --port ${ESPPORT} --baud 115200 --before no_reset write_flash 0x0000 bluepad32-airlift-full.bin
```

## Further info

- [Bluepad32 for AirLift documentation][bluepad32-airlift]
- [Upgrade External ESP32 AirLift firmware][adafruit-esp32]
- [Nina-W10 firmware][nina-fw]

[bluepad32-airlift]: https://bluepad32.readthedocs.io/en/latest/plat_airlift/
[adafruit-esp32]: https://learn.adafruit.com/adafruit-airlift-breakout/upgrade-external-esp32-airlift-firmware
[nina-fw]: https://github.com/adafruit/nina-fw
