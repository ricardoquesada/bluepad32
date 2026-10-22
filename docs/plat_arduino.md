# Bluepad32 for Arduino + ESP32

## Supported boards

Bluepad32 runs on **any ESP32, ESP32-S3, ESP32-C3, ESP32-C5, ESP32-C6, or
ESP32-H2 module** that supports the [Arduino Core for ESP32][arduino-core].

!!! Bug

    If you see no serial output on ESP32-S3 boards such as the
    [Arduino Nano ESP32][arduino_nano_esp32] or [Lolin S3 Mini][lolin_s3_mini],
    read [GitHub issue #65][github_issue_65] to enable USB CDC output.

Choose one of two methods to configure a Bluepad32 Arduino project:

* Option A: Use Arduino IDE (recommended for Arduino IDE users)
* Option B: Use ESP-IDF with the template project (recommended for advanced
    configuration and console access)

[arduino-core]: https://github.com/espressif/arduino-esp32
[github_issue_65]: https://github.com/ricardoquesada/bluepad32/issues/65#issuecomment-1987046804
[arduino_nano_esp32]: https://store-usa.arduino.cc/products/nano-esp32
[lolin_s3_mini]: https://www.wemos.cc/en/latest/s3/s3_mini.html

## Option A: Use the "Arduino Core for ESP32 + Bluepad32" board

**Recommended for Arduino IDE users.**

<iframe width="560" height="315" src="https://www.youtube.com/embed/0jnY-XXiD8Q?si=YphWYgQf0a1YX_nq" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" allowfullscreen></iframe>

<iframe width="560" height="315" src="https://www.youtube.com/embed/EEViXFoSzww?si=DsOQGOOwuKC2qOtz" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>

<iframe width="560" height="315" src="https://www.youtube.com/embed/Laa93Wj7f-I?si=gRxO5sAWl0cAE8vn" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>

Complete these 4 steps:

1.  Add the ESP32 and Bluepad32 board packages to Boards Manager.
2.  Install the ESP32 and Bluepad32 board packages.
3.  Select an `ESP32 + Bluepad32` board.
4.  Open and upload the `Bluepad32` example sketch.

### 1. Add the ESP32 and Bluepad32 board packages to Boards Manager

Add these two board index URLs to Arduino IDE:

![arduino-board-manager][arduino-board-manager]

* Official ESP32 package:
    `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
* Bluepad32 + ESP32 package:
    `https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json`

For instructions on adding board URLs, read
[Add or remove third-party boards in Boards Manager][arduino_3rd_party_board].

### 2. Install the ESP32 and Bluepad32 board packages

After you add both board URLs, install the packages:

* Open `Tools` -> `Board` -> `Boards Manager`.
* Install the `esp32` board package.
* Install the `esp32_bluepad32` board package.

![arduino-bluepad32-install][arduino-bluepad32-install]

### 3. Select an "ESP32 + Bluepad32" board

Select your target board from the menu:

* Open `Tools` -> `Board` -> `ESP32 + Bluepad32 Arduino` and select your board.

![arduino-select-board][arduino-select-board]

### 4. Open and upload the "Bluepad32" example

![arduino-bluepad32-example][arduino-bluepad32-example]

Open the Bluepad32 Controller example sketch:

* Open `File` -> `Examples` -> `Bluepad32_ESP32` -> `Controller`.

Compile and upload the sketch to your board.

[arduino_3rd_party_board]: https://support.arduino.cc/hc/en-us/articles/360016466340-Add-or-remove-third-party-boards-in-Boards-Manager

[arduino-board-manager]: https://lh3.googleusercontent.com/pw/AJFCJaVTKWM_lvVeTuaFcSk5Q6IfGZKFf6uKJnW7k_uOFVxC9SWAU5Ga_InmS8GgvKxQ5oh6w4jEz99lwPbyadId0pXBBw9RfBS9hmbTZ7kYVn_8Dmz3ybY6d-IvRbqeWsFkhB8oF8j0mo8OUOQTl54_zFY3Yw=-no

[arduino-bluepad32-install]: https://lh3.googleusercontent.com/pw/AJFCJaU35fPG9uzppEqonktTXlxJDXgf_33aeNmV_6XnYARTAlhH6PojpEJnK-XuZ-tLJEggPZxblmSL8qtogD59AVNnuUZI5-1kRzuqqHKTUf43eWw_HKUWjf5MlqPfjC_6464hUdW5i-C9mfi1dUDQwRwrbA=-no

[arduino-select-board]: https://lh3.googleusercontent.com/pw/AJFCJaVF6jr8D5R6ntl9TSX8nCoHJP96YHCfBpVhLtqBvYOunQietvKm8_tkAwNyF_gd32WoSvoK4gb0LMz3F__xl2JEwZUVksDq-RjI8fO4X4jwnc3O814Ztk0ZQ6di4sWVHnrFicOQBcJp1CaAydUImFZgvw=-no

[arduino-bluepad32-example]: https://lh3.googleusercontent.com/pw/AJFCJaXPSlzTv7Ol0nx2WpqepXgpDXjxJC_Cfxl_muVb1YamL1tWZSW7vFfbAHV212Lwgibg7trrI28CY9FGPNFI3fbS8dyPpJHS5rPFcYjxJyiCmMEIgef7S7B6CE33QozCD03xP7v57MY9L_MBRN3jyYJ9uw=-no

## Option B: Use ESP-IDF with the template project

!!! Note

    Recommended for advanced users.

### Clone the template project

```sh
git clone --recursive https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template.git my_project
```

Choose one of two ways to build with ESP-IDF:

* Option A: Use PlatformIO
* Option B: Install the ESP-IDF toolchain manually

#### A) Use PlatformIO with ESP-IDF

![open_project][pio_open_project]

1.  Open Visual Studio Code and select the PlatformIO extension.
2.  Click **Pick a folder** and select the `my_project` folder.

PlatformIO automatically downloads the ESP-IDF toolchain and all required
dependencies.

![build_project][pio_build_project]

After PlatformIO installs all dependencies, build the project:

1.  Select a preconfigured board target (such as `esp32-s3-devkit-1`), or add
    your board in `platformio.ini`.
2.  Click **Build**.

![monitor_project][pio_monitor_project]

Click **Upload and Monitor**:

* PlatformIO flashes the firmware to your board.
* PlatformIO opens the serial console. Type `help` in the console to list
    available commands.

[pio_open_project]: https://lh3.googleusercontent.com/pw/ABLVV85JEEjjsQqcCcfZUclYF1ItYSHPmpzP0SC4VH9Ypqp05r2ixlv9C2xv4p-r6fW_CyCNa8ylmeSjyUg_K2Sp-XUXQRTYO_6HvhQXcXxTZXgQvvNBqA8JaerwCB1UODkXgYa_6ONT19KTO52OMs0eOOeeMg=-no-gm?authuser=0
[pio_build_project]: https://lh3.googleusercontent.com/pw/ABLVV86DiV9H-wDEv1X8ra_fJAw0OG2sBoM5d0gJElPfptzVpb6n8gzOEHDfKXLMKrivzNSt03XpMWSw-hSVJUi0aavQiwgL0t1rmQeKqfYpXkGCKKwcerrNx8BBkFR3VoKQEPMF-e-xVvKVque2pi1sTa8tWA=-no-gm?authuser=0
[pio_monitor_project]: https://lh3.googleusercontent.com/pw/ABLVV845uPqRtJkUrv4JlODuTr7Shnw0HR7BdojRbxv3xWyiUO-V_Kv42YAKAV-XyoNRPY5vsyj0yRDsRxH0mxz8Q1NYzvhCKw5Ni9MH6UYR8IiaT8XS9hysR81APn8X2tnVgnmJ6ZkSPCgUURnE2MVYIWYrNQ=-no-gm?authuser=0

#### B) Install ESP-IDF manually

Follow the official ESP-IDF setup guide for your operating system:

* [ESP-IDF for Windows][esp_idf_win]
* [ESP-IDF for Linux or macOS][esp_idf_linux]

[esp_idf_win]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html
[esp_idf_linux]: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html

### Compile and flash the template project

Run these commands to build and flash the project:

```sh
# Open your project folder
cd my_project

# Compile the project
idf.py build

# Flash the binary and open the serial monitor
idf.py flash monitor
```

Run `menuconfig` to customize project components:

```sh
# Configure components
idf.py menuconfig
```

### Further reading

See the template repository for complete documentation:
<https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template>
