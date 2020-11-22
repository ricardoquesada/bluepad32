# Bluepad32

![logo](https://lh3.googleusercontent.com/pw/ACtC-3dNNrE9YKWMQNBTVYl8gkX70jN2qLwSYMQFLR0IzzoRT9uTQ1a9n80O3tyNmF95MLxL9NPWlqm5ph7e9wTGZoHeQWPMsJWqs3qiHub1LcigLtlEX09V6f1DWwQlg52OkeumKDJTG_ext8EN_J6kn0kAqg=-no)

A bluetooth gamepad "host" for the ESP32.

Modern Bluetooth gamepads, like the Xbox One and DS4, can connect to the ESP32.
What you do with the ESP32, is up to you. But here are some examples:

* Use gamepads to play games in the Commodore 64 ([Unijoysticle2][unijoysticle2])
* Use it to play games in a FPGA console ([ULX3S][ulx3s])
* Play games in [MatrixPortal M4][maxtrixportal] (or any AirLift board)
* Control your LED lamp
* Control your remote car
* Add it to your own IoT/smart-home project
* And so on

[unijoysticle2]: https://retro.moe/unijoysticle2/
[ulx3s]: https://www.crowdsupply.com/radiona/ulx3s
[matrixportal]: https://learn.adafruit.com/adafruit-matrixportal-m4


## Supported controllers

![Supported_gamepads](https://lh3.googleusercontent.com/U1PRr4a21yGffPHxRlONqeolOnr2i-IuONM4ajQksvxB5Lr3zfQFmkHJJbwRNVUY0WrNik5Ia79se3sQx0aa4axuGnBbytyH_5fJnKELX4FOMRM4qrF3bYCmmp0Vk3ZnltQ0YCiRTK0=-no)

- Sony DUALSHOCK 4
- Sony DUALSHOCK 3
- Xbox One S
- Nintendo Switch Pro
- Nintendo Wii U
- Nintendo Wii
- Android gamepads
- PC/Window gamepads
- 8BitDo
- iCade
- And more

See: [Supported gamepads][gamepads]

[gamepads]: https://gitlab.com/ricardoquesada/bluepad32/blob/master/docs/supported_gamepads.md

## How to compile it

1. Install ESP-IDF

Install the ESP32 toolchain. Use version **4.1**. Might work on newer / older
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
$ cd ${BLUEPAD32}/external/btstack/ports/esp32
$ ./integrate_btstack.py
```

4. Compile Bluepad32

Choose target platform:

```
# Choose target platform: unijoysticle, airlift, etc...
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

- [Discord][discord]
- [Google groups][forum]

[discord]: https://discord.com/channels/775177861665521725/775177925938642945
[forum]: https://groups.google.com/forum/#!forum/unijoysticle
