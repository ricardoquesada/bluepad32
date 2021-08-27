# Flashing precompiled firmware

If you only want to flash the latest firmware version without downloading the toolchain + sources you should do:

Download latest precompiled firmware from here:

- https://github.com/ricardoquesada/bluepad32/releases

And then...

## Command Line (esptool)

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

## GUI (ESP32 Flash Tool)

![flash_tool](https://lh3.googleusercontent.com/pw/ACtC-3c6KvmSei83mYKogxIadcq7tWamg41jsNk7pqJOpjnPhNoeN3uYjehB94wAja72mIDRNrhrWIqG0Sle1gxZHr0gANCSJyDFUcSfXMdoetUTynure2UrjRv7WkZEYnj0nqpiYJ54mwj85jDLkFrnD4jd-g=-no)

If you are not familiar with command-line tools, you can try with the ESP32 Flash Tool (Windows only):

- Download: https://www.espressif.com/en/products/hardware/esp32/resources

A tutorial that explains how to use and from where to download is here:

- Tutorial: [ESP32 Flash Tool tutorial](http://iot-bits.com/esp32/esp32-flash-download-tool-tutorial/)

# Compiling + flashing firmware

**Linux**: Supported. Keep reading.

**macOS**: The Linux instructions should work, but not %100 sure.

**Windows**:
NOT supported. Install [Linux Subsystem for Windows](https://docs.microsoft.com/en-us/windows/wsl/install-win10) and do the compilation/flashing from Linux.


## 1. Download

### Download Bluepad32

```sh
$ git clone --recursive https://gitlab.com/ricardoquesada/bluepad32.git
```

### Download ESP-IDF

Make sure to check latest info from here: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html

```sh
# Needs to be done just once
# 1. Clone the ESP-IDF git repo
$ mkdir -p ~/esp
$ cd ~/esp
$ git clone -b v4.2 --recursive https://github.com/espressif/esp-idf.git

# 2. And then install the toolchain
$ cd ~/esp/esp-idf
$ ./install.sh
```

```sh
# Needs to be done every time you open a new terminal and want to compile Bluepad32
$ source ~/esp/esp-idf/export.sh
```

### Optional: Download libusb

Only if you target Linux as a device (not a ESP32 device):
```sh
$ sudo apt install libusb-1.0.0-dev
```

## 2. Compile

### Integrate BTstack for ESP32

```sh
$ cd bluepad32/external/btstack/port/esp32
$ ./integrate_btstack.py
```

### Optional: Compile BTStack for Linux as target

```sh
$ cd bluepad32/external/btstack/port/libusb
$ make
```

### Compile Bluepad32

...and finally compile Bluepad32 :)

#### Bluepad32 for ESP32 as target

```sh
$ cd bluepad32/src
# Choose target platform: unijoysticle, airlift, etc.
$ export PLATFORM=unijoysticle
# Choose the correct port. Might vary.
$ export ESPPORT=/dev/ttyUSB0
$ make -j
$ make flash monitor
```

#### Optional: Bluepad32 for Linux as target

```sh
# Optional: Only if you are targeting Linux as a device
$ cd bluepad32/tools/pc_debug
$ make -j
$ sudo ./bluepad32
```

Put the gamepad in discovery mode. The gamepad should be recognized and when you press buttons, you should see them on the console.

[1]: https://www.aliexpress.com/item/MH-ET-LIVE-ESP32-MINI-KIT-WiFi-Bluetooth-Internet-of-Things-development-board-based-ESP8266-Fully/32819107932.html
[2]: https://wiki.wemos.cc/products:d1:d1_mini
[3]: https://github.com/bluekitchen/btstack/blob/master/port/esp32/README.md
