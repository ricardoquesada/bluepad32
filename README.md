# Bluepad32

[![discord](https://img.shields.io/discord/775177861665521725.svg)](https://discord.gg/r5aMn6Cw5q)

![logo](https://lh3.googleusercontent.com/pw/ACtC-3dNNrE9YKWMQNBTVYl8gkX70jN2qLwSYMQFLR0IzzoRT9uTQ1a9n80O3tyNmF95MLxL9NPWlqm5ph7e9wTGZoHeQWPMsJWqs3qiHub1LcigLtlEX09V6f1DWwQlg52OkeumKDJTG_ext8EN_J6kn0kAqg=-no)

A Bluetooth gamepad "host" for the ESP32.

Add Bluetooth gamepad support to your projects easily. Supported platforms:

* Arduino + ESP32 boards:
  [further info][plat_arduino], [template project][esp-idf-bluepad32-arduino]
* Arduino + boards with the NINA coprocessor:
  [further info][plat_nina], [library][bp32-arduino]
* CircuitPython + boards with the AirLift coprocessor:
  [further info][plat_airlift], [library][bp32-circuitpython]
* Or in custom boards like:
  * [Unijoysticle v2, to play games on the C64][unijoysticle2]
  * [Unijoysticle v2 for the Amiga][amiga]
  * [ULX3S][ulx3s], an FPGA console

[amiga]: https://gitlab.com/SukkoPera/unijoysticle2
[bp32-arduino]: https://gitlab.com/ricardoquesada/bluepad32-arduino
[bp32-circuitpython]: https://gitlab.com/ricardoquesada/bluepad32-circuitpython
[esp-idf-bluepad32-arduino]: https://gitlab.com/ricardoquesada/esp-idf-arduino-bluepad32-template
[plat_airlift]: docs/plat_airlift.md
[plat_arduino]: docs/plat_arduino.md
[plat_nina]: docs/plat_nina.md
[ulx3s]: https://www.crowdsupply.com/radiona/ulx3s
[unijoysticle2]: https://retro.moe/unijoysticle2/

## Features

* Supports most, if not all, modern Bluetooth gamepads (see below)
* Fast (very low latency)
* Small footprint
* Uses only one core (CPU0). The remaining one is free to use.
* C99 based
* Open Source (see below)

## Supported controllers

![Supported gamepads](https://lh3.googleusercontent.com/pw/ACtC-3cg22O7VPT8NwXIATr2rsgs-rn2kShZeiUbArIK-2lIkskCLI6q06nRtK9been8Hom49dOacwHD8bVT2Tc8YKsxd5w73W25lhOvlRk6Xf9RVXgB5AZcmdl2PoWhrEAZUbmBl1pS6HrtMuZYI506US7YuA=-no)

* Sony DualSense (PS5)
* Sony DUALSHOCK 4 (PS4)
* Sony DUALSHOCK 3 (PS3)
* Xbox One S
* Nintendo Switch Pro
* Nintendo Wii U
* Nintendo Wii
* Android gamepads
* PC/Window gamepads
* 8BitDo
* iCade
* And more

See: [Supported gamepads][gamepads]

[gamepads]: https://gitlab.com/ricardoquesada/bluepad32/blob/master/docs/supported_gamepads.md

## How to compile it

1. Install ESP-IDF

    Install the ESP32 toolchain. Use version **4.2**. Might work on newer / older
    ones, but not tested.

    * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

2. Clone repo

   ```sh
   git clone --recursive https://gitlab.com/ricardoquesada/bluepad32.git
   ```

3. Integrate BTStack into ESP32

   ```sh
   cd ${BLUEPAD32}/external/btstack/port/esp32
   ./integrate_btstack.py
   ```

4. Compile Bluepad32

    Choose target platform (default is Unijoysticle):

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
