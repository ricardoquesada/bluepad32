# Choose target platform

Bluepad32 supports different IDEs, and different microcontrollers. These are the options:

| IDE / MCU                 | ESP32 family     | NINA co-processor | Airlift co-processor | Pico W           |
|---------------------------|------------------|-------------------|----------------------|------------------|
| Arduino IDE               | :material-check: | :material-check:  |                      |                  |
| Arduino Core / PlatformIO | :material-check: |                   |                      |                  |
| CircuitPython             |                  |                   | :material-check:     |                  |
| ESP-IDF (raw API)         | :material-check: |                   |                      |                  |
| PICO SDK (raw API)        |                  |                   |                      | :material-check: |

Choose the right one depending on your knowledge, devkit board and requirements:

=== "Arduino"

    For Arduino developers, there are three options.

    * [Arduino IDE with an ESP32 devkit][plat_arduino]
    * [Arduino Core with an ESP32 devkit using ESP-IDF toolchain][plat_arduino] (supports PlatformIO)
    * [Arduino IDE with an Arduino NINA devkit][plat_nina]

=== "CircuitPython"

    * Works with Airlift co-processor boards.
    * Detailed info here: [Bluepad32 for CircuitPython][bp32-circuitpython]

=== "ESP-IDF"

    * Works with ESP32 microcontrollers: ESP32, ESP32-S3, ESP32-C3.
    * Works both with ESP-IDF v4.4 and v5.X
    * Uses the ESP-IDF SDK
    * Use this example as reference: [ESP32 example][esp32-example]

=== "Pico SDK"

    * Works with Pico W microcontroller.
    * Uses the Pico SDK.
    * Use this example as reference: [Pico W example][pico-w-example]

## Where to start

| Platform                            | Start here                                                        | Further info        | Community projects                                                                                        | WiFi / BLE       | Other Features                                                      |
|-------------------------------------|-------------------------------------------------------------------|---------------------|-----------------------------------------------------------------------------------------------------------|------------------|---------------------------------------------------------------------|
| Arduino IDE                         | [![Watch the video][youtube_image]](https://youtu.be/0jnY-XXiD8Q) | [Doc][plat_arduino] | [Controller for Tello drone][tello]                                                                       | :material-check: | Easy to debug, familiar IDE, Arduino libraries                      |
| Arduino using ESP-IDF toolchain     | [Template project][esp-idf-bluepad32-arduino]                     | [Doc][plat_arduino] | [Lego Robot][esp32_example] ([video][esp32_video]), [gbaHD Shield][esp32_example2] (a GameBoy consolizer) | :material-check: | Very easy to debug, console, Arduino libraries, PlatformIO          |
| Arduino + NINA coprocessor          | [Arduino Library][bp32-arduino]                                   | [Doc][plat_nina]    | [Philips CD-i meets Bluetooth][nina_example]                                                              | :material-close: | Difficult to debug, familiar IDE, Arduino libraries, **deprecated** |
| CircuitPython + AirLift coprocessor | [CircuitPython Library][bp32-circuitpython]                       | [Doc][plat_airlift] | [Quico console][airlift_example], Controlling 4 servos ([video][airlift_video])                           | :material-close: | Difficult to debug, easy to program, CircuitPython libraries        |
| Pico W                              | [Pico W example][pico-w-example]                                  | [Doc][plat_custom]  | [Pico Switch][pico_switch]                                                                                | :material-check: | Very easy to debug, for advanced developers, Pico SDK               |
| ESP-IDF                             | [ESP32 example][esp32-example]                                    | [Doc][plat_custom]  |                                                                                                           | :material-check: | Very easy to debug, for advanced developers, ESP-IDF SDK            |
| Posix (Linux, macOS)                | [Posix example][posix-example]                                    | [Doc][plat_custom]  |                                                                                                           | :material-check: | Very easy to debug, useful for quick development                    | 

[airlift_example]: https://gitlab.com/ricardoquesada/quico

[airlift_video]: https://twitter.com/makermelissa/status/1482596378282913793

[arduino-esp-idf-example]: https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

[arduino-ide-example]: https://www.youtube.com/watch?v=0jnY-XXiD8Q

[bp32-arduino]: https://github.com/ricardoquesada/bluepad32-arduino

[bp32-circuitpython]: https://github.com/ricardoquesada/bluepad32-circuitpython

[esp-idf-bluepad32-arduino]: https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

[esp32-example]: https://github.com/ricardoquesada/bluepad32/tree/main/examples/esp32

[esp32_example2]: https://github.com/ManCloud/GBAHD-Shield

[esp32_example]: https://github.com/antonvh/LMS-uart-esp/blob/main/Projects/LMS-ESP32/BluePad32_idf/README.md

[esp32_video]: https://www.instagram.com/p/Ca7T6twKZ0B/

[posix-example]: https://github.com/ricardoquesada/bluepad32/tree/main/examples/posix

[nina_example]: https://eyskens.me/cd-i-meets-bluetooth/

[pico-w-example]: https://github.com/ricardoquesada/bluepad32/tree/main/examples/pico_w

[pico_switch]: https://github.com/juan518munoz/PicoSwitch-WirelessGamepadAdapter

[plat_airlift]: ../plat_airlift

[plat_arduino]: ../plat_arduino

[plat_custom]: ../adding_new_platform

[plat_mightymiggy]: ../plat_mightymiggy

[plat_nina]: ../plat_nina

[plat_unijoysticle]: ../plat_unijoysticle

[tello]: https://github.com/jsolderitsch/ESP32Controller

[youtube_image]: https://lh3.googleusercontent.com/pw/AJFCJaXiDBy3NcQBBB-WFFVCsvYBs8szExsYQVwG5qqBTtKofjzZtJv_6GSL7_LfYRiypF1K0jjjgziXJuxAhoEawvzV84hlbmVTrGeXQYpVnpILZwWkbFi-ccX4lEzEbYXX-UbsEzpHLhO8qGVuwxOl7I_h1Q=-no?authuser=0

[amazon_esp32_devkit]: https://www.amazon.com/s?k=esp32+devkit

[amazon_esp32_s3_devkit]: https://www.amazon.com/s?k=esp32-s3+devkit

[amazon_esp32_c3_devkit]: https://www.amazon.com/s?k=esp32-c3+devkit

[btstack]: https://github.com/bluekitchen/btstack

[arduino_ble_library]: https://www.arduino.cc/reference/en/libraries/arduinoble/

[nano_rp2040]: https://store-usa.arduino.cc/products/arduino-nano-rp2040-connect-with-headers

[nano_33_iot]: https://store-usa.arduino.cc/products/arduino-nano-33-iot

[mkr_wifi]: https://store-usa.arduino.cc/products/arduino-mkr-wifi-1010

[uni_wifi]: https://store-usa.arduino.cc/products/arduino-uno-wifi-rev2

[mkr_vidor_4000]: https://store.arduino.cc/products/arduino-mkr-vidor-4000
