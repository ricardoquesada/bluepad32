# Bluepad32 documentation

<figure markdown="span">
  ![logo][bp32_logo]{ width="300" }
</figure>

Bluepad32 is a Bluetooth host library for HID controllers. For example, you can
use a DualSense controller to control a robot.

[bp32_logo]: images/bluepad32_logo_ok_280.png

## Features

* Supports modern Bluetooth gamepads, mice, and keyboards
* Supports ESP32 and Pico W microcontroller families
* Supports ESP-IDF, Pico SDK, and Arduino APIs
* Delivers low latency and a small memory footprint
* Runs on a single core (`CPU0`)
* Uses standard C11
* Provides an open-source license
* Integrates into third-party projects

## Supported controllers

Bluepad32 supports the following controllers:

* Sony DualSense, DualShock 4, and DualShock 3 :material-sony-playstation:
* Nintendo Switch, Wii U, Wii, and Wii extensions :material-nintendo-switch:
* Xbox Wireless :material-microsoft-xbox-controller:
* Android :material-android:
* Keyboards
* Mice

For the complete list, see [Supported gamepads][supported_gamepads] :material-gamepad-variant:,
[supported mice][supported_mice] :material-mouse:, and
[supported keyboards][supported_keyboards] :material-keyboard:.

[supported_gamepads]: supported_gamepads
[supported_mice]: supported_mice
[supported_keyboards]: supported_keyboards
