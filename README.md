# Bluepad32

[![discord](https://img.shields.io/discord/775177861665521725.svg)](https://discord.gg/r5aMn6Cw5q)

<p align="center">
<img src="https://github.com/ricardoquesada/bluepad32/blob/develop/docs/images/bluepad32_logo_ok_280.png?raw=true" alt="drawing" width="216"/>
</p>

A Bluetooth controller "host" for the ESP32, ESP32-S3, ESP32-C3, Raspberry Pi Pico W and Posix (Linux, macOS).

Add Bluetooth gamepad, mouse and keyboard support to your projects easily.

In other words, it allows you to control a robot using a DualSense controller.

## Where to start

Choose your target platform:

| Platform                            | Start here                                                        | Further info              | Community projects                                                                                        |
|-------------------------------------|-------------------------------------------------------------------|---------------------------|-----------------------------------------------------------------------------------------------------------|
| Arduino IDE                         | [![Watch the video][youtube_image]](https://youtu.be/0jnY-XXiD8Q) | [Doc][plat_arduino]       | [Controller for Tello drone][tello]                                                                       |
| Arduino using ESP-IDF toolchain     | [Template project][esp-idf-bluepad32-arduino]                     | [Doc][plat_arduino]       | [Lego Robot][esp32_example] ([video][esp32_video]), [gbaHD Shield][esp32_example2] (a GameBoy consolizer) |
| Arduino + NINA coprocessor          | [Arduino Library][bp32-arduino]                                   | [Doc][plat_nina]          | [Philips CD-i meets Bluetooth][nina_example]                                                              |
| CircuitPython + AirLift coprocessor | [CircuitPython Library][bp32-circuitpython]                       | [Doc][plat_airlift]       | [Quico console][airlift_example], Controlling 4 servos ([video][airlift_video])                           |
| Pico W                              | [Pico W example][pico-w-example]                                  | [Doc][plat_picow_picosdk] | [Pico Switch][pico_switch], [PicoNtrol][pico_ntrol]                                                       |
| ESP-IDF                             | [ESP32 example][esp32-example]                                    | [Doc][plat_esp32_espidf]  | [OGX-Wireless-Lite][ogx_wireless_lite]                                                                    |
| Posix (Linux, macOS)                | [Posix example][posix-example]                                    | [Doc][plat_custom]        |                                                                                                           |
| Unijoysticle                        | [Unijoysticle2][unijoysticle2]                                    | [Doc][plat_unijoysticle]  |                                                                                                           |
| MightyMiggy                         | [Unijoysticle for Amiga][unijoysticle_sukko]                      | [Doc][plat_mightymiggy]   |                                                                                                           |

[airlift_example]: https://gitlab.com/ricardoquesada/quico

[airlift_video]: https://twitter.com/makermelissa/status/1482596378282913793

[bp32-arduino]: https://github.com/ricardoquesada/bluepad32-arduino

[bp32-circuitpython]: https://github.com/ricardoquesada/bluepad32-circuitpython

[esp-idf-bluepad32-arduino]: https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

[esp32_example2]: https://github.com/ManCloud/GBAHD-Shield

[esp32_example]: https://github.com/antonvh/LMS-uart-esp/blob/main/Projects/LMS-ESP32/BluePad32_idf/README.md

[esp32_video]: https://www.instagram.com/p/Ca7T6twKZ0B/

[nina_example]: https://eyskens.me/cd-i-meets-bluetooth/

[ogx_wireless_lite]: https://github.com/wiredopposite/OGX-Wireless-Lite

[pico_ntrol]: https://github.com/ShadeReogen/PicoNtrol

[pico_switch]: https://github.com/juan518munoz/PicoSwitch-WirelessGamepadAdapter

[plat_airlift]: https://bluepad32.readthedocs.io/en/latest/plat_airlift

[plat_arduino]: https://bluepad32.readthedocs.io/en/latest/plat_arduino

[plat_custom]: https://bluepad32.readthedocs.io/en/latest/adding_new_platform

[plat_esp32_espidf]: https://bluepad32.readthedocs.io/en/latest/plat_esp32

[plat_mightymiggy]: https://bluepad32.readthedocs.io/en/latest/plat_mightymiggy

[plat_nina]: https://bluepad32.readthedocs.io/en/latest/plat_nina

[plat_unijoysticle]: https://bluepad32.readthedocs.io/en/latest/plat_unijoysticle

[plat_picow_picosdk]: https://bluepad32.readthedocs.io/en/latest/plat_picow

[tello]: https://github.com/jsolderitsch/ESP32Controller

[unijoysticle2]: https://retro.moe/unijoysticle2/

[unijoysticle_sukko]: https://gitlab.com/SukkoPera/unijoysticle2

[youtube_image]: https://lh3.googleusercontent.com/pw/AJFCJaXiDBy3NcQBBB-WFFVCsvYBs8szExsYQVwG5qqBTtKofjzZtJv_6GSL7_LfYRiypF1K0jjjgziXJuxAhoEawvzV84hlbmVTrGeXQYpVnpILZwWkbFi-ccX4lEzEbYXX-UbsEzpHLhO8qGVuwxOl7I_h1Q=-no?authuser=0

## Features

* Supports most, if not all, modern Bluetooth gamepads, mice and keyboards (see below)
* Supports ESP32 and Pico W
* Supported APIs: ESP-IDF, Pico-SDK, Arduino and CircuitPython
* Fast (very low latency)
* Small footprint
* Uses only one core (CPU0). The remaining one is free to use.
* C11 based
* Open Source (see below)
* Easy to integrate into 3rd party projects

## Supported controllers

![Supported gamepads](https://lh3.googleusercontent.com/pw/AMWts8BB7wT51jpn3HxWHuZLiEM2lX05gmTDsnldHszkXuYqxbowNvtxPtpbHh3CNjv1OBzeyadZjNLNBgE4w2tl2WmP8M9gGBCfWhzmZGQnHBlERSoy5W2dj6-EYmT84yteKTFjp4Jz2H3DgByFiKXaxfFC2g=-no)

* Sony DualSense (PS5)
* Sony DualShock 4 (PS4)
* Sony DualShock 3 (PS3)
* Nintendo Switch Pro controller
* Nintendo Switch JoyCon
* Nintendo Wii U controller
* Nintendo Wii Remote + accessories
* Xbox Wireless controller (models 1708, 1914, adaptive)
* Android controllers
* Steam controller
* Stadia controller
* PC/Window controller
* 8BitDo controllers
* Atari joystick
* iCade
* Mouse
* Keyboards
* And more

NOTE: Original **ESP32** and **Pico W** support all listed controllers. **ESP32-S3** and **ESP32-C3** support only a
subset.

See: [Supported gamepads][gamepads], [supported mice][mice] and [supported keyboards][keyboards]

[gamepads]: https://bluepad32.readthedocs.io/en/latest/supported_gamepads/

[mice]: https://bluepad32.readthedocs.io/en/latest/supported_mice/

[keyboards]: https://bluepad32.readthedocs.io/en/latest/supported_keyboards/

## Pre-compiled binaries

Download pre-compiled binaries for Unijoysticle, Nina, AirLift, MightyMiggy:

* https://github.com/ricardoquesada/bluepad32/releases

## Creating your project

See the examples folder which includes examples for:

* [Bluepad32 for ESP32][esp32-example] (ESP32, ESP32-S3, ESP32-C3)
* [Bluepad32 for Pico W][pico-w-example]
* [Bluepad32 for Posix (Linux, macOS)][posix-example]

Arduino examples are in:

* [Bluepad32 for Arduino IDE][arduino-ide-example]
* [Bluepad32 for Arduino + ESP-IDF][arduino-esp-idf-example]

[esp32-example]: examples/esp32/

[pico-w-example]: examples/pico_w/

[posix-example]: examples/posix

[arduino-ide-example]: https://www.youtube.com/watch?v=0jnY-XXiD8Q

[arduino-esp-idf-example]: https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

## Support

* [Documentation][docs] [![Documentation Status](https://readthedocs.org/projects/bluepad32/badge/?version=latest)](https://bluepad32.readthedocs.io/?badge=latest)
* [Discord][discord] [![discord](https://img.shields.io/discord/775177861665521725.svg)](https://discord.gg/r5aMn6Cw5q)

[docs]: https://bluepad32.readthedocs.io/

[discord]: https://discord.gg/r5aMn6Cw5q

## License

Bluepad32 is open source, [licensed under Apache 2][apache2].

However, Bluepad32 depends on the great [BTstack library][btstack-github]. Which is free to use for
open source projects. But commercial for closed-source projects.

If you are developing a commercial product for:

- ESP32: [You should contact BTstack people][btstack-homepage].
- Pico W: [You are already covered by Raspberry Pi License][rpi-btstack-license].

Notice: I’m not affiliated with BTstack people. They are super friendly and willing to help.

[btstack-github]: https://github.com/bluekitchen/btstack

[apache2]: https://www.apache.org/licenses/LICENSE-2.0

[btstack-homepage]: https://bluekitchen-gmbh.com/

[rpi-btstack-license]: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_btstack/LICENSE.RP
