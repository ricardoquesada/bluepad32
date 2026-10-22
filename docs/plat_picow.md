# Bluepad32 for Pico W and Pico 2 W with Pico SDK

## Supported boards

- [Pico W][pico_w_board]
- [Pico 2 W][pico_2w_board]
- [Pico W clones][pico_w_clones]

[pico_w_board]: https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html#picow-technical-specification
[pico_2w_board]: https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html#pico2w-technical-specification
[pico_w_clones]: https://www.aliexpress.us/item/3256805949408500.html

## Installation

### 1. Install the Pico SDK

Install the Pico SDK for your operating system:

=== "macOS"

    Use Pico SDK v2.3.0 or newer.

    Clone the Pico SDK from GitHub (<https://github.com/raspberrypi/pico-sdk>):

    ```sh
    # Create a destination directory
    mkdir ~/pico && cd ~/pico

    # Clone the Pico SDK repository
    git clone https://github.com/raspberrypi/pico-sdk.git --branch master
    cd pico-sdk
    git submodule update --init
    ```

    Set `PICO_SDK_PATH` to the Pico SDK directory:

    ```sh
    # Set PICO_SDK_PATH to the correct path
    export PICO_SDK_PATH=$HOME/pico/pico-sdk/
    ```

=== "Linux"

    Use Pico SDK v2.3.0 or newer.

    Clone the Pico SDK from GitHub (<https://github.com/raspberrypi/pico-sdk>):

    ```sh
    # Create a destination directory
    mkdir ~/pico && cd ~/pico

    # Clone the Pico SDK repository
    git clone https://github.com/raspberrypi/pico-sdk.git --branch master
    cd pico-sdk
    git submodule update --init
    ```

    Set `PICO_SDK_PATH` to the Pico SDK directory:

    ```sh
    # Set PICO_SDK_PATH to the correct path
    export PICO_SDK_PATH=$HOME/pico/pico-sdk/
    ```

=== "Windows"

    Follow the official Windows installer instructions:

    * <https://www.raspberrypi.com/news/raspberry-pi-pico-windows-installer/>

### 2. Clone the Bluepad32 repository

```sh
git clone --recursive https://github.com/ricardoquesada/bluepad32.git
```

### 3. Apply the BTstack patches

```sh
cd ${BLUEPAD32_SRC}/external/btstack
git apply ../patches/*.patch
```

### 4. Customize the example

Open the Pico W example directory:

```sh
# ${BLUEPAD32} is the path to your cloned Bluepad32 repository
cd ${BLUEPAD32}/examples/pico_w
```

Edit `src/my_platform.c` to match your application requirements:

- [my_platform.c](https://github.com/ricardoquesada/bluepad32/blob/main/examples/pico_w/src/my_platform.c)

### 5. Build the firmware

Run these commands to build the `.uf2` firmware file:

```sh
mkdir build
cd build
cmake ..
make -j
```

### 6. Flash the firmware

Copy `build/bluepad32_picow_example_app.uf2` to your Pico W or Pico 2 W.

For flashing instructions, see:

- <https://projects.raspberrypi.org/en/projects/get-started-pico-w/>
