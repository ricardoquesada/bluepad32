# Developer Notes

## Create a new release

* Update `src/components/bluepad32/include/uni_version.h` and
    `src/components/bluepad32/idf_component.yml`.
* Update `CHANGELOG.md`.
* Update `AUTHORS`.
* Merge `main` into `develop` and resolve any conflicts on `develop`.
* Merge `develop` into `main`:

    ```sh
    git merge main
    # Resolve conflicts if any exist
    git checkout main
    git merge develop
    ```

* Create a new annotated tag:

    ```sh
    git tag -a 4.0
    ```

* Push commits and tags to both GitLab and GitHub:

    ```sh
    git push gitlab
    git push gitlab --tags
    git push github
    git push github --tags
    ```

* Build the release binaries:

    ```sh
    cd tools/fw
    ./build.py --set-version v2.4.0 all
    ```

* Create the release on both GitLab and GitHub, and upload the release
    binaries.

## Analyze a core dump

Bluepad32 v2.4.0 and newer save core dumps in flash memory. Run this command to
read a core dump directly:

```sh
# Read and decode in one command
espcoredump.py --port /dev/ttyUSB0 --baud 921600 info_corefile build/bluepad32-app.elf
```

Alternatively, read the raw flash partition first and then decode the core file:

```sh
# Read flash and decode in two steps
esptool.py --port /dev/ttyUSB0 read_flash 0x110000 0x10000 /tmp/core.bin
espcoredump.py info_corefile --core /tmp/core.bin --core-format raw build/bluepad32-app.elf
```

## Analyze Bluetooth packets

1.  Build and run the POSIX example:

    ```sh
    cd examples/posix
    mkdir build && cd build
    cmake ..
    make -j
    sudo ./bluepad32_posix_example_app
    ```

2.  Stop the application and open the packet capture in Wireshark:

    ```sh
    wireshark /tmp/hci_dump.pklg
    ```

## Use OpenOCD with Pico W and Pico 2 W

For complete probe wiring details, see the
<https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html>
guide.

Enable UART output and disable USB output in `CMakeLists.txt`, then recompile:

```cmake
# Disable USB output
pico_enable_stdio_usb(bluepad32_picow_example_app 0)
# Enable UART output
pico_enable_stdio_uart(bluepad32_picow_example_app 1)
```

### Program Pico W and Pico 2 W

```sh
# For Pico W (RP2040):
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program bluepad32_picow_example_app.elf verify reset exit"

# For Pico 2 W (RP2350):
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "program bluepad32_picow_example_app.elf verify reset exit"
```

### Debug Pico W and Pico 2 W

Open four terminals and run these commands:

In terminal 1:

```sh
# For Pico W (RP2040):
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000"

# For Pico 2 W (RP2350):
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000"
```

In terminal 2:

```sh
arm-none-eabi-gdb bluepad32_picow_example_app.elf
(gdb) target remote localhost:3333
(gdb) cont
```

In terminal 3:

```sh
arm-none-eabi-gdb bluepad32_picow_example_app.elf
(gdb) target remote localhost:3334
(gdb) monitor reset init
(gdb) cont
```

In terminal 4:

```sh
tio /dev/ttyACM0
```

## Create a template project

> **Note:** Bluepad32 v5.0.0 moved the `bluepad32_arduino` component and
> template instructions to the standalone
> [`esp-idf-arduino-bluepad32-template`](https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template)
> repository. See that repository for instructions on ESP-IDF + Arduino Core
> projects.
