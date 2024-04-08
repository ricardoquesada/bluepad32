# Developer Notes

## Creating a new release

* update `src/version.txt` and `uni_version.h`
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

* And generate the release both in Gitlab and GitHub, and upload the already generated binaries

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

## Using OpenOCD with Pico W

Detailed instructions here: <https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html>

Recompile by using UART as output. In the `CMakeLists.txt` do this:
```cmake
# Disable USB output
pico_enable_stdio_usb(bluepad32_picow_example_app 0)
# Enable UART output
pico_enable_stdio_uart(bluepad32_picow_example_app 1)
```

### Program Pico W

```sh
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program bluepad32_picow_example_app.elf verify reset exit"
```

### Debug Pico W

Have four terminals.

In terminal 1:

```sh
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000"
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

It is split in four parts:

* Create an empty project
* Install the needed components
* Update configuration
* Copy the "main Arduino" files to your project

### Create an empty ESP-IDF project

Create an ESP-IDF project from template:

```sh
# One simple way to start a new ESP-IDF project, is to clone the "template" project
git clone --recursive https://github.com/espressif/esp-idf-template my_project
```

### Installing components

Include the following components in your project's `components` folder:

* arduino
* bluepad32
* bluepad32_arduino
* btstack

Arduino component:

```sh
cd ${MYPROJECT}/components/
git clone --recursive https://github.com/espressif/arduino-esp32.git arduino
```

Bluepad32 / Bluepad32_arduino component:

```sh
# Just copy bluepad32 component to your project's component folder
cp -r ${BLUEPAD32}/src/components/bluepad32 ${MYPROJECT}/components
cp -r ${BLUEPAD32}/src/components/bluepad32_arduino ${MYPROJECT}/components
```

BTStack component:

```sh
cd ${BLUEPAD32}/external/btstack/port/esp32
# This will install BTstack as a component inside Bluepad32 source code (recommended).
# Remove "IDF_PATH=${BLUEPAD32}/src/" if you want it installed in the ESP-IDF folder
IDF_PATH=${BLUEPAD32}/src/ ./integrate_btstack.py
```

### Update configuration

And then do:

1. `idf.py menuconfig`
2. And set these Arduino options from `Arduino Configuration`:

* "Autostart Arduino setup and loop on boot" must be OFF
* "Core on which Arduino's setup() and loop() are running" must be "Core 1"
    * Same for the remaining "Core" options
* "Loop thread stack size": depends on what you do. 8192 is a good default value.
* "Include only specific Arduino libraries" must ON
    * "Enable BLE" must be OFF
    * "Enable BluetoothSerial" must be OFF

  ![sdk-config](https://lh3.googleusercontent.com/pw/AM-JKLUC4p0Yf5fwxsmzBTqmisp09ElowiFvD06VZfVFeTe6qZZ7pavXZ3sOZ1qKe5wWvwCrnhZrvgOerIgb4XJcrX_fGQETiL2QObmE1u8KFn8wtRoO-vrLSJCRbQVgkC8_pnbyUQM4onrK6GXaaEf-Fuf4iQ=-no)

3. Set these Bluetooth options:

* "Component Config" -> "Bluetooth" -> "Bluetooth Controller"
    * "Bluetooth Controller Mode": Bluetooth Dual Mode
    * "BLE Max Connections": 3
    * "BR/EDR ACL Max Connections": 7
    * "BR/EDR Sync (SCO/eSCO) Max Connections": 3
* "Component Config" -> "Bluetooth" -> "Bluetooth Host"
    * "Controller only"

  ![sdkconfig-bluetooth](https://lh3.googleusercontent.com/pw/AM-JKLVOfishwCTAmGZN2owF0TNiTNVOlCR0DZf7PqUZprM0ujp_iM1e-tYMqDbhZKSe5zvJD4K4PCZJ-SuqO4IGnamgQL79vanzfvpItspvztGlsl0t_FlEkDYmif6q0WgbS6XCH7qrS0iM5LtqNxDySAWJhg=-no)

  ![sdkconfig-bluetooth2](https://lh3.googleusercontent.com/pw/AM-JKLUqEgrT5sF48hKUkmMsP2-9QzV6-JgyYyKwBfZA7GxjwOtQrDqYXvRE3R5tL7SQsAqRurXCiFqHoPU3k9noCtB-k_ZzJ4F_vqKqb9HVJXpI0ZkR5nJv8SzJ959LEmjjX9QaUteHpoJvbdHsiU-0TPoF8w=-no)

4. Set these ESP32 options:

* "Component Config" -> "ESP32-specific"
    * "Main XTAL frequency": Autodetect

  ![sdkconfig-esp32](https://lh3.googleusercontent.com/pw/AM-JKLVvcfEonqhFDIWH98KajzMGSADBgaNoCI2QjGHaVFLPeRRAQMcIlXFwRmhvDSmNo6kIX_TGtKRr3V6EerW4ngPEiWbBtJYQPSOe2fixKC-rb16m3hhAVirbH7VnVmFwE1EXvRZk3MnNj7Yu2ydFn9f5Gg=-no)

5. Set Serial flasher config:

* "Serial flasher cofnig"
    * "Flash size": 4MB (or choose the right for your module)

### Copy Bluepad32-Arduino API files

And in your *main* project, you must include the files that are located in `${BLUEPAD32}/src/main/*`

```sh
cd my_project/main
cp -r ${BLUEPAD32}/src/main/* .
```

Further reading:

* [Arduino as a ESP-IDF component][esp-idf-component]

[esp-idf-component]: https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html
