# Bluepad32 for Pico W using Pico SDK

## Supported boards

- [Pico W][pico_w_board]
- [Pico W clones][pico_w_clones]

[pico_w_board]: https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html
[pico_w_clones]: https://www.aliexpress.us/item/3256805949408500.html

## Installation

### 1. Download Pico SDK

You need to install Pico SDK. The instructions are different for Linux/macOS and Windows.

#### 1.1 Pico SDK for Linux / macOS

Install Pico SDK: https://github.com/raspberrypi/pico-sdk

And set `PICO_SDK_PATH` to the correct path:

```
# set PICO_SDK_PATH to the correct path
export PICO_SDK_PATH=$HOME/pico-sdk/
```

### 1.2 or Pico SDK for Windows

Follow these instructions

* <https://www.raspberrypi.com/news/raspberry-pi-pico-windows-installer/>

### 2. Clone Bluepad32 GitHub repo

   ```sh
   git clone --recursive https://github.com/ricardoquesada/bluepad32.git
   ```

### 3. Modify example

Go to example folder:

   ```sh
   cd ${BLUEPAD32}/examples/pico_w
   ```

Customize `src/my_platform.c` file to your needs:

- [my_platform.c](https://github.com/ricardoquesada/bluepad32/blob/main/examples/pico_w/src/my_platform.c)


### 4. Build it

```sh
mkdir build
cd build
cmake ..
make -j
```

Copy `build/bluepad32_picow_example_app.uf2` to Pico W.

Use this guide if you are not sure how to do it:

* <https://projects.raspberrypi.org/en/projects/get-started-pico-w/>
