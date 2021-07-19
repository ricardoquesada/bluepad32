# Bluepad32

[![discord](https://img.shields.io/discord/775177861665521725.svg)](https://discord.gg/r5aMn6Cw5q)

![logo](https://lh3.googleusercontent.com/pw/ACtC-3dNNrE9YKWMQNBTVYl8gkX70jN2qLwSYMQFLR0IzzoRT9uTQ1a9n80O3tyNmF95MLxL9NPWlqm5ph7e9wTGZoHeQWPMsJWqs3qiHub1LcigLtlEX09V6f1DWwQlg52OkeumKDJTG_ext8EN_J6kn0kAqg=-no)

A bluetooth gamepad "host" for the ESP32.

Add Bluetooth gamepad support to your projects easily.

* Use gamepads to play games in the Commodore 64 ([Unijoysticle2][unijoysticle2])
* Or play games in Commodore Amiga ([Unijoysticle2 for the Amiga][amiga])
* Or play games using [CircuitPython][bp32-circuitpython] in a [MatrixPortal M4][matrixportal] (or any Adafruit board with an AirLift co-processor)
* Or control a robot using [Arduino][bp32-arduino] in a [Nano RP2040 Connect][rp2040] (or any Arduino board with a NINA co-processor)
* Or play games in a FPGA console ([ULX3S][ulx3s])
* etc.

[unijoysticle2]: https://retro.moe/unijoysticle2/
[amiga]: https://gitlab.com/SukkoPera/unijoysticle2
[ulx3s]: https://www.crowdsupply.com/radiona/ulx3s
[matrixportal]: https://learn.adafruit.com/adafruit-matrixportal-m4
[rp2040]: https://docs.arduino.cc/hardware/nano-rp2040-connect
[bp32-arduino]: https://gitlab.com/ricardoquesada/bluepad32-arduino
[bp32-circuitpython]: https://gitlab.com/ricardoquesada/bluepad32-circuitpython

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

```
$ git clone https://gitlab.com/ricardoquesada/bluepad32.git
$ cd bluepad32
$ git submodule update --init
```

3. Integrate BTStack into ESP32

```
$ cd ${BLUEPAD32}/external/btstack/port/esp32
$ ./integrate_btstack.py
```

4. Compile Bluepad32

Choose target platform:

```
# Choose target platform: unijoysticle, airlift, nina, etc...
$ export PLATFORM=unijoysticle
```

And compile it:

```
$ cd ${BLUEPAD32}/src
$ make -j
```

5. Flash it

```
$ cd ${BLUEPAD32}/src
$ make flash monitor
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
