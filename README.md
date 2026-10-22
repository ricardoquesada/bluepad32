# Bluepad32

[![discord](https://img.shields.io/discord/775177861665521725.svg)](https://discord.gg/r5aMn6Cw5q)

<p align="center">
<img src="https://github.com/ricardoquesada/bluepad32/blob/develop/docs/images/bluepad32_logo_ok_280.png?raw=true" alt="drawing" width="216"/>
</p>

Bluepad32 is a Bluetooth controller host for ESP32, ESP32-S3, ESP32-C3,
ESP32-C5, ESP32-C6, ESP32-H2, Raspberry Pi Pico W, Pico 2 W, and POSIX (Linux,
macOS).

Use Bluepad32 to add Bluetooth gamepad, mouse, and keyboard support to your
projects. For example, you can control a robot with a DualSense controller.

## Where to start

Select your target platform:

| Platform                            | Start here                                                        | Further info              | Community projects                                                                                        |
|-------------------------------------|-------------------------------------------------------------------|---------------------------|-----------------------------------------------------------------------------------------------------------|
| Arduino IDE                         | [![Watch the video][youtube_image]](https://youtu.be/0jnY-XXiD8Q) | [Doc][plat_arduino]       | [Controller for Tello drone][tello]                                                                       |
| Arduino using ESP-IDF toolchain     | [Template project][esp-idf-bluepad32-arduino]                     | [Doc][plat_arduino]       | [Lego Robot][esp32_example] ([video][esp32_video]), [gbaHD Shield][esp32_example2] (a GameBoy consolizer) |
| Pico W                              | [Pico W example][pico-w-example]                                  | [Doc][plat_picow_picosdk] | [Pico Switch][pico_switch], [PicoNtrol][pico_ntrol]                                                       |
| ESP-IDF                             | [ESP32 example][esp32-example]                                    | [Doc][plat_esp32_espidf]  | [OGX-Wireless-Lite][ogx_wireless_lite]                                                                    |
| Posix (Linux, macOS)                | [Posix example][posix-example], [Posix ImGui][posix-imgui-example]| [Doc][plat_custom]        |                                                                                                           |
| Unijoysticle                        | [Unijoysticle2][unijoysticle2]                                    | [Doc][plat_unijoysticle]  |                                                                                                           |

[esp-idf-bluepad32-arduino]: https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

[esp32_example2]: https://github.com/ManCloud/GBAHD-Shield

[esp32_example]: https://github.com/antonvh/LMS-uart-esp/blob/main/Projects/LMS-ESP32/BluePad32_idf/README.md

[esp32_video]: https://www.instagram.com/p/Ca7T6twKZ0B/

[ogx_wireless_lite]: https://github.com/wiredopposite/OGX-Wireless-Lite

[pico_ntrol]: https://github.com/ShadeReogen/PicoNtrol

[pico_switch]: https://github.com/juan518munoz/PicoSwitch-WirelessGamepadAdapter

[plat_arduino]: https://bluepad32.readthedocs.io/en/latest/plat_arduino

[plat_custom]: https://bluepad32.readthedocs.io/en/latest/adding_new_platform

[plat_esp32_espidf]: https://bluepad32.readthedocs.io/en/latest/plat_esp32

[plat_unijoysticle]: https://bluepad32.readthedocs.io/en/latest/plat_unijoysticle

[plat_picow_picosdk]: https://bluepad32.readthedocs.io/en/latest/plat_picow

[tello]: https://github.com/jsolderitsch/ESP32Controller

[unijoysticle2]: https://retro.moe/unijoysticle2/

[youtube_image]: https://lh3.googleusercontent.com/pw/AJFCJaXiDBy3NcQBBB-WFFVCsvYBs8szExsYQVwG5qqBTtKofjzZtJv_6GSL7_LfYRiypF1K0jjjgziXJuxAhoEawvzV84hlbmVTrGeXQYpVnpILZwWkbFi-ccX4lEzEbYXX-UbsEzpHLhO8qGVuwxOl7I_h1Q=-no?authuser=0

## Features

* Supports modern Bluetooth gamepads, mice, and keyboards
* Supports ESP32 and Pico W microcontroller families
* Supports ESP-IDF, Pico SDK, and Arduino APIs
* Delivers low latency and a small memory footprint
* Runs on a single core (`CPU0`) and leaves the second core free
* Uses standard C11
* Provides an open-source license
* Integrates into third-party projects

## Supported controllers

![Supported gamepads](https://lh3.googleusercontent.com/pw/AMWts8BB7wT51jpn3HxWHuZLiEM2lX05gmTDsnldHszkXuYqxbowNvtxPtpbHh3CNjv1OBzeyadZjNLNBgE4w2tl2WmP8M9gGBCfWhzmZGQnHBlERSoy5W2dj6-EYmT84yteKTFjp4Jz2H3DgByFiKXaxfFC2g=-no)

* Sony DualSense (PS5)
* Sony DualShock 4 (PS4)
* Sony DualShock 3 (PS3)
* Nintendo Switch Pro controller
* Nintendo Switch Joy-Con
* Nintendo Wii U controller
* Nintendo Wii Remote and accessories
* Xbox Wireless controller (models 1708, 1914, Adaptive)
* Android controllers
* Steam controller
* Stadia controller
* PC and Windows controllers
* 8BitDo controllers
* Atari joystick
* iCade
* Mice
* Keyboards

NOTE: The original **ESP32**, **Pico W**, and **Pico 2 W** support all listed
controllers. **ESP32-S3**, **ESP32-C3**, **ESP32-C5**, **ESP32-C6**, and
**ESP32-H2** support only a subset of controllers.

See [Supported gamepads][gamepads], [supported mice][mice], and
[supported keyboards][keyboards].

[gamepads]: https://bluepad32.readthedocs.io/en/latest/supported_gamepads/

[mice]: https://bluepad32.readthedocs.io/en/latest/supported_mice/

[keyboards]: https://bluepad32.readthedocs.io/en/latest/supported_keyboards/

## Pre-compiled binaries

Download pre-compiled binaries for Unijoysticle from the releases page:

* https://github.com/ricardoquesada/bluepad32/releases

## Create your project

The `examples` folder includes sample projects for these platforms:

* [Bluepad32 for ESP32][esp32-example] (ESP32, ESP32-S3, ESP32-C3, ESP32-C5,
    ESP32-C6, and ESP32-H2)
* [Bluepad32 for Pico W][pico-w-example] (Pico W and Pico 2 W)
* [Bluepad32 for Posix (Linux, macOS)][posix-example]
* [Bluepad32 Posix ImGui Diagnostic Suite][posix-imgui-example]

Find the Arduino examples at these links:

* [Bluepad32 for Arduino IDE][arduino-ide-example]
* [Bluepad32 for Arduino + ESP-IDF][arduino-esp-idf-example]

[esp32-example]: examples/esp32/

[pico-w-example]: examples/pico_w/

[posix-example]: examples/posix
[posix-imgui-example]: examples/posix_imgui

[arduino-ide-example]: https://www.youtube.com/watch?v=0jnY-XXiD8Q

[arduino-esp-idf-example]: https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

## Support

* [Documentation][docs] [![Documentation Status](https://readthedocs.org/projects/bluepad32/badge/?version=latest)](https://bluepad32.readthedocs.io/?badge=latest)
    * [Linux / macOS (POSIX)](docs/plat_posix.md)
* [Discord][discord] [![discord](https://img.shields.io/discord/775177861665521725.svg)](https://discord.gg/r5aMn6Cw5q)

[docs]: https://bluepad32.readthedocs.io/

[discord]: https://discord.gg/r5aMn6Cw5q

## License

Bluepad32 is open-source software under the [Apache 2.0 License][apache2].

Bluepad32 depends on the [BTstack library][btstack-github]. BTstack is free for
open-source projects, but requires a commercial license for closed-source
projects.

If you develop a commercial product, follow these licensing rules:

- ESP32: [Contact BlueKitchen][btstack-homepage] for a commercial license.
- Pico W: The [Raspberry Pi License][rpi-btstack-license] covers commercial
    use.

Note: Bluepad32 has no affiliation with BlueKitchen.

[btstack-github]: https://github.com/bluekitchen/btstack

[apache2]: https://www.apache.org/licenses/LICENSE-2.0

[btstack-homepage]: https://bluekitchen-gmbh.com/

[rpi-btstack-license]: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_btstack/LICENSE.RP
