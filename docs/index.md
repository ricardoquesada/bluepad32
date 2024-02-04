# Bluepad32 documentation

![logo][bluepad32_logo]

[bluepad32_logo]: images/bluepad32-logo.png

Bluepad32 is a library that acts as a "host" Bluetooth controller for HID devices.

In other words, it allows gamepads, keyboards and mice to connect to your project.

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

* DualSense
* DualShock 4
* DualShock 3
* Xbox Wireless
* Nintendo Switch
* Nintendo Wii U
* Nintendo Wii and extensions like: Wii Mote, Balance Board, Nunchuk, etc.
* Android
* Steam
* Stadia
* 8Bitdo
* Keyboard
* Mouse
* and more

If your favorite controller is not supported, [let us know][filing_a_bug]


[filing_a_bug]: https://github.com/ricardoquesada/bluepad32/issues
