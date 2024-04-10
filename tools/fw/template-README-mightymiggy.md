# Firmware setup

## Flashing precompiled firmware

If you only want to flash the latest firmware version without downloading the toolchain + sources you should do:

Download the latest precompiled firmware from here:

- https://github.com/ricardoquesada/bluepad32/releases

And then...

### Command Line (esptool)

![esptool](https://lh3.googleusercontent.com/UfYRw0D2m6DUy337fskfNYP6FA3oj_AgATe6QU3y5OvGe14DaI5amCb-rhmGliSepoFYmhvX-u5uzq5N0wChP0lr0eSOrY4YMLB__UBZ8tY8ASbw5DgI6dUX-oEt2ZpWHPLpnBdxryA=-no)

For Command Line tool, download `esptool.py`:

- Source Code: https://github.com/espressif/esptool
- macOS: `brew install esptool`
- Debian: `sudo apt install esptool`
- Windows and other OSs: `pip install esptool`

And:

```sh
# macOS
$ export ESPPORT=/dev/cu.SLAB_USBtoUART
# Linux
$ export ESPPORT=/dev/ttyUSB0
# Windows
$ export ESPPORT=COM??  #??? Try different ones

$ esptool.py --port ${ESPPORT} --baud 115200 --before default_reset --after hard_reset write_flash 0x0000 bluepad32-unijoysticle-full.bin
```

### GUI (ESP32 Flash Tool)

![flash_tool](https://lh3.googleusercontent.com/eO0uXc9kZHw2W1_UGiP9mw5QuzgD9gc0dIotrSUhIZW1cTVcfNIyi6grTNnSX5OryS0Bjs8hQ5PQtdg-fnxykzby5elywNT1rZ8ddtlRcTPdeJ9fS11eqrHP3TRecCHqHl9TdecncTE=-no)

If you are not familiar with command-line tools, you can try with the ESP32 Flash Tool (Windows only):

- Download: https://www.espressif.com/en/products/hardware/esp32/resources

A tutorial that explains how to use and from where to download is here:

- Tutorial: [ESP32 Flash Tool tutorial](http://iot-bits.com/esp32/esp32-flash-download-tool-tutorial/)
