# Bluepad32

[![discord](https://img.shields.io/discord/775177861665521725.svg)](https://discord.gg/r5aMn6Cw5q)

![logo](https://lh3.googleusercontent.com/pw/ACtC-3dNNrE9YKWMQNBTVYl8gkX70jN2qLwSYMQFLR0IzzoRT9uTQ1a9n80O3tyNmF95MLxL9NPWlqm5ph7e9wTGZoHeQWPMsJWqs3qiHub1LcigLtlEX09V6f1DWwQlg52OkeumKDJTG_ext8EN_J6kn0kAqg=-no)

A Bluetooth gamepad "host" for the ESP32.

Add Bluetooth gamepad support to your projects easily. Supported platforms:

* Arduino + ESP32 boards:
  * [Documentation][plat_arduino]
  * [Template project][esp-idf-bluepad32-arduino] (🠐 START HERE)
  * From the community: [Lego Robot][esp32_example] ([video][esp32_video]), [gbaHD Shield][esp32_example2] (a GameBoy consolizer)
* Arduino + boards with the NINA coprocessor:
  * [Documentation][plat_nina]
  * [Library][bp32-arduino]
  * From the community: [Philips CD-i meets Bluetooth][nina_example]
* CircuitPython + boards with the AirLift coprocessor:
  * [Documentation][plat_airlift]
  * [Library][bp32-circuitpython]
  * From the community: [Quico console][airlift_example], Controlling 4 servos ([video][airlift_video])
* Or in custom boards like:
  * [Unijoysticle v2, to play games on the Commodore 64][unijoysticle2]
  * [or on the Commodore Amiga][amiga]
  * [ULX3S][ulx3s], an FPGA console

Note: Only the original ESP32 is supported, which supports Bluetooth Classic.
Newer models like ESP32-Sx, ESP32-Cx, ESP32-Hx are **NOT** supported since they don't support Bluetooth Classic.

[airlift_example]: https://gitlab.com/ricardoquesada/quico
[airlift_video]: https://twitter.com/makermelissa/status/1482596378282913793
[amiga]: https://gitlab.com/SukkoPera/unijoysticle2
[bp32-arduino]: https://gitlab.com/ricardoquesada/bluepad32-arduino
[bp32-circuitpython]: https://gitlab.com/ricardoquesada/bluepad32-circuitpython
[esp32_example]: https://github.com/antonvh/LMS-uart-esp/blob/main/Projects/BluePad32_idf/README.md
[esp32_example2]: https://github.com/ManCloud/GBAHD-Shield
[esp32_video]: https://www.instagram.com/p/Ca7T6twKZ0B/
[esp-idf-bluepad32-arduino]: https://gitlab.com/ricardoquesada/esp-idf-arduino-bluepad32-template
[nina_example]: https://eyskens.me/cd-i-meets-bluetooth/
[plat_airlift]: docs/plat_airlift.md
[plat_arduino]: docs/plat_arduino.md
[plat_nina]: docs/plat_nina.md
[ulx3s]: https://www.crowdsupply.com/radiona/ulx3s
[unijoysticle2]: https://retro.moe/unijoysticle2/

## Features

* Supports most, if not all, modern Bluetooth gamepads and mice (see below)
* Fast (very low latency)
* Small footprint
* Uses only one core (CPU0). The remaining one is free to use.
* C11 based
* Open Source (see below)

## Supported controllers

![Supported gamepads](https://lh3.googleusercontent.com/pw/AM-JKLXpmyDvNXZ_LmlmBSYObRZDhwuY6hHXXBzAicFw1YH1QNSgZrpiPWXZMiPNM0ATgrockqGf5bLsI3fWceJtQQEj2_OroHs1SrxsgmS8Rh4XHlnFolchomsTPVC7o5zi4pXGQkhGEFbinoh3-ub_a4lQIw=-no)

* Sony DualSense (PS5)
* Sony DUALSHOCK 4 (PS4)
* Sony DUALSHOCK 3 (PS3)
* Nintendo Switch Pro controller
* Nintendo Switch JoyCon
* Nintendo Wii U controller
* Nintendo Wii Remote + accesories
* Xbox Wireless controller (model 1708)
* Android gamepads
* PC/Window gamepads
* 8BitDo controllers
* iCade
* Mouse
* And more

See: [Supported gamepads][gamepads] and [supported mice][mice]

[gamepads]: https://gitlab.com/ricardoquesada/bluepad32/blob/master/docs/supported_gamepads.md
[mice]: https://gitlab.com/ricardoquesada/bluepad32/blob/master/docs/supported_mice.md

## How to compile it

1. Install ESP-IDF

    Install the ESP32 toolchain. Use version **4.4.1**. Might work on newer / older
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
* [Google groups][forum]

[docs]: https://gitlab.com/ricardoquesada/bluepad32/-/tree/master/docs
[discord]: https://discord.gg/r5aMn6Cw5q
[forum]: https://groups.google.com/forum/#!forum/unijoysticle

## License

Bluepad32 is open source, [licensed under Apache 2][apache2].

However Bluepad32 depends on the great [BTStack library][btstack-github]. Which is free to use for
open source projects. But commercial for closed-source projects.
[Contact them for details][btstack-homepage]. They are very friendly + helpful
(I’m not affiliated with them).

[btstack-github]: https://github.com/bluekitchen/btstack
[apache2]: https://www.apache.org/licenses/LICENSE-2.0
[btstack-homepage]: https://bluekitchen-gmbh.com/
