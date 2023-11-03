# Bluepad32

[![discord](https://img.shields.io/discord/775177861665521725.svg)](https://discord.gg/r5aMn6Cw5q)

![logo](https://lh3.googleusercontent.com/pw/ACtC-3dNNrE9YKWMQNBTVYl8gkX70jN2qLwSYMQFLR0IzzoRT9uTQ1a9n80O3tyNmF95MLxL9NPWlqm5ph7e9wTGZoHeQWPMsJWqs3qiHub1LcigLtlEX09V6f1DWwQlg52OkeumKDJTG_ext8EN_J6kn0kAqg=-no)

A Bluetooth gamepad "host" for the ESP32 / ESP32-S3 / ESP32-C3.

Add Bluetooth gamepad support to your projects easily.

## Where to start

Choose your target platform:

| Platform          | Start here | Further info | Community projects |
| ----------------- | ---------- | ------------ | ------------------ |
| Arduino IDE       | [![Watch the video][youtube_image]](https://youtu.be/0jnY-XXiD8Q) | [Doc][plat_arduino] | [Controller for Tello drone][tello] |
| Arduino using ESP-IDF toolchain | [Template project][esp-idf-bluepad32-arduino] | [Doc][plat_arduino] | [Lego Robot][esp32_example] ([video][esp32_video]), [gbaHD Shield][esp32_example2] (a GameBoy consolizer) |
| Arduino + NINA coprocessor      | [Arduino Library][bp32-arduino] | [Doc][plat_nina] | [Philips CD-i meets Bluetooth][nina_example] |
| CircuitPython + AirLift coprocessor | [CircuitPython Library][bp32-circuitpython] | [Doc][plat_airlift] | [Quico console][airlift_example], Controlling 4 servos ([video][airlift_video]) |
| Unijoysticle      | [Unijoysticle2][unijoysticle2]| [Doc][plat_unijoysticle] | |
| MightyMiggy       | [Unijoysticle for Amiga][unijoysticle_sukko] | [Doc][plat_mightymiggy] | |
| Custom            | | [Doc][plat_custom] | |


[airlift_example]: https://gitlab.com/ricardoquesada/quico
[airlift_video]: https://twitter.com/makermelissa/status/1482596378282913793
[bp32-arduino]: https://gitlab.com/ricardoquesada/bluepad32-arduino
[bp32-circuitpython]: https://gitlab.com/ricardoquesada/bluepad32-circuitpython
[esp32_example]: https://github.com/antonvh/LMS-uart-esp/blob/main/Projects/LMS-ESP32/BluePad32_idf/README.md
[esp32_example2]: https://github.com/ManCloud/GBAHD-Shield
[esp32_video]: https://www.instagram.com/p/Ca7T6twKZ0B/
[esp-idf-bluepad32-arduino]: https://gitlab.com/ricardoquesada/esp-idf-arduino-bluepad32-template
[nina_example]: https://eyskens.me/cd-i-meets-bluetooth/
[plat_airlift]: docs/plat_airlift.md
[plat_arduino]: docs/plat_arduino.md
[plat_custom]: docs/adding_new_platform.md
[plat_mightymiggy]: docs/plat_mightymiggy.md
[plat_nina]: docs/plat_nina.md
[plat_unijoysticle]: docs/plat_unijoysticle.md
[tello]: https://github.com/jsolderitsch/ESP32Controller
[unijoysticle2]: https://retro.moe/unijoysticle2/
[unijoysticle_sukko]: https://gitlab.com/SukkoPera/unijoysticle2
[youtube_image]: https://lh3.googleusercontent.com/pw/AJFCJaXiDBy3NcQBBB-WFFVCsvYBs8szExsYQVwG5qqBTtKofjzZtJv_6GSL7_LfYRiypF1K0jjjgziXJuxAhoEawvzV84hlbmVTrGeXQYpVnpILZwWkbFi-ccX4lEzEbYXX-UbsEzpHLhO8qGVuwxOl7I_h1Q=-no?authuser=0

## Features

* Supports most, if not all, modern Bluetooth gamepads and mice (see below)
* Fast (very low latency)
* Small footprint
* Uses only one core (CPU0). The remaining one is free to use.
* C11 based
* Open Source (see below)

## Supported controllers

![Supported gamepads](https://lh3.googleusercontent.com/pw/AMWts8BB7wT51jpn3HxWHuZLiEM2lX05gmTDsnldHszkXuYqxbowNvtxPtpbHh3CNjv1OBzeyadZjNLNBgE4w2tl2WmP8M9gGBCfWhzmZGQnHBlERSoy5W2dj6-EYmT84yteKTFjp4Jz2H3DgByFiKXaxfFC2g=-no)

* Sony DualSense (PS5)
* Sony DUALSHOCK 4 (PS4)
* Sony DUALSHOCK 3 (PS3)
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
* iCade
* Mouse
* Keyboards
* And more

NOTE: Original **ESP32** supports all listed controllers. **ESP32-S3** and **ESP32-C3** support only a subset.

See: [Supported gamepads][gamepads], [supported mice][mice] and [supported keyboards][keyboards]

[gamepads]: https://gitlab.com/ricardoquesada/bluepad32/blob/master/docs/supported_gamepads.md
[mice]: https://gitlab.com/ricardoquesada/bluepad32/blob/master/docs/supported_mice.md
[keyboards]: https://gitlab.com/ricardoquesada/bluepad32/blob/master/docs/supported_keyboards.md

## Pre-compiled binaries

* https://gitlab.com/ricardoquesada/bluepad32/-/releases

## How to compile it

1. Install ESP-IDF

    Install the ESP32 toolchain. Use version **4.4** or **5.0**. Might work on newer / older
    ones, but not tested.

    * <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/>

2. Clone repo

   ```sh
   git clone --recursive https://gitlab.com/ricardoquesada/bluepad32.git
   ```

3. Integrate BTStack into ESP32

   ```sh
   cd ${BLUEPAD32}/external/btstack/port/esp32
   # This will install BTstack as a component inside Bluepad32 source code (recommended).
   # Remove "IDF_PATH=../../../../src" if you want it installed in the ESP-IDF folder
   IDF_PATH=../../../../src ./integrate_btstack.py
   ```

4. Compile Bluepad32

    Choose target platform (default is *Unijoysticle*):

    ```sh
    cd ${BLUEPAD32}/src
    idf.py menuconfig
    ```

    The Bluepad32 options are in:
    `Components config` -> `Bluepad32` (find it at the very bottom) -> `Target platform`

    And compile it:

    ```sh
    idf.py build
    ```

5. Flash it

    ```sh
    idf.py flash monitor
    ```

## Support

* [Documentation][docs]
* [Discord][discord]

[docs]: https://gitlab.com/ricardoquesada/bluepad32/-/tree/master/docs
[discord]: https://discord.gg/r5aMn6Cw5q

## License

Bluepad32 is open source, [licensed under Apache 2][apache2].

However, Bluepad32 depends on the great [BTStack library][btstack-github]. Which is free to use for
open source projects. But commercial for closed-source projects.
[Contact them for details][btstack-homepage]. They are very friendly + helpful
(I’m not affiliated with them).

[btstack-github]: https://github.com/bluekitchen/btstack
[apache2]: https://www.apache.org/licenses/LICENSE-2.0
[btstack-homepage]: https://bluekitchen-gmbh.com/
