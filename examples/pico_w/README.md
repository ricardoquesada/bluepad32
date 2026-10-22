# Bluepad32 for Pico W

## Install dependencies

Install Pico SDK 2.2 or newer. The installation steps differ for Linux, macOS,
and Windows.

### Pico SDK for Linux and macOS

1.  Install the Pico SDK from <https://github.com/raspberrypi/pico-sdk>.
2.  Set `PICO_SDK_PATH` to the Pico SDK directory:

    ```shell
    # Set PICO_SDK_PATH to the correct path
    export PICO_SDK_PATH=$HOME/pico-sdk/
    ```

### Pico SDK for Windows

Follow the official Windows installer instructions:

* <https://www.raspberrypi.com/news/raspberry-pi-pico-windows-installer/>

## Build

Run these commands to build the example:

```shell
mkdir build
cd build
cmake ..
make -j
```

By default, CMake targets `pico_w`. To compile for Pico 2 W, run:

```shell
# Run from the "build" folder
cmake -DPICO_BOARD=pico2_w ..
make -j
```

Copy `build/bluepad32_picow_example_app.uf2` to your Pico W. For flashing
instructions, see:

* <https://projects.raspberrypi.org/en/projects/get-started-pico-w/>

## Debug with GDB

To debug with `gdb`, use a [Pico Debug Probe module][pico_probe] and follow the
[Pico Debug Probe instructions][pico_probe_doc].

Open 3 terminals and run the following commands:

### Terminal 1

```shell
# Edit CMakeLists.txt to enable the UART console and disable the USB console
vim CMakeLists.txt

# Verify these lines exist:
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
# Flash the binary with OpenOCD
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program bluepad32_picow_example_app.elf verify reset exit"
```

```shell
# Start the OpenOCD server
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

- Example code: Public Domain.
- Bluepad32: Apache 2.0 License.
- BTstack:
    - The Raspberry Pi Foundation provides a commercial license for Pico W and
        Pico 2 W hardware.
    - <https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_btstack/LICENSE.RP>
