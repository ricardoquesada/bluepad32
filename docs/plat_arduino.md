# Bluepad32 for Arduino + ESP32

## Supported boards

It works on **any ESP32 / ESP32-S3 / ESP32-C3 module** where the [Arduino Core for ESP32][arduino-core] runs.
In other words, if you already have Arduino working on a ESP32 / ESP32-S3 / ESP32-C3 module, you can have Bluepad32
running on it as well.

!!! Bug

    No output in certain ESP32-S3 boards like [Arduino Nano ESP32][arduino_nano_esp32] or [Lolin S3 Mini][lolin_s3_mini] ?

    Read [Github issue #65][github_issue_65] to see how to enable it.

There are two ways to setup a Bluepad32 Arduino project for ESP32 chips:

* Option A: Use Arduino IDE
    * Recommended for Arduino IDE users.
* Option B: Use ESP-IDF + template project
    * Recommended advanced users.
    * Fine-tune your project
    * Includes advanced features

[arduino-core]: https://github.com/espressif/arduino-esp32
[github_issue_65]: https://github.com/ricardoquesada/bluepad32/issues/65#issuecomment-1987046804
[arduino_nano_esp32]: https://store-usa.arduino.cc/products/nano-esp32
[lolin_s3_mini]: https://www.wemos.cc/en/latest/s3/s3_mini.html

## Option A: Use "Arduino Core for ESP32 + Bluepad32" board

**RECOMMENDED for Arduino IDE users**.

<iframe width="560" height="315" src="https://www.youtube.com/embed/0jnY-XXiD8Q?si=YphWYgQf0a1YX_nq" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" allowfullscreen></iframe>

These 4 steps are needed:

1. Add ESP32 and Bluepad32 board packages to Board Manager
2. Install ESP32 and Bluepad32 files
3. Select a "ESP32 + Bluepad32" board
4. Open the "Bluepad32" example

### 1. Add ESP32 and Bluepad32 board packages to Board Manager

These two boards must be added to Arduino IDE.

![arduino-board-manager][arduino-board-manager]

* Official ESP32 package: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
* "Bluepad32 + ESP32"
  package: `https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json`

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

Finally, open the Bluepad32 example:

* `File` -> `Examples` -> `Bluepad32_ESP32` -> `Controller`

And compile & flash!

[arduino_3rd_party_board]: https://support.arduino.cc/hc/en-us/articles/360016466340-Add-or-remove-third-party-boards-in-Boards-Manager

[arduino-board-manager]: https://lh3.googleusercontent.com/pw/AJFCJaVTKWM_lvVeTuaFcSk5Q6IfGZKFf6uKJnW7k_uOFVxC9SWAU5Ga_InmS8GgvKxQ5oh6w4jEz99lwPbyadId0pXBBw9RfBS9hmbTZ7kYVn_8Dmz3ybY6d-IvRbqeWsFkhB8oF8j0mo8OUOQTl54_zFY3Yw=-no

[arduino-bluepad32-install]: https://lh3.googleusercontent.com/pw/AJFCJaU35fPG9uzppEqonktTXlxJDXgf_33aeNmV_6XnYARTAlhH6PojpEJnK-XuZ-tLJEggPZxblmSL8qtogD59AVNnuUZI5-1kRzuqqHKTUf43eWw_HKUWjf5MlqPfjC_6464hUdW5i-C9mfi1dUDQwRwrbA=-no

[arduino-select-board]: https://lh3.googleusercontent.com/pw/AJFCJaVF6jr8D5R6ntl9TSX8nCoHJP96YHCfBpVhLtqBvYOunQietvKm8_tkAwNyF_gd32WoSvoK4gb0LMz3F__xl2JEwZUVksDq-RjI8fO4X4jwnc3O814Ztk0ZQ6di4sWVHnrFicOQBcJp1CaAydUImFZgvw=-no

[arduino-bluepad32-example]: https://lh3.googleusercontent.com/pw/AJFCJaXPSlzTv7Ol0nx2WpqepXgpDXjxJC_Cfxl_muVb1YamL1tWZSW7vFfbAHV212Lwgibg7trrI28CY9FGPNFI3fbS8dyPpJHS5rPFcYjxJyiCmMEIgef7S7B6CE33QozCD03xP7v57MY9L_MBRN3jyYJ9uw=-no

## Option B: Use ESP-IDF + template project

!!! Note

    Recommended for advanced users.

### Clone the template project

```sh
git clone --recursive https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template.git my_project
```

To install and use ESP-IDF you have two options:

* a) Using PlatformIO
* b) Or manually install ESP-IDF toolchain

#### A) Using PlatformIO + ESP-IDF

![open_project][pio_open_project]

1. Open Visual Studio Code, select the PlatformIO plugin
2. Click on "Pick a folder", a select the recently cloned "my_project" folder

That's it. The PlatformIO will download the ESP-IDF toolchain and its dependencies.

It might take a few minutes to download all dependencies. Be patient.

![build_project][pio_build_project]

After all dependencies were installed:

1. Click on one of the pre-created boards, like *esp32-s3-devkit-1*. Or edit `platformio.ini` file, and add your own.
2. Click on *build*

![monitor_project][pio_monitor_project]

Finally, click on "Upload and Monitor":

* It will upload your sketch
* And will enter into "monitor" mode: You can see and use the console. Try typing `help` on the console.


[pio_open_project]: https://lh3.googleusercontent.com/pw/ABLVV85JEEjjsQqcCcfZUclYF1ItYSHPmpzP0SC4VH9Ypqp05r2ixlv9C2xv4p-r6fW_CyCNa8ylmeSjyUg_K2Sp-XUXQRTYO_6HvhQXcXxTZXgQvvNBqA8JaerwCB1UODkXgYa_6ONT19KTO52OMs0eOOeeMg=-no-gm?authuser=0
[pio_build_project]: https://lh3.googleusercontent.com/pw/ABLVV86DiV9H-wDEv1X8ra_fJAw0OG2sBoM5d0gJElPfptzVpb6n8gzOEHDfKXLMKrivzNSt03XpMWSw-hSVJUi0aavQiwgL0t1rmQeKqfYpXkGCKKwcerrNx8BBkFR3VoKQEPMF-e-xVvKVque2pi1sTa8tWA=-no-gm?authuser=0
[pio_monitor_project]: https://lh3.googleusercontent.com/pw/ABLVV845uPqRtJkUrv4JlODuTr7Shnw0HR7BdojRbxv3xWyiUO-V_Kv42YAKAV-XyoNRPY5vsyj0yRDsRxH0mxz8Q1NYzvhCKw5Ni9MH6UYR8IiaT8XS9hysR81APn8X2tnVgnmJ6ZkSPCgUURnE2MVYIWYrNQ=-no-gm?authuser=0

#### B) Manually installing ESP-IDF

Install ESP-IDF toolchain by following these instructions:

* [ESP-IDF for Windows][esp_idf_win]
* [ESP-IDF for Linux or macOS][esp_idf_linux]

[esp_idf_win]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html
[esp_idf_linux]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html

### Compile tempalte project and flash it

To compile it and flash it, do:

```sh
# cd to your project folder
cd my_project

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

### Further reading

Detailed instructions here: <https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template>
