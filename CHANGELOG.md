# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] - 2022-??-??
### New

- Devices: Support for feature reports.
- DualSense / DualShock 4: add support for get firmware version and calibration values.
  Although calibration is ignored ATM.
- DualShock3: Add partial support for DualShock 3 clones, like PANHAI.
- 8BitDo Arcade Stick: Add support for it.
  It keeps responding to inquiries even after the device is connected.

### Changed

- DualSense: Player LEDs are reported in the same way as PlayStation 5.
- Switch: Don't turn ON Home Light. 
  8BitDo devices don't support it. And might break the gamepad setup sequence.
- Xbox: Improved reliability (based on the new auto delete code)
- API: renamed some functions / typedefs
  - uni_hid_parser() -> uni_hid_parse_input_report()
  - report_parse_raw_fn_t -> report_parse_input_report_fn_t
- These functions are "safe". Can be called from another task and/or CPU.
  The suffix "_safe" were added to their names:
  - uni_bluetooth_del_keys()
  - uni_bluetooth_enable_new_connections()
- Arduino: Seat assignment simplified.
- Bluetooth: request device name for incoming connections.
- Bluetooth: Replaced the old "auto_delete" code, which used "counts", with
  one based on a timer. New code includes incoming connections.
- Bluetooth: SDP query refactor
  SDP query timeout using btstack timers instead of ad-hoc one.
  Perform VID/PID query before HID-descriptor.
  If HID-descriptor is not needed, don't do the query. Reduces connection latency.
  Overall, cleaner code.

### Fixed

- Bluetooth: add log about Feature Report not being supported ATM.
- Bluetooth: Incoming connections are more reliable.
  Scan inquiry is only done for 1.28secs, then a "wait" for 1 second, that
  allows incoming connections to be received.
- Bluetooth: fix when parsing device name. Does not reuse previous discovered device.
- DualSense: doesn't not disconnect randomly
- Switch: Don't enable "home light" at setup time.
  8BitDo gamepads don't implement it and might not be able to finish setup.

## [2.5.0] - 2022-01-08
### New

- tools/pc_debug: Integrate into Gitlab CI
- tools/fw: add --clean argument to build.py
- uni_bluetooth: add uni_bluetooth_set_enabled().
  Allows users to enable/disable accepting new gamepads in runtime.

### Changed

- Update BTStack to 1.5.1. Using hash: d778e7488c62d361fc176e8ae40c0d6bc8b00089
- Doc: Updated to use idf.py instead of make
- tools/fw: build.py uses idf.py instead of make
- Kconfig: The max number of connected gamepads can be changed from menuconfig
  Default changed from 8 to 4.

### Fixed

- NINA/AirLift:
  - Disconnect works as expected.
  - Replaced custom "queue" code with FreeRTOS xQueue
- tools/fw: Remove sdkconfig before building another platform

## [2.4.0] - 2021-12-26
### New

- Arduino platform:
  This time for real. Generic ESP32 modules are supported.
  It is possible to have Bluepad32 + Arduino in a generic ESP32 module.
  Supported via ESP-IDF toolchain.
  A "bluepad32_arduino" ESP-IDF component was added with helper classes.
- Bluepad32 is a ESP-IDF "component":
  It makes easier to embed Bluepad32 in other projects.
- BLE: WIP. For the moment it is disabled.

### Changed

- Update BTStack. Using hash: 0f2a810173d1d70943d1c915bffd6f9b1171e8f6
- Update ESP-IDF to latest v4.3 version. Using hash: da6c5be6c15e1c1854d91787fa166f426568d678
- idf.py: uses new ESP-IDF cmake functions, works as intented
- sdkconfig: removed. Using sdkconfig.defauls now. Easier to mantain.
- sdkconfig: coredump is stored in flash, and not dumped in UART console.
- Kconfig: Moved most uni_config.h defines to Kconfig
- uni_conn: Placed connection variables in uni_conn instead of having them in uni_device.
  uni_device has a uni_conn_t now.
- clang-format: switched to 4-space indent, 120-column-width

### Fixed

