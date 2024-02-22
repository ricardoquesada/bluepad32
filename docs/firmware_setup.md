# Flashing precompiled firmware

!!! Note

    Only valid for Arduino NINA, Adafruit AirLift, Unijoysticle and MightyMiggy boards.

If you only want to flash the latest firmware version without downloading the toolchain + sources you should do:

Download the latest precompiled firmware from here:

- <https://github.com/ricardoquesada/bluepad32/releases>

And then you can flash it either using:

- Command Line
- or GUI

## Command Line (esptool)

![esptool](https://lh3.googleusercontent.com/UfYRw0D2m6DUy337fskfNYP6FA3oj_AgATe6QU3y5OvGe14DaI5amCb-rhmGliSepoFYmhvX-u5uzq5N0wChP0lr0eSOrY4YMLB__UBZ8tY8ASbw5DgI6dUX-oEt2ZpWHPLpnBdxryA=-no)

Supported platforms: Linux, macOS and Windows

Flashing your device from the command line
requires [`esptool.py`](https://docs.espressif.com/projects/esptool/en/latest/esp32/) ([source code](https://github.com/espressif/esptool)):

- Ubuntu / Debian: `sudo apt install esptool`
- Fedora / Red Hat: `sudo dnf install esptool`
- macOS with [Homebrew](https://brew.sh/) installed: `brew install esptool`
- Windows, macOS without Homebrew, and other OSes: after installing Python, run `pip install esptool`. Python
  is [available for download here](https://www.python.org/downloads/). On Windows 10 and newer, you can
  also [install Python from the Microsoft Store](https://www.microsoft.com/store/productId/9PJPW5LDXLZ5) to get
  automatic updates.

To flash your device:

``` sh
# Linux
export ESPPORT=/dev/ttyUSB0 # This may be different if you have multiple USB serial devices connected.
# macOS
export ESPPORT=/dev/cu.SLAB_USBtoUART
# Windows
set ESPPORT=COM??  # You can find a list of COM devices in Device Manager.
                   # Identify the correct COM port by watching to see which one appears when connecting
                   # the device you intend to update.

python -m esptool --port ${ESPPORT} --baud 115200 --before default_reset --after hard_reset write_flash 0x0000 bluepad32-unijoysticle-full.bin
```

On Linux, the flash operation may fail with a permissions error if you're not running as root. If you'd like to be able
to flash as a standard user, you can instead change the permissions for the appropriate ttyUSB device to allow writes by
unprivileged users.

## GUI (ESP32 Flash Download Tool)

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

# Compiling + flashing firmware

!!! Note

    Although Bluepad32 works both with ESP-IDF v4.4 and 5.x, Unijoysticle, NINA and MightyMiggy require ESP-IDF v4.4.

## For Windows

1. Install [ESP-IDF v4.4][esp-idf-windows-installer]. For further info,
   read: [ESP-IDF Getting Started for Windows][esp-idf-windows-setup]

    * Either the Online or Offline version should work
    * When asked which components to install, don't change anything. Default options are Ok.
    * When asked whether ESP can modify the system, answer "Yes"

2. Launch the "ESP-IDF v4.4 CMD" (type that in the Windows search box)

3. From the ESP-IDF cmd, clone Bluepad32 repo

      ```sh
      git clone --recursive https://github.com/ricardoquesada/bluepad32.git
      ```

4. Setup

      ```cmd
      # Setup BTstack
      cd bluepad32\external\btstack\port\esp32
      # This will install BTstack as a component inside Bluepad32 source code (recommended).
      # Remove "IDF_PATH=../../../../src" if you want it installed in the ESP-IDF folder
      set IDF_PATH=..\..\..\..\src
      python integrate_btstack.py
      ```

      IMPORTANT: Once you complete the previous step, restart ESP-IDF CMD. This is because IDF_PATH
      has been modified, and the easiest way to restore it is to relaunch CMD.

      ```cmd
      # Setup Bluepad32 Platform
      cd bluepad32\src
      idf.py menuconfig
      ```

      And select `Component config` -> `Bluepad32` -> `Target platform` (choose the right one for you).

5. Compile it

      ```cmd
      # Compile it
      cd bluepad32\src
      idf.py build

      # Flash + open debug terminal
      idf.py flash monitor
      ```

[esp-idf-windows-setup]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html

[esp-idf-windows-installer]: https://dl.espressif.com/dl/esp-idf/?idf=4.4

## For Linux / macOS

1. Requirements and permissions

      Install ESP-IDF dependencies (taken from [here][toolchain-deps]):

      ```sh
      # For Ubuntu / Debian
      sudo apt-get install git wget flex bison gperf python3 python3-pip python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
      ```

      And in case you don't have permissions to open `/dev/ttyUSB0`, do:
      (taken from [here][ttyusb0])

      ```sh
      # You MUST logout/login (or in some cases reboot Linux) after running this command
      sudo usermod -a -G dialout $USER
      ```

2. Install and setup ESP-IDF

      ```sh
      # Needs to be done just once
      # Clone the ESP-IDF git repo
      mkdir ~/esp && cd ~/esp
      git clone -b release/v4.4 --recursive https://github.com/espressif/esp-idf.git

      # Then install the toolchain
      cd ~/esp/esp-idf
      ./install.sh
      ```

3. Clone Bluepad32 repo

      ```sh
      git clone --recursive https://github.com/ricardoquesada/bluepad32.git
      ```
 
4. Patch BTstack

      ```sh
      cd ${BLUEPAD32_SRC}/external/btstack
      git apply ../patches/*.patch 
      ```

5. Setup

      ```sh
      # Setup BTstack
      cd ${BLUEPAD32}/external/btstack/port/esp32
      # This will install BTstack as a component inside Bluepad32 source code (recommended).
      # Remove "IDF_PATH=../../../../src" if you want it installed in the ESP-IDF folder
      IDF_PATH=../../../../src ./integrate_btstack.py
      ```

6. Compile it

      ```sh
      # Compile it
      cd ${BLUEPAD32}/tools/fw
      ./build.py all
      ```

[toolchain-deps]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-setup.html

[ttyusb0]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/establish-serial-connection.html#linux-dialout-group