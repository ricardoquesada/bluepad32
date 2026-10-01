# Developer Notes

## Creating a new release

* update `src/components/bluepad32/include/uni_version.h` and
  `src/components/bluepad32/idf_component.yml`
* update `CHANGELOG.md`
* update `AUTHORS`
* merge `main` into `develop`... and solve possible conflicts on `develop` first
* then merge `develop` into `main`

  ```sh
  git merge main
  # Solve possible conflicts
  git checkout main
  git merge develop
  ```

* generate a new tag

  ```sh
  git tag -a 4.0
  ```

* push changes both to Gitlab and GitHub:

  ```sh
  git push gitlab
  git push gitlab --tags
  git push github
  git push github --tags
  ```

* Generate binaries

  ```sh
  cd tools/fw
  ./build.py --set-version v2.4.0 all
  ```

* And generate the release both in Gitlab and GitHub, and upload the already
  generated binaries

## Analyzing a core dump

Since v2.4.0, core dumps are stored in flash. And they can be retrieved using:

 ```sh
 # Using just one command
 espcoredump.py --port /dev/ttyUSB0 --baud 921600 info_corefile build/bluepad32-app.elf
 ```

```sh
# Or it can be done in two parts
esptool.py --port /dev/ttyUSB0 read_flash 0x110000 0x10000 /tmp/core.bin
espcoredump.py info_corefile --core /tmp/core.bin --core-format raw build/bluepad32-app.elf 
 ```

## Analyzing Bluetooth packets

Use the "posix" platform:

```sh
cd example/posix
mkdir build && cd build
cmake ..
make -j
sudo ./bluepad32_posix_example_app
```

Let it run... stop it... and open the logs using:

```sh
wireshark /tmp/hci_dump.pklg
```

## Using OpenOCD with Pico W / Pico 2 W

Detailed instructions here: <https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html>

Recompile by using UART as output. In the `CMakeLists.txt` do this:
```cmake
# Disable USB output
pico_enable_stdio_usb(bluepad32_picow_example_app 0)
# Enable UART output
pico_enable_stdio_uart(bluepad32_picow_example_app 1)
```

### Program Pico W / Pico 2 W

```sh
# For Pico W (RP2040):
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program bluepad32_picow_example_app.elf verify reset exit"

# For Pico 2 W (RP2350):
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "program bluepad32_picow_example_app.elf verify reset exit"
```

### Debug Pico W / Pico 2 W

Have four terminals.

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

## Creating a template project from scratch

> **Note:** As of Bluepad32 v5.0.0, the `bluepad32_arduino` component and
> template instructions have been moved to the standalone
> [`esp-idf-arduino-bluepad32-template`](https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template)
> repository. Please refer to that repository for up-to-date instructions on
> starting an ESP-IDF + Arduino Core project.
