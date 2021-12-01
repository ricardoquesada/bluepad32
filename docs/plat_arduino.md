# Bluepad32 firmware for Arduino

**EXPERIMENTAL FEATURE**

These instructions are "volatile". Might significately change in the future.

## Supported boards

It works on any ESP32 module where the [Arduino Core for ESP32][arduino-core] runs.
In other words, if you already have Arduino working on ESP32 module, you can have Bluepad32 running on it as well.

But there is catch:

* It only works on ESP-IDF v4.4 or newer
* It is driven from command line, using Makefile. Might not work from Arduino IDE.

Are you still interested ? Good, then you must follow these instructions:

1. Follow the [Bluepad32 firmware setup instructions][bluepad32-fw-setup]
   * But make sure to use ESP-IDF **v4.4** (or newer)
2. Clone "Arduino Core for ESP32" in the `src/components`

    ```sh
    cd $BLUEPAD32_SRC/src/components
    git clone https://github.com/espressif/arduino-esp32.git arduino
    cd arduino
    git submodule update --init --recursive
    ```

3. Run `menuconfig`

    ```sh
    cd $BLUEPAD32_SRC/src
    make menuconfig
    ```

4. Set the right Arduino options:
   * "Autostart Arduino setup and loop on boot" must be OFF
   * "Core on which Arduino's setup() and loop() are running" must be "Core 1"
     * Same for the remaining "Core" options
   * "Loop thread stack size": depends on what you do. 8192 is a good default value.
   * "Include only specific Arduino libraries" must ON
     * "Enable BLE" must be OFF
     * "Enable BluetoothSerial" must be OFF

    ![sdk-config](https://lh3.googleusercontent.com/pw/AM-JKLUC4p0Yf5fwxsmzBTqmisp09ElowiFvD06VZfVFeTe6qZZ7pavXZ3sOZ1qKe5wWvwCrnhZrvgOerIgb4XJcrX_fGQETiL2QObmE1u8KFn8wtRoO-vrLSJCRbQVgkC8_pnbyUQM4onrK6GXaaEf-Fuf4iQ=-no)

5. Set "PLATFORM=arduino" environment variable

    ```sh
    export PLATFORM=arduino
    ```

6. Add your custom code

    ```sh
    cd BLUEPAD32_SRC/src/main
    vim arduino_main.cpp
    ```

7. Compile it and flash it

    ```sh
    cd BLUEPAD32_SRC/src
    make flash
    make monitor
    ```

[arduino-core]: https://github.com/espressif/arduino-esp32
[bluepad32-fw-setup]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/main/docs/firmware_setup.md#compiling-flashing-firmware
[esp-idf-setup]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/
