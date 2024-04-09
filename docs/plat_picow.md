# Bluepad32 for Pico W using Pico SDK

## Supported boards

- [Pico W][pico_w_board]
- [Pico W clones][pico_w_clones]

[pico_w_board]: https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html
[pico_w_clones]: https://www.aliexpress.us/item/3256805949408500.html

## Installation

### 1. Download Pico SDK

You need to install Pico SDK. The instructions are different for Linux/macOS and Windows.

=== "Pico SDK for Linux / macOS"

    Clone Pico SDK from Github: <https://github.com/raspberrypi/pico-sdk>

    ```sh
    # Optionally create a destination folder
    mkdir ~/pico && cd ~/pico

    # Clone it from Github
    git clone https://github.com/raspberrypi/pico-sdk.git --branch master
    cd pico-sdk
    git submodule update --init
    ```

    And set `PICO_SDK_PATH` to the correct path:

    ```
    # set PICO_SDK_PATH to the correct path
    export PICO_SDK_PATH=$HOME/pico/pico-sdk/
    ```

=== "Pico SDK for Windows"

    Follow these instructions

    * <https://www.raspberrypi.com/news/raspberry-pi-pico-windows-installer/>

### 2. Clone Bluepad32 GitHub repo

   ```sh
   git clone --recursive https://github.com/ricardoquesada/bluepad32.git
   ```

### 3. Patch BTstack

   ```sh
   cd ${BLUEPAD32_SRC}/external/btstack
   git apply ../patches/*.patch
   ```

### 4. Modify example

Go to example folder:

   ```sh
   # ${BLUEPAD32} represents the folder where Bluepad32 Github repo was cloned
   cd ${BLUEPAD32}/examples/pico_w
   ```

Customize `src/my_platform.c` file to your needs:

- [my_platform.c](https://github.com/ricardoquesada/bluepad32/blob/main/examples/pico_w/src/my_platform.c)


### 5. Build it

Build the `.uf2` file by doing:

```sh
mkdir build
cd build
cmake ..
make -j
```

### 6. Flash it

Copy `build/bluepad32_picow_example_app.uf2` to Pico W.

Use this guide if you are not sure how to do it:

* <https://projects.raspberrypi.org/en/projects/get-started-pico-w/>
