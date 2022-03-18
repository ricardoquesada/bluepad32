# Flashing precompiled firmware

If you only want to flash the latest firmware version without downloading the toolchain + sources you should do:

Download latest precompiled firmware from here:

- <https://gitlab.com/ricardoquesada/bluepad32/-/releases>

And then you can flash it either using:

- Command Line
- or GUI

## Command Line (esptool)

![esptool](https://lh3.googleusercontent.com/UfYRw0D2m6DUy337fskfNYP6FA3oj_AgATe6QU3y5OvGe14DaI5amCb-rhmGliSepoFYmhvX-u5uzq5N0wChP0lr0eSOrY4YMLB__UBZ8tY8ASbw5DgI6dUX-oEt2ZpWHPLpnBdxryA=-no)

Supported platforms: Linux, macOS and Windows

For Command Line tool, download `esptool.py`:

- Source Code: <https://github.com/espressif/esptool>
- Ubuntu / Debian: `sudo apt install esptool`
- macOS: `brew install esptool`
- Windows and other OSs: `pip install esptool`

And:

```sh
# Linux
export ESPPORT=/dev/ttyUSB0
# macOS
export ESPPORT=/dev/cu.SLAB_USBtoUART
# Windows
set ESPPORT=COM??  #??? Try different ones

esptool.py --port ${ESPPORT} --baud 115200 --before default_reset --after hard_reset write_flash 0x0000 bluepad32-unijoysticle-full.bin
```

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

## For Windows

1. Install [ESP-IDF v4.4][esp-idf-windows-installer]. For further info, read: [ESP-IDF Getting Started for Windows][esp-idf-windows-setup]

   * Either the Online or Offline version shoud work
   * When asked which components to install, don't change anything. Default options are Ok.
   * When asked whether ESP can modify the system, answer "Yes"

2. Launch the "ESP-IDF v4.4 CMD" (type that in the Windows search box)

3. From the ESP-IDF cmd, clone Bluepad32 repo

   ```sh
   git clone --recursive https://gitlab.com/ricardoquesada/bluepad32.git
   ```

4. Setup

    ```sh
    # Setup BTStack
    cd ${BLUEPAD32}/external/btstack/port/esp32
    # This will install BTstack as a component inside Bluepad32 source code (recommended).
    # Remove "IDF_PATH=../../../../src" if you want it installed in the ESP-IDF folder
    IDF_PATH=../../../../src ./integrate_btstack.py
    ```

    ```sh
    # Setup Bluepad32 Platform
    cd ${BLUEPAD32}/src
    idf.py menuconfig
    ```

    And select `Component config` -> `Bluepad32` -> `Target platform` (choose the right one for you).

5. Compile it

    ```sh
    # Compile it
    cd ${BLUEPAD32}/src
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
   git clone --recursive https://gitlab.com/ricardoquesada/bluepad32.git
   ```

4. Setup

    ```sh
    # Setup BTStack
    cd ${BLUEPAD32}/external/btstack/port/esp32
    # This will install BTstack as a component inside Bluepad32 source code (recommended).
    # Remove "IDF_PATH=../../../../src" if you want it installed in the ESP-IDF folder
    IDF_PATH=../../../../src ./integrate_btstack.py
    ```

    ```sh
    # Setup Bluepad32 Platform
    cd ${BLUEPAD32}/src
    idf.py menuconfig
    ```

    And select `Component config` -> `Bluepad32` -> `Target platform` (choose the right one for you).

5. Compile it

    ```sh
    # Compile it
    cd ${BLUEPAD32}/src
    idf.py build

    # Flash + open debug terminal
    idf.py flash monitor
    ```


[toolchain-deps]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-setup.html
[ttyusb0]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/establish-serial-connection.html#linux-dialout-group


## Optional: Linux as a target device

Only if you target Linux as a device (not a ESP32 device):

1. Install dependencies

  ```sh
  sudo apt install libusb-1.0.0-dev
  ```

2. Setup BTSTack for libusb

  ```sh
  cd ${BLUEPAD32}/external/btstack/port/libusb
  make
  ```

3. Compile Bluepad32

  ```sh
  cd ${BLUEPAD32}/tools/pc_debug
  make
  sudo ./bluepad32
  ```

Put the gamepad in discovery mode. The gamepad should be recognized and when you press buttons, you should see them on the console.
