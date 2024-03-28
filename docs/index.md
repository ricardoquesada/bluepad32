# Bluepad32 documentation

<figure markdown="span">
  ![logo][bp32_logo]{ width="300" }
</figure>

Bluepad32 is a library that acts as a "host" Bluetooth controller for HID devices.

In other words, it allows you to control a robot using a DualSense controller.


[bp32_logo]: images/bluepad32_logo_ok_280.png

## Features

* Supports most, if not all, modern Bluetooth gamepads, mice and keyboards
* Supports ESP32 (ESP32, ESP32-S3, ESP32-C3) and Pico W microcontrollers
* Supported APIs: ESP-IDF, Pico-SDK, Arduino and CircuitPython
* Fast (very low latency)
* Small footprint
* Uses only one core (CPU0)
* C11 based
* Open Source
* Easy to integrate into 3rd party projects

## Supported controllers

Non-exhaustive list:

* Sony DualSense, DualShock 4, DualShock 3 :material-sony-playstation:
* Nintendo Switch, Wii U, Wii and Wii extensions :material-nintendo-switch:
* Xbox Wireless :material-microsoft-xbox-controller:
* Android :material-android:
* Keyboard
* Mouse
* and more

For detailed list see: [Supported gamepads][supported_gamepads] :material-gamepad-variant:,
[supported mice][supported_mice] :material-mouse: and [supported keyboards][supported_keyboards] :material-keyboard:.

[supported_gamepads]: supported_gamepads
[supported_mice]: supported_mice
[supported_keyboards]: supported_keyboards
