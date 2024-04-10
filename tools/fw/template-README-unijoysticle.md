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

### GUI (ESP32 Flash Download Tool)

![flash_tool](https://lh3.googleusercontent.com/pw/ACtC-3c6KvmSei83mYKogxIadcq7tWamg41jsNk7pqJOpjnPhNoeN3uYjehB94wAja72mIDRNrhrWIqG0Sle1gxZHr0gANCSJyDFUcSfXMdoetUTynure2UrjRv7WkZEYnj0nqpiYJ54mwj85jDLkFrnD4jd-g=-no)

Supported platforms: Windows only

If you are not familiar with command-line tools, you can try with the ESP32 Flash Download Tool:

- Download: <https://www.espressif.com/en/support/download/other-tools>

Parameters:

- If asked about "chipType", select "ESP32"
- Select "SPIDownload" tab
- Click on "..." and select the `bluepad32-unijoysticle-full.bin` file
- Set `0x0000` as address
- Select the correct "COM" port
- And then click "START"
