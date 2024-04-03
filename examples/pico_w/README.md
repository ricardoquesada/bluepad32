# Bluepad32 for Pico W

## Install dependencies

You need to install Pico SDK. The instructions are different for Linux/macOS and Windows.

### Pico SDK for Linux / macOS

Install Pico SDK: https://github.com/raspberrypi/pico-sdk

And set `PICO_SDK_PATH` to the correct path:

```
# set PICO_SDK_PATH to the correct path
export PICO_SDK_PATH=$HOME/pico-sdk/
```

### Pico SDK for Windows

Follow these instructions

* <https://www.raspberrypi.com/news/raspberry-pi-pico-windows-installer/>

## Build

And then build

```sh
mkdir build
cd build
cmake ..
make -j
```

Copy `build/bluepad32_picow_example_app.uf2` to Pico W.

Use this guide if you are not sure how to do it:

* <https://projects.raspberrypi.org/en/projects/get-started-pico-w/>

## License

- Example code: licensed under Public Domain.
- Bluepad32: licensed under Apache 2.
- BTstack:
  - Commercial license is already paid by the Raspberry Pi foundation, but only when used in a Pico W.
  - <https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_btstack/LICENSE.RP>