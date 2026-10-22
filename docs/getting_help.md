# Get help

!!! Note

    If someone directed you to this page, your question or bug report lacks
    required information. Read this entire page before you post.

To get help or report a bug, use these channels:

* Post questions on the [Discord server][discord_server] :simple-discord:
* Report bugs or feature requests on the [GitHub Issue Tracker][github_issue_tracker] :simple-github:

Include the following information in your report:

* Bluepad32 platform and version
* DevKit board model and product link
* Controller model, firmware version, and product link
* Minimal sketch that reproduces the issue
* Host operating system and version
* Serial debug log

[github_issue_tracker]: https://github.com/ricardoquesada/bluepad32/issues

## Platforms

Bluepad32 supports several platforms:

* Arduino IDE for ESP32 modules
* Arduino + ESP-IDF
* ESP-IDF
* Pico SDK
* Unijoysticle

Specify your platform, platform version, and Bluepad32 version.

Example: `Arduino IDE v2.0.9 with Bluepad32 v3.7.3`.

## DevKit module

Specify the exact DevKit board and provide a link to the hardware page.

Example: `SparkFun Thing Plus - ESP32 WROOM:` <https://www.sparkfun.com/products/17381>.

## Controllers

Check the official compatibility lists first:

* [Supported gamepads][supported_gamepads] :material-gamepad-variant:
* [Supported mice][supported_mice] :material-mouse:
* [Supported keyboards][supported_keyboards] :material-keyboard:

Provide the controller brand, model, firmware version, and product link. State
whether the controller is genuine or a third-party clone.

Example: `Genuine Nintendo Switch Joy-Con with firmware v3.72`.

Bluepad32 does not guarantee support for unlisted controllers.

### Request support for unsupported controllers

To request support for a new controller, contact us on
[Discord][discord_server] :simple-discord: or [file a feature request][github_bug] :simple-github::

* Send a link to the controller product page (such as Amazon or AliExpress).
* Send the cost of the gamepad plus US shipping [via PayPal][paypal] :simple-paypal:.
* We will purchase the controller and attempt to add support. We cannot
    guarantee compatibility, especially for low-cost clones.

[supported_gamepads]: ../supported_gamepads/
[supported_mice]: ../supported_mice/
[supported_keyboards]: ../supported_keyboards/
[paypal]: https://www.paypal.com/paypalme/RicardoQuesada
[github_bug]: https://github.com/ricardoquesada/bluepad32/issues

## Sketch

Share the smallest sketch that reproduces the bug. Remove all code that does
not relate to Bluepad32.

Upload your sketch to <http://gist.github.com> and share the link. Do not paste
long code blocks directly into Discord.

## Operating system

Specify your host operating system (Windows, macOS, or Linux) and its version.

Example: `macOS 12.6.7`.

## Debug log

Copy the serial console output and upload it to <http://gist.github.com>.

You can attach a screenshot for context, but you must also provide the text
debug log.

[discord_server]: https://discord.gg/r5aMn6Cw5q

## Example report

Use this template when you ask a question:

!!! Example

    I use Bluepad32 v3.9.0 on a SparkFun Thing ESP32 with Arduino IDE v2.1.0.

    I call `ctl->setRumble(0xc0, 0xc0);` to enable rumble.

    Rumble works on a Nintendo Switch controller, but fails on a DualShock 4
    controller.

    - Console output: [paste gist.github.com link]
    - Minimal sketch: [paste gist.github.com link]

    How can I fix this issue?

*(Short link to this page: <http://bit.ly/bluepad32-help>)*

