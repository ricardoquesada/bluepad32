# Bluepad32 firmware for Arduino + ESP32

## Supported boards

It works on **any ESP32 / ESP32-S3 / ESP32-C3 module** where the [Arduino Core for ESP32][arduino-core] runs.
In other words, if you already have Arduino working on a ESP32 / ESP32-S3 / ESP32-C3 module, you can have Bluepad32 running on it as well.

There are three ways to setup a Bluepad32 Arduino project for ESP32 chips:

* Option A: Use Arduino IDE
  * Recommended for Arduino IDE users.
* Option B: Use ESP-IDF + template project
  * Recommended advanced users.
  * Fine-tune your project
  * Includes advanced features
* Option C: Starting a project from scratch
  * Not recommended for users. Only if you want to create your own "template" project from scratch.

[arduino-core]: https://github.com/espressif/arduino-esp32

## Option A: Create an "Arduino Core for ESP32 + Bluepad32" library

**RECOMMENDED for Arduino IDE users**.

[![Watch the video][youtube_image]](https://youtu.be/0jnY-XXiD8Q)

These 4 steps are needed:

1. Add ESP32 and Bluepad32 board packages to Board Manager
2. Install ESP32 and Bluepad32 files
3. Select a "ESP32 + Bluepad32" board
4. Open the "Bluepad32" example

[youtube_image]: https://lh3.googleusercontent.com/pw/AJFCJaXiDBy3NcQBBB-WFFVCsvYBs8szExsYQVwG5qqBTtKofjzZtJv_6GSL7_LfYRiypF1K0jjjgziXJuxAhoEawvzV84hlbmVTrGeXQYpVnpILZwWkbFi-ccX4lEzEbYXX-UbsEzpHLhO8qGVuwxOl7I_h1Q=-no?authuser=0

### 1. Add ESP32 and Bluepad32 board packages to Board Manager

These two boards must be added to Arduino IDE.

![arduino-board-manager][arduino-board-manager]

* Official ESP32 package: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
* "Bluepad32 + ESP32" package: `https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json`


If don't know how to add a board, then read [Add or Remove 3rd party boards in Board Manager][arduino_3rd_party_board]

### 2. Install ESP32 and Bluepad32 files

After adding both the ESP32 & Bluepad32 boards:
* go to: `Tools` -> `Board` -> `Board Manager`
* Install `ESP32` board package
* And install the `ESP32 + Bluepad32` board package

![arduino-bluepad32-install][arduino-bluepad32-install]

### 3. Select a "ESP32 + Bluepad32" board

Then choose any of the "ESP32 + Bluepad32" boards:

* `Tools` -> `Board` -> `ESP32 + Bluepad32 Arduino` -> The board you are using

![arduino-select-board][arduino-select-board]

### 4. Open the "Bluepad32" example

![arduino-bluepad32-example][arduino-bluepad32-example]

Finally open the Bluepad32 example:

* `File` -> `Examples` -> `Bluepad32_ESP32` -> `Controller`

And compile & flash!

[arduino_3rd_party_board]: https://support.arduino.cc/hc/en-us/articles/360016466340-Add-or-remove-third-party-boards-in-Boards-Manager
[arduino-board-manager]: https://lh3.googleusercontent.com/pw/AJFCJaVTKWM_lvVeTuaFcSk5Q6IfGZKFf6uKJnW7k_uOFVxC9SWAU5Ga_InmS8GgvKxQ5oh6w4jEz99lwPbyadId0pXBBw9RfBS9hmbTZ7kYVn_8Dmz3ybY6d-IvRbqeWsFkhB8oF8j0mo8OUOQTl54_zFY3Yw=-no
[arduino-bluepad32-install]: https://lh3.googleusercontent.com/pw/AJFCJaU35fPG9uzppEqonktTXlxJDXgf_33aeNmV_6XnYARTAlhH6PojpEJnK-XuZ-tLJEggPZxblmSL8qtogD59AVNnuUZI5-1kRzuqqHKTUf43eWw_HKUWjf5MlqPfjC_6464hUdW5i-C9mfi1dUDQwRwrbA=-no
[arduino-select-board]: https://lh3.googleusercontent.com/pw/AJFCJaVF6jr8D5R6ntl9TSX8nCoHJP96YHCfBpVhLtqBvYOunQietvKm8_tkAwNyF_gd32WoSvoK4gb0LMz3F__xl2JEwZUVksDq-RjI8fO4X4jwnc3O814Ztk0ZQ6di4sWVHnrFicOQBcJp1CaAydUImFZgvw=-no
[arduino-bluepad32-example]: https://lh3.googleusercontent.com/pw/AJFCJaXPSlzTv7Ol0nx2WpqepXgpDXjxJC_Cfxl_muVb1YamL1tWZSW7vFfbAHV212Lwgibg7trrI28CY9FGPNFI3fbS8dyPpJHS5rPFcYjxJyiCmMEIgef7S7B6CE33QozCD03xP7v57MY9L_MBRN3jyYJ9uw=-no


## Option B: Use ESP-IDF + template project

**RECOMMENDED for Advanced users**.

```sh
git clone --recursive https://gitlab.com/ricardoquesada/esp-idf-arduino-bluepad32-template.git my_project
```

... and that's it.

To test it just do:

```sh
# To compile it do:
idf.py build

# To flash it do:
idf.py flash monitor
```

To fine-tune it do:

```sh
# Add / edit / remove components with:
idf.py menuconfig
```

## Option C: Create your a project from scratch

Use this option if you want to understand how the "template" project (from Option A) was
created.

It is split in 4 parts:

* Create an empty project
* Install the needed components
* Update configuration
* Copy the "main Arduino" files to your project

### Create an empty ESP-IDF project

Create an ESP-IDF project from template:

```sh
# One simiple way to start a new ESP-IDF project, is to clone the "template" project
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

1. Add `set(ENV{BLUEPAD32_ARDUINO} TRUE)` to your main `CMakelists.txt`
2. `idf.py menuconfig`
3. Select "Components config" -> "Bluepad32" -> "Target platform" -> "Arduino"

   ![bluepad32-arduino](https://lh3.googleusercontent.com/pw/AM-JKLXm9ZyIvTKiTUlFBCT9QSaduKrhGZTXrWdR7G7F6krTHjkHJhpeGTXek_MCV3ZcXHCA8wnhxFAdDvQ_MbbGVMQY2AD58DK3DyK-_Cxua7BKHbvp8zkjtkcr87czftE7ySiCCUEcb6uSuMr9KY96JjQe-g=-no)

4. And set these Arduino options:
   * "Autostart Arduino setup and loop on boot" must be OFF
   * "Core on which Arduino's setup() and loop() are running" must be "Core 1"
     * Same for the remaining "Core" options
   * "Loop thread stack size": depends on what you do. 8192 is a good default value.
   * "Include only specific Arduino libraries" must ON
     * "Enable BLE" must be OFF
     * "Enable BluetoothSerial" must be OFF

    ![sdk-config](https://lh3.googleusercontent.com/pw/AM-JKLUC4p0Yf5fwxsmzBTqmisp09ElowiFvD06VZfVFeTe6qZZ7pavXZ3sOZ1qKe5wWvwCrnhZrvgOerIgb4XJcrX_fGQETiL2QObmE1u8KFn8wtRoO-vrLSJCRbQVgkC8_pnbyUQM4onrK6GXaaEf-Fuf4iQ=-no)

5. Set these Bluetooth options:
   * "Component Config" -> "Bluetooth" -> "Bluetooth Controller"
     * "Bluetooth Controller Mode": Bluetooth Dual Mode
     * "BLE Max Connections": 3
     * "BR/EDR ACL Max Connections": 7
     * "BR/EDR Sync (SCO/eSCO) Max Connections": 3
   * "Component Config" -> "Bluetooth" -> "Bluetooth Host"
     * "Controller only"

     ![sdkconfig-bluetooth](https://lh3.googleusercontent.com/pw/AM-JKLVOfishwCTAmGZN2owF0TNiTNVOlCR0DZf7PqUZprM0ujp_iM1e-tYMqDbhZKSe5zvJD4K4PCZJ-SuqO4IGnamgQL79vanzfvpItspvztGlsl0t_FlEkDYmif6q0WgbS6XCH7qrS0iM5LtqNxDySAWJhg=-no)

     ![sdkconfig-bluetooth2](https://lh3.googleusercontent.com/pw/AM-JKLUqEgrT5sF48hKUkmMsP2-9QzV6-JgyYyKwBfZA7GxjwOtQrDqYXvRE3R5tL7SQsAqRurXCiFqHoPU3k9noCtB-k_ZzJ4F_vqKqb9HVJXpI0ZkR5nJv8SzJ959LEmjjX9QaUteHpoJvbdHsiU-0TPoF8w=-no)

6. Set these ESP32 options:
   * "Component Config" -> "ESP32-specific"
      * "Main XTAL frequency": Autodetect

     ![sdkconfig-esp32](https://lh3.googleusercontent.com/pw/AM-JKLVvcfEonqhFDIWH98KajzMGSADBgaNoCI2QjGHaVFLPeRRAQMcIlXFwRmhvDSmNo6kIX_TGtKRr3V6EerW4ngPEiWbBtJYQPSOe2fixKC-rb16m3hhAVirbH7VnVmFwE1EXvRZk3MnNj7Yu2ydFn9f5Gg=-no)

7. Set Serial flasher config:
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
