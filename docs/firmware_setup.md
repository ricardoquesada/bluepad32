# Flash precompiled firmware

!!! Note

    This section applies only to Unijoysticle boards.

To flash the latest firmware without a local toolchain, follow these steps:

1.  Download the latest precompiled firmware from the releases page:

    - <https://github.com/ricardoquesada/bluepad32/releases>

2.  Flash the binary with either the command line or the GUI tool.

## Command Line (esptool)

![esptool](https://lh3.googleusercontent.com/UfYRw0D2m6DUy337fskfNYP6FA3oj_AgATe6QU3y5OvGe14DaI5amCb-rhmGliSepoFYmhvX-u5uzq5N0wChP0lr0eSOrY4YMLB__UBZ8tY8ASbw5DgI6dUX-oEt2ZpWHPLpnBdxryA=-no)

Supported platforms: Linux, macOS, and Windows.

Install [`esptool.py`](https://docs.espressif.com/projects/esptool/en/latest/esp32/) ([source code](https://github.com/espressif/esptool)) for your operating system:

- Ubuntu or Debian: `sudo apt install esptool`
- Fedora or Red Hat: `sudo dnf install esptool`
- macOS with [Homebrew](https://brew.sh/): `brew install esptool`
- Windows, macOS without Homebrew, and other operating systems: Install
    [Python](https://www.python.org/downloads/) or get [Python from the Microsoft Store](https://www.microsoft.com/store/productId/9PJPW5LDXLZ5). Then run `pip install esptool`.

Run these commands to flash your device:

```sh
# Linux (port name can vary if multiple USB serial devices are connected)
export ESPPORT=/dev/ttyUSB0
# macOS
export ESPPORT=/dev/cu.SLAB_USBtoUART
# Windows (check Device Manager to identify the COM port for your device)
set ESPPORT=COM??

python -m esptool --chip esp32 --port ${ESPPORT} --baud 115200 --before default_reset --after hard_reset write_flash 0x0000 bluepad32-unijoysticle-full.bin
```

On Linux, standard users need write permission for the `ttyUSB` device. Grant
permission to your user account or run the command as root.

## GUI (ESP32 Flash Download Tool)

![flash_tool](https://lh3.googleusercontent.com/pw/ACtC-3c6KvmSei83mYKogxIadcq7tWamg41jsNk7pqJOpjnPhNoeN3uYjehB94wAja72mIDRNrhrWIqG0Sle1gxZHr0gANCSJyDFUcSfXMdoetUTynure2UrjRv7WkZEYnj0nqpiYJ54mwj85jDLkFrnD4jd-g=-no)

Supported platform: Windows.

Download the ESP32 Flash Download Tool:

- <https://www.espressif.com/en/support/download/other-tools>

Configure the tool with these parameters:

- Set `chipType` to `ESP32`.
- Select the `SPIDownload` tab.
- Click `...` and select the `bluepad32-unijoysticle-full.bin` file.
- Set the flash address to `0x0000`.
- Select the target `COM` port.
- Click `START`.

# Compile and flash firmware

## Windows

1.  Install [ESP-IDF v5.5][esp-idf-windows-installer]. See
    [ESP-IDF Getting Started for Windows][esp-idf-windows-setup] for details:

    * Use either the Online or Offline installer.
    * Keep the default component selections.
    * Click `Yes` when the installer asks for permission to modify the system.

2.  Start `ESP-IDF v5.5 CMD` from the Windows search menu.

3.  Clone the Bluepad32 repository:

    ```sh
    git clone --recursive https://github.com/ricardoquesada/bluepad32.git
    ```

4.  Configure the target platform:

    ```cmd
    # Configure Bluepad32 platform
    cd bluepad32\src
    idf.py menuconfig
    ```

    Select `Component config` -> `Bluepad32` -> `Target platform` and choose
    your target platform.

5.  Compile and flash the firmware:

    ```cmd
    # Compile the firmware
    cd bluepad32\src
    idf.py build

    # Flash and open the serial monitor
    idf.py flash monitor
    ```

[esp-idf-windows-setup]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html

[esp-idf-windows-installer]: https://dl.espressif.com/dl/esp-idf/?idf=5.5

## Linux and macOS

1.  Install the [ESP-IDF dependencies][toolchain-deps]:

    ```sh
    # For Ubuntu / Debian
    sudo apt-get install git wget flex bison gperf python3 python3-pip python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
    ```

    If you cannot open `/dev/ttyUSB0`, add your user to the `dialout` group
    ([see ESP-IDF serial documentation][ttyusb0]):

    ```sh
    # Log out and log back in after you run this command
    sudo usermod -a -G dialout $USER
    ```

2.  Clone and install ESP-IDF v5.5:

    ```sh
    # Clone the ESP-IDF repository
    mkdir ~/esp && cd ~/esp
    git clone -b release/v5.5 --recursive https://github.com/espressif/esp-idf.git

    # Install the toolchain
    cd ~/esp/esp-idf
    ./install.sh
    ```

3.  Clone the Bluepad32 repository:

    ```sh
    git clone --recursive https://github.com/ricardoquesada/bluepad32.git
    ```

4.  Apply the BTstack patches:

    ```sh
    cd ${BLUEPAD32_SRC}/external/btstack
    git apply ../patches/*.patch
    ```

5.  Compile the firmware:

    ```sh
    cd ${BLUEPAD32}/tools/fw
    ./build.py all
    ```

[toolchain-deps]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-setup.html

[ttyusb0]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/establish-serial-connection.html#linux-dialout-group
