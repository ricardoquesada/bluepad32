# Bluepad32 for Pico W

## Install dependencies

You need to install Pico SDK. The instructions are different for Linux/macOS and Windows.

Recommended: Use Pico SDK 2.1 or newer.

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

```shell
mkdir build
cd build
cmake ..
make -j
```

If `PICO_BOARD` is not specified, it will use `pico_w`. To compile it for Pico 2 W, do:

```shell
# From the recently crated "build" folder
cmake -DPICO_BOARD=pico2_w ..
make -j
```

Copy `build/bluepad32_picow_example_app.uf2` to Pico W.

Use this guide if you are not sure how to do it:

* <https://projects.raspberrypi.org/en/projects/get-started-pico-w/>

## Debugging

In case you need to debug it using `gdb`, the recommended way is to get a [Pico Debug Probe module][pico_probe], and
follow the [Pico Debug Probe instructions][pico_probe_doc].

TL;DR: Open 3 terminals, and do:

### Terminal 1

```shell
# Edit CMakeList.txt file and enable UART console, and disable USB console
vim CMakeList.txt

# Make sure you have these lines:
> pico_enable_stdio_usb(bluepad32_picow_example_app 0)
> pico_enable_stdio_uart(bluepad32_picow_example_app 1)
```

```shell
# Compile in debug mode
mkdir build
cd build
cmake -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Debug ..
make -j
```

```shell
# Flash it using OpenOCD
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program bluepad32_picow_example_app.elf verify reset exit"
```

```shell
# Open OpenOCD
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000"
```

### Terminal 2

```shell
arm-none-eabi-gdb bluepad32_picow_example_app.elf
> target remote :3333
> monitor reset init
> continue
```

### Terminal 3

```shell
# macOS
tio /dev/tty.usbmodem21202

# Linux
tio /dev/ttyACM0
```

[pico_probe]: https://www.raspberrypi.com/products/debug-probe/
[pico_probe_doc]: https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html

## License

- Example code: licensed under Public Domain.
- Bluepad32: licensed under Apache 2.
- BTstack:
  - Commercial license is already paid by the Raspberry Pi foundation, but only when used in a Pico W or Pico 2 W.
  - <https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_btstack/LICENSE.RP>
