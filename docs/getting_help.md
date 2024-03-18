# Getting help

!!! Note

    If you've been asked to read this page, your question/bug is missing information.
    Read the entire page.


Something not working as expected?

* Post questions on [Discord server][discord_server] :simple-discord:
* File bugs / feature requests in [Github Issue Tracker][github_issue_tracker] :simple-github:

Add the following info:

* Bluepad32 platform you are using, and version.
* Devkit/board you are using. Add a link if possible
* Controller you are using, including the controller firmware version. Add a link if possible.
* Sketch you are using.
* Host Operating System you are using
* Debug log

[github_issue_tracker]: https://github.com/ricardoquesada/bluepad32/issues

## Platforms

Bluepad32 supports different platforms, like:

* Arduino IDE for ESP32 modules
* Arduino IDE for NINA coprocessors, like the Arduino Nano RP2040 Connect
* Arduino + ESP-IDF
* CircuitPython for AirLift coprocessors
* ESP-IDF
* Pico SDK
* Unijoysticle
* MightyMiggy

So, provide the platform you are using, and the version. And also which Bluepad32 version!

E.g: I'm using Arduino IDE v2.0.9 with Bluepad32 v3.7.3

## DevKit Module

Not all devkits are created equal.

Which devkkit are you using? Provide a link to the device.

E.g: I'm using Sparkfun Thing Plus - ESP32 WROOM: <https://www.sparkfun.com/products/17381>

## Controllers

Not all controllers behave the same way... some are reliable... some not.

The officially supported controllers are enumerated here:

* [Supported gamepads][supported_gamepads] :material-gamepad-variant:
* [Supported mice][supported_mice] :material-mouse:
* [Supported keyboards][supported_keyboards] :material-keyboard:

Please provide: brand, model, firmware and link to the device (if applicable).
Also mention whether it is genuine or a knock-off.

E.g.: I'm using a genuine Nintendo Switch JoyCon using FW 3.72.

If you are using a controller not listed there, YOU ARE ON YOUR OWN!

### Supporting unsupported controllers

If you really want us to support a controller that is not supported, contact us (Send us a Private Message in
[Discord][discord_server] :simple-discord:, or [file a feature request][github_bug] :simple-github: ). The way it works is:

* You send us a link to the controller that is not supported, like the Amazon or AliExpress link.
* You send us [via PayPal][paypal] :simple-paypal: the cost of the gamepad + shipping to the US.
* We purchase it, and we will do our best to support it. But we don't guarantee anything. So many things can go
  wrong, especially with low-cost clones where many features are not implemented.

[supported_gamepads]: ../supported_gamepads/
[supported_mice]: ../supported_mice/
[supported_keyboards]: ../supported_keyboards/
[paypal]: https://www.paypal.com/paypalme/RicardoQuesada
[github_bug]: https://github.com/ricardoquesada/bluepad32/issues

## Sketch

Share your sketch.

But share with us the smallest sketch that reproduces the bug. Remove all the things that are not related to Bluepad32.

Copy & paste your sketch in services like: <http://gist.github.com>

DO NOT COPY your sketch on Discord. It is difficult to read it there.

E.g.: The sketch that I'm using is in this URL (add a link).

## Operating System

Are you using Windows, macOS, Linux? Which version?

E.g.: I'm using macOS 12.6.7

## Debug log

Copy & paste the output from the serial console, and share it with us.

Use services like: <http://gist.github.com>

Attaching a screenshot might be useful, but the screenshot is not a replacement of the Debug Log.

E.g.: The console output can be found on this link (add link).

[discord_server]: https://discord.gg/r5aMn6Cw5q

## Example

Example of question with the needed info:


!!! Example

    I'm using Bluepad32 v3.9.0 with a SparkFun Thing ESP32.
    I'm using it with Arduino IDE v2.1.0.

    I'm trying to enable rumble with:

    `ctl->setRumble(0xc0, 0xc0);`.

    It works with a Nintendo Switch controller, but it doesn't work with a DualShock 4 controller.

    - This is the output [attach console output or post it in gist.github.com]
    - The complete sketch is here: [attach sketch or post it in gist.github.com]

    What could be the issue ?


*(link to this page: <http://bit.ly/bluepad32-help>)*

