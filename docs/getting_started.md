# Choose platform

Bluepad32 supports different platforms, and different microcontrollers.

Choose the right one depending on your knowledge, devkits and requirements:

=== "Arduino"

    For Arduino developers, there are three options.

    1. Using Arduino IDE with an ESP32 devkit

        - Works with any [ESP32][amazon_esp32_devkit], [ESP32-S3][amazon_esp32_s3_devkit] or [ESP32-C3][amazon_esp32_c3_devkit] devkit
        - WiFi API available
        - Bluetooth:
             - :material-check: Available by using the [BTstack API][btstack].
             - :material-close: DO NOT use [Arduino BLE library][arduino_ble_library]. It won't work.
        - Easy to setup, easy to use, easy to update.
        - Recommended for beginners and existing Arduino users

        <iframe width="560" height="315" src="https://www.youtube.com/embed/0jnY-XXiD8Q?si=YphWYgQf0a1YX_nq" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" allowfullscreen></iframe>

        !!! Bug
            [Arduino Nano ESP32][arduino_nano_esp32] board is not supported at the moment.
            See [Github issue #65][github_issue_65] for status.


    2. Using Arduino Core (not the IDE) with ESP-IDF toolchain

    3. Using Arduino IDE with an Arduino NINA board

        !!! Warning

            Only use if the only thing you have is an Arduino NINA board.

=== "CircuitPython"

    Hola

=== "ESP-IDF"

    Hola

=== "Pico SDK"

    Hola

| Platform                            | Start here                                                        | Further info        | Community projects                                                                                        | Features                                                     |
|-------------------------------------|-------------------------------------------------------------------|---------------------|-----------------------------------------------------------------------------------------------------------|--------------------------------------------------------------|
| Arduino IDE                         | [![Watch the video][youtube_image]](https://youtu.be/0jnY-XXiD8Q) | [Doc][plat_arduino] | [Controller for Tello drone][tello]                                                                       | Easy to debug, WiFi / BT, familiar IDE, Arduino libraries    |
| Arduino using ESP-IDF toolchain     | [Template project][esp-idf-bluepad32-arduino]                     | [Doc][plat_arduino] | [Lego Robot][esp32_example] ([video][esp32_video]), [gbaHD Shield][esp32_example2] (a GameBoy consolizer) | Very easy to debug, console, WiFi / BT, Arduino libraries    |
| Arduino + NINA coprocessor          | [Arduino Library][bp32-arduino]                                   | [Doc][plat_nina]    | [Philips CD-i meets Bluetooth][nina_example]                                                              | Difficult to debug, familiar IDE, Arduino libraries          |
| CircuitPython + AirLift coprocessor | [CircuitPython Library][bp32-circuitpython]                       | [Doc][plat_airlift] | [Quico console][airlift_example], Controlling 4 servos ([video][airlift_video])                           | Difficult to debug, easy to program, CircuitPython libraries |
| Pico W                              | [Pico W example][pico-w-example]                                  | [Doc][plat_custom]  | [Pico Switch][pico_switch]                                                                                | Very easy to debug, WiFi / BT, for advanced developers       |
| ESP-IDF                             | [ESP32 example][esp32-example]                                    | [Doc][plat_custom]  |                                                                                                           | Very easy to debug, WiFi / BT, for advanced developers       |
| Linux                               | [Linux example][linux-example]                                    | [Doc][plat_custom]  |                                                                                                           | Very easy to debug, WiFi / BT, useful for quick development  | 

[airlift_example]: https://gitlab.com/ricardoquesada/quico

[airlift_video]: https://twitter.com/makermelissa/status/1482596378282913793

[arduino-esp-idf-example]: https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

[arduino-ide-example]: https://www.youtube.com/watch?v=0jnY-XXiD8Q

[bp32-arduino]: https://github.com/ricardoquesada/bluepad32-arduino

[bp32-circuitpython]: https://github.com/ricardoquesada/bluepad32-circuitpython

[esp-idf-bluepad32-arduino]: https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

[esp32-example]: examples/esp32/

[esp32_example2]: https://github.com/ManCloud/GBAHD-Shield

[esp32_example]: https://github.com/antonvh/LMS-uart-esp/blob/main/Projects/LMS-ESP32/BluePad32_idf/README.md

[esp32_video]: https://www.instagram.com/p/Ca7T6twKZ0B/

[linux-example]: examples/linux

[nina_example]: https://eyskens.me/cd-i-meets-bluetooth/

[pico-w-example]: examples/pico_w/

[pico_switch]: https://github.com/juan518munoz/PicoSwitch-WirelessGamepadAdapter

[plat_airlift]: docs/plat_airlift.md

[plat_arduino]: docs/plat_arduino.md

[plat_custom]: docs/adding_new_platform.md

[plat_mightymiggy]: docs/plat_mightymiggy.md

[plat_nina]: docs/plat_nina.md

[plat_unijoysticle]: docs/plat_unijoysticle.md

[tello]: https://github.com/jsolderitsch/ESP32Controller

[youtube_image]: https://lh3.googleusercontent.com/pw/AJFCJaXiDBy3NcQBBB-WFFVCsvYBs8szExsYQVwG5qqBTtKofjzZtJv_6GSL7_LfYRiypF1K0jjjgziXJuxAhoEawvzV84hlbmVTrGeXQYpVnpILZwWkbFi-ccX4lEzEbYXX-UbsEzpHLhO8qGVuwxOl7I_h1Q=-no?authuser=0

[github_issue_65]: https://github.com/ricardoquesada/bluepad32/issues/65

[arduino_nano_esp32]: https://store-usa.arduino.cc/products/nano-esp32

[amazon_esp32_devkit]: https://www.amazon.com/s?k=esp32+devkit

[amazon_esp32_s3_devkit]: https://www.amazon.com/s?k=esp32-s3+devkit

[amazon_esp32_c3_devkit]: https://www.amazon.com/s?k=esp32-c3+devkit

[btstack]: https://github.com/bluekitchen/btstack

[arduino_ble_library]: https://www.arduino.cc/reference/en/libraries/arduinoble/