- crc32_le() fixed. ESP32 does not call our code. Fixes core dumps and other related crc32 features.
- Unijoysticle: Turn off LEDs when gamepad disconnects.
- core: When a gamepad disconnects, it resets it state, allowing re-connecting it again (issue #1)

## [2.3.0] - 2021-11-13
### Changed

- Unijoysticle: Gamepad 2nd & 3rd buttons swap. "X" is 2nd button, and "Y" is 3rd button
- Update gamepad database from SDL. Using hash: 7dfd22ac5ef60c730d7900053b64e8430e08a29d
- Update BTStack. Using hash: 4ac37295456dc9203a99c17ee4649b7e06238983
- Update ESP-IDF to latest v4.2 version. Using hash: 047a5bf2f7e121077740e7ea59e616718d417fed

## [2.2.1] - 2021-07-24
### Added

- NINA/AirLift: Add "Connection Status" method so that WiFiNINA's CheckFirmwareVersion
                sketch works unmodified.

### Changed

- NINA/AirLift: Return "1" (instead of "0") when transaction is Ok.
- NINA/AirLift: Improved documentation

## [2.2.0] - 2021-07-17
### Added

- New platform: NINA.
  - Boards like Arduino Nano RP2040 Connect are supported
  - Airlift & NANO share the same code.
  - Protocol was update to be more friendly with new NINA commands, that were not present on
    Adafruit Airlift firmware

### Changed

- Using bstack v1.4.1

## [2.1.0] - 2021-06-25
### Added

- Nintendo JoyCon support: Both Left and Right JoyCons are supported.
      Each one is its own controller. Should be used in Horizontal position.

### Changed

- Using esp-idf v4.2
- Using bstack v1.4
- Nintendo Switch:
  - "+" is used a "Home" button, instead of "Capture"
  - Code is cleaner, easier to read.

### Fixed

- Improved Android gamepad support, including support for SteelSeries Stratus Duo

## [2.0.0] - 2021-05-23
### Changed

- No changes. Just renamed v2.0.0-rc0 to v2.0.0

## [2.0.0-rc0] - 2021-04-09
### Added

- New platform: MightyMiggy, a new platform that targets Commodore Amiga
- uni_hid_device: controller_subtype: populated by each "driver".
                  Sets in which "mode" or which attachment the driver has.
                  For the moment only used for Wii attachments.
- device_vendors: VID/PID for 8bitdo Zero 2 and TUTUO (a PS4 clone)
- ds4: explicitly require Stream mode at setup time.

## [2.0.0-beta2] - 2021-01-14
### Added

- DualSense: Supports lightbar LED + player LEDs + rumble
- Wii / Wii U: Supports '-' button to mappings

### Changed

- Wii: Improved mappings while WiiMote is in "vertical" mode
- Parser: rename some functions. Now the follow the "standard" names:
  - set_leds -> set_player_leds
  - set_led_color -> set_lightbar_color
- tools/circuitpython: moved more complex samples to [quico repo][quico]
- DualSense/DS4/DS3: simplified code, easier to read & maintain

### Fixed

- DualShock4: Rumble duration works Ok
- DualShock3: Rmble + LED work at the same time
- Switch: Rumble duration works Ok

[quico]: https://gitlab.com/ricardoquesada/quico

## [2.0.0-beta1] - 2020-11-30

- uni_bluetooth: added "auto-delete" to prevent having "orphan" connections forever
- AirLift:
  - Multi-core code fixed.
  - Added "bluepad32.py" library (tools/circuitpython/)
  - Added more samples/tests in tools/circuitpython/
- Gamepads: Provide 3 properties (when avaiable):
  - Setting player LEDs
  - Setting LED color (DS4, DualSense)
  - Rumble
- DualShock4: Supports Rumble (WIP) + any color can be set to the LED
- DualShock3: Supports players LEDs + Rumble
- DualShock3: Fix button mappings. A <-> B, X <-> Y
- XboxOne: Suports Rumble
- Switch: Supports rumble (WIP)
- tools: each tool in its own directory.
- tools/circuitpython: Added "snake", a "real" game that showcases the gamepad
- ESP-IDF sdkconfig:
  - Bluetooth Controller: BR/EDR ACL Max Connections: 7 (max value)
  - Bluetooth Controller: BR/EDR Sync (SCO/eSCO) Max Connections: 3 (max value)

## [2.0.0-beta0] - 2020-11-20

- Firmware: Added support Sony DualSense (PS5) and DualShock3 (PS3) gamepads
- Platform: Added support for AirLift family of modules
- Refactor: Move platform logic to "platforms". Uni_hid_device is simpler,
        less error-prone. Platforms should implement their own logic.
        Added more events to platforms like connect, disconnect, ready.
- Misc: Firmware extracted from Unijoysticle2 project and make it its own
        project. Easier for other non-unijoysticle user to integrate into their
        own projects. New name for the firmware: "bluepad32".
- tools:
  - Compiles & updated to new btstack code
  - Added "circuitpython_paint.py": Example of how to use Bluepad32 with
    MatrixPortal M4 (AirLift)
- BTstack: Using master 2020-10-07 - 16f6f81ae588cf9f7d69b19328eee69fbd03db94
- ESP-IDF: Using v4.1

## [1.1.0] - 2020-07-23

- Firmware: Nintendo Wii/Wii U: auth works as expected.
- Firmware: use gap_pin_code_response() to send Pin codes.
            iCade + Nintendo Wii works in any situation.
- Firmware: "idf.py build" fixed
- Firmware: Gamepad Reconnection works.
- ESP-IDF: Using v4.1-beta2 - c5cb44d8135a5ec6678259e30468632bc2442288
- BTstack: Using develop 2020-07-23 - 2992f73460b28b1f150ed983204620fbe35503bc

## [1.0.0] - 2020-02-29 (abuelito Ricardo release)

- Firmware: Nintendo Switch Pro: turns on LED when connected
- Firmware: Where applicable, added technical info references at top of .c files
- Firmware: Renamed PS4 to DS4 (DUALSHOCK4)
- BtStack: Using hash 4d24213549c6b94b84d732afda9c2628df22fd70 (2020-02-20)
- ESP-IDF: Using v4.0
  - Components Bluetooth: enabled
    - Controller -> Bluetooth controller mode: Bluetooth dual mode
    - Controller -> BR/EDR ACL Max Connections: 4
    - Controller -> BR/EDR Sync Max Connections: 2
    - Host: Controller only
  - Components ESP32-specific
    - Main XTAL frequency: Autodetect
  - Components Core -dump
    - Data destination: UART

## [0.5.4] - 2020-02-23

### Added

- Firmware: Nintendo Switch Pro uses "raw" parser instead of HID, making it more flexible.
- Firmware: Nintendo Switch Pro clones supported.
- Firmware: Nintendo Switch Pro uses factory calibration data to align sticks.
- Firmware: hid_device dump includes controller type.
- Firmware: updated controllers DB from SDL

## [0.5.3] - 2020-02-09

### Added

- Firmware: Nintendo Switch Pro: Original Nintendo Switch Pro works.
- Firmware: Nintendo Switch Pro: LEDs are being set.
- Firmware: DualShock 4: LEDs are being set. Uses HID report type 0x11 by default.

### Changed

- Firmware: MTU changed from 48 to 128 bytes. Needed for report 0x11 in DualShock4.
- Firmware: Linux version compiles with clang by default.
- Firmware: Added missing 'break' in OUYA and Generic drivers, making them more reliable.

## [0.5.2] - 2020-02-01

### Added

- Firmware: Xbox One: added support for firmware v4.8.

## [0.5.1] - 2020-01-03

### Added

- Firmware: Xbox One: rumbles when connected or switches joystick port.
- Firmware: ESP-IDF v3.3.1
- Firmware: SDP queries timeout after 3 seconds, enabling another SDP query to start.
- Firmware: improved logging

## [0.5] - 2019-12-15

### Added

- Firmware: Wii Remote 1st gen correctly detects attached extensions like the
            Nunchuk and Classic Controller.
- Docs: firmware setup doc has info about Windows and includes some screenshots.

## [0.5-rc2] - 2019-12-14
### Added

- Firmware: Support for Nintendo Wii Classic Controller / Classic Controller Pro

### Changed

- Firmware: After swapping the joysticks ports, the joysticks lines are "Off".
            Prevents leaving unexpected lines as "On".
- Firmware: Wii driver: clean up code. Added "instance" concept, easier to mantain.

## [0.5-rc1] - 2019-12-09
### Added

- Firmware: Added support for Nintendo Wii Nunchuk
- Firmware: PC platform: added support for "delete keys" and "enable enhanced mode"
            via command line.

### Changed

- Firmware: Auto-fire in enhanced mode is swapped.
            Button "Shoulder Left" triggers auto-fire in to Joy A.
            Button "Shoulder Right" triggers auto-fire in Joy B.
- Firmware: Improved debug logging in "PC platform".
- Firmware: Code is more readable in uni_gamepad.c.
- Firmware: When two controllers are connected, the two LEDs are turned on.

## [0.5-rc0] - 2019-12-06
### Added

- Firmware: Added support for Nintendo Switch Pro controller.
- Firmware: Improved 8Bitdo gamepad support (SN30 Pro & Lite).
- Firmware: BTStack f07720a033c9fcfa856511634253b9889fa94cd8 (2019-12-6)

## [0.5-beta2] - 2019-11-17
### Changed

- Firmware: iCade 8-bitty fixed shoulder-left / start button mappings.
- Firmware: Print to console whether single-port / 3-button mode is enabled.

## [0.5-beta1] - 2019-11-10
### Added

- Firmware: Added support for "single joystick" unijoysticle devices.
            Edit `uni_config.h` and change `UNIJOYSTICLE_SINGLE_PORT` to 1
- Firmware: Added support for iCade 8-bitty gamepad.

### Changed

- Firmware: Improved iCade Cabinet support:  autofire, debug and shoulder buttons supported.
- Firmware: ESP-IDF v3.3
- Firmware: BTStack 138818a33e591e964a727284c192700abe2fee26 (2019-9-9)

## [0.4] - 2019-09-28
### Changed

- Firmware: Fix: Can enter combo Joy-Joy when there are disconnected devices.

## [0.4-rc0] - 2019-08-12
### Added

- Firmware: Support for Nintendo Wii Remote Motion Plus controller.
- Firmware: Support for "accelerometer mode" in Nintendo Wii Remote.
- Firmware: Support for "vertical mode" in Nintendo Wii Remote.

### Changed

- Firmware: Fix crash when printing "cannot swap joystick"
- Firmware: Compile ESP-IDF as Release build.
- Docs: Improved "supported devices".

## [0.3] - 2019-08-05
### Added

- Firmware: Nintendo Wii controller LED's represent the joystick port assigned to.

### Changed

- Firmware: Nintendo Wii controller uses "horizontal" orientation setup.
- Firmware: Nintendo Wii U has Y axis working correctly.

## [0.3-rc0] - 2019-08-03
### Added

- Firmware: Add Nintendo Wii generic support. This includes
  - Wii U Pro controller
  - Wii Remote
  - Possibly other Nintendo Wii controllers

### Changed

- Firmware: Nintendo Wii U Pro support: Works Ok on ESP32.
- Firmware: Bluetooth state machine. Code clean-up. It is easier to mantain.
- Firmware: Using btstack master-branch. Commit: dbb3cbc198393187c63748b8b0ed0a7357c9f190

### Removed

- Firmware: Name discovery disabled for the moment

## [0.3-beta] - 2019-07-27
### Added

- Firmware: Added Wii U Pro controller support.

### Changed

- Firmware: Using ESP-IDF v3.2.2
- Firmware: Using btstack develop-branch. Commit: a4ea32feba8ca8a16509a75d3d80e8017ca2cf3b

## [0.2.1] - 2019-06-29
### Added

- Firmware: more verbose logs when detecting the type of device
- Firmware: Started Wii U Pro controller support. Not working yet.

### Changed

- Firmware: Gamepad names are fetched correctly.
- Firmware: Using btstack develop-branch. Commit: 32b46fec1df77000b2e383d209074f4c2866ebdf
- Firmware: "apple" parser renamed to "nimbus" parser.

## [0.2.0] - 2019-05-22
### Added

- Docs: User guide

### Changed

- Firmware: Combo-mode:
  - Turn on both LEDs when enabled.
  - When back from combo-mode, restore previously used port
  - Cannot swap ports when in combo mode
- Firmware: Atari ST mouse support
  - Buttons working as expected
  - A bit smoother than v0.1.0 but still not good enough
- Firmware: Updated link to http://retro.moe/unijoysticle2
- Firmware: Using ESP-IDF v3.2. Commit: 286202caa31b61c2182209f37f8069a0b60fb942
  - Components Bluetooth: enabled
    - Bluedroid: disabled
    - Controller -> Bluetooth controller mode: Bluetooth dual mode
    - Controller -> BR/EDR ACL Max Connections: 4
    - Controller -> BR/EDR Sync Max Connections: 2
  - Components ESP32-specific
    - Coredump to UART
    - Main XTAL frequency: Autodetect
  - Components Wi-Fi
    - Software controls WiFi/Bluetooth coexistence: disabled
- Firmware: Using btstack develop-branch. Commit: 4ce43359e6190a70dcb8ef079b902c1583c2abe4

## [0.1.0] - 2019-04-15
### Added

- Firmware: v0.1.0
  - Using ESP-IDF v3.1.3. Commit: cf5dbadf4f25b395887238a7d4d8251c279afa8c
  - Using btstack develop-branch. Commit: 8b22c04ddc425565c8e4002a6d4d26a53426a31f
