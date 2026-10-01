# Choose target platform

<img src="https://github.com/ricardoquesada/bluepad32/blob/main/docs/images/bluepad32-logo.png?raw=true" alt="drawing" width="400"/>

Bluepad32 supports different IDEs, and different microcontrollers. These are
the options:

| IDE / MCU              | ESP32 family     | Pico W           |
|------------------------|------------------|------------------|
| Arduino IDE            | :material-check: |                  |
| ESP-IDF + Arduino Core | :material-check: |                  |
| ESP-IDF (raw API)      | :material-check: |                  |
| Pico SDK (raw API)     |                  | :material-check: |

Choose the right one depending on your knowledge, devkit board and requirements:

=== "Arduino"

    For Arduino developers, there are two options.

    * [Arduino IDE with an ESP32 devkit][plat_arduino]
    * [Arduino Core with an ESP32 devkit using ESP-IDF toolchain][plat_arduino]
      (supports PlatformIO)

=== "ESP-IDF"

    * Works with ESP32 microcontrollers: ESP32, ESP32-S3, ESP32-C3, ESP32-C5,
      ESP32-C6, ESP32-H2
    * Works with ESP-IDF v5.5.x+
    * Uses the ESP-IDF SDK
    * Can be used with PlatformIO
    * Detailed info here: [Bluepad32 for ESP32 + ESP-IDF][plat_esp32_espidf]

=== "Pico SDK"

    * Works with Pico W and Pico 2 W microcontrollers.
    * Uses the Pico SDK.
    * Detailed info here: [Bluepad32 for Pico W + Pico SDK][plat_picow_picosdk]

## Where to start

| Platform                            | Start here                                                        | Further info              | Community projects                                                                                        | WiFi / BLE       | Other Features                                                      |
|-------------------------------------|-------------------------------------------------------------------|---------------------------|-----------------------------------------------------------------------------------------------------------|------------------|---------------------------------------------------------------------|
| Arduino IDE                         | [![Watch the video][youtube_image]](https://youtu.be/0jnY-XXiD8Q) | [Doc][plat_arduino]       | [Controller for Tello drone][tello]                                                                       | :material-check: | Easy to debug, familiar IDE, Arduino libraries                      |
| Arduino Core                        | [Template project][esp-idf-bluepad32-arduino]                     | [Doc][plat_arduino]       | [Lego Robot][esp32_example] ([video][esp32_video]), [gbaHD Shield][esp32_example2] (a GameBoy consolizer) | :material-check: | Very easy to debug, console, Arduino libraries, ESP-IDF, PlatformIO |
| Pico W                              | [Pico W example][pico-w-example]                                  | [Doc][plat_picow_picosdk] | [Pico Switch][pico_switch], [PicoNtrol][pico_ntrol]                                                       | :material-check: | Very easy to debug, for advanced developers, Pico SDK               |
| ESP-IDF                             | [ESP32 example][esp32-example]                                    | [Doc][plat_esp32_espidf]  | [OGX-Wireless-Lite][ogx_wireless_lite]                                                                    | :material-check: | Very easy to debug, for advanced developers, ESP-IDF, PlatformIO    |
| Posix (Linux, macOS)                | [Posix example][posix-example], [Posix ImGui][posix-imgui-example]| [Doc][plat_custom]        |                                                                                                           | :material-check: | Very easy to debug, useful for quick development                    | 

[amazon_esp32_c3_devkit]: https://www.amazon.com/s?k=esp32-c3+devkit

[amazon_esp32_devkit]: https://www.amazon.com/s?k=esp32+devkit

[amazon_esp32_s3_devkit]: https://www.amazon.com/s?k=esp32-s3+devkit

[arduino-esp-idf-example]: https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

[arduino-ide-example]: https://www.youtube.com/watch?v=0jnY-XXiD8Q

[arduino_ble_library]: https://www.arduino.cc/reference/en/libraries/arduinoble/

[btstack]: https://github.com/bluekitchen/btstack

[esp-idf-bluepad32-arduino]: https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

[esp32-example]: https://github.com/ricardoquesada/bluepad32/tree/main/examples/esp32

[esp32_example2]: https://github.com/ManCloud/GBAHD-Shield

[esp32_example]: https://github.com/antonvh/LMS-uart-esp/blob/main/Projects/LMS-ESP32/BluePad32_idf/README.md

[esp32_video]: https://www.instagram.com/p/Ca7T6twKZ0B/

[ogx_wireless_lite]: https://github.com/wiredopposite/OGX-Wireless-Lite

[pico-w-example]: https://github.com/ricardoquesada/bluepad32/tree/main/examples/pico_w

[pico_ntrol]: https://github.com/ShadeReogen/PicoNtrol

[pico_switch]: https://github.com/juan518munoz/PicoSwitch-WirelessGamepadAdapter

[plat_arduino]: ../plat_arduino

[plat_custom]: ../adding_new_platform

[plat_esp32_espidf]: ../plat_esp32

[plat_picow_picosdk]: ../plat_picow

[plat_unijoysticle]: ../plat_unijoysticle

[posix-example]: https://github.com/ricardoquesada/bluepad32/tree/main/examples/posix
[posix-imgui-example]: https://github.com/ricardoquesada/bluepad32/tree/main/examples/posix_imgui

[tello]: https://github.com/jsolderitsch/ESP32Controller

[youtube_image]: https://lh3.googleusercontent.com/pw/AJFCJaXiDBy3NcQBBB-WFFVCsvYBs8szExsYQVwG5qqBTtKofjzZtJv_6GSL7_LfYRiypF1K0jjjgziXJuxAhoEawvzV84hlbmVTrGeXQYpVnpILZwWkbFi-ccX4lEzEbYXX-UbsEzpHLhO8qGVuwxOl7I_h1Q=-no?authuser=0
