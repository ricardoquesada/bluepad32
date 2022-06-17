# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] -
## New
- Arduino: Add "Console" class. An alternative to Arduino "Serial" that is
  compatible with Bluepad32's USB console.
- Property storage abstraction
  API that allows to store key/values in non-volatile areas
- Console. Added new commands:
  `get` / `set_gap_security_level`: Similar to the `menuconfig` option, but no need to recompile it.
  `get` / `set_gap_periodic_inquiry`: To change the new-connection/reconnection window
- NINA/AirLift: Add support for "enable/disable new bluetooth connections"
- Mouse: Added support for:
  - Apple Magic Mouse 2nd gen
  - Logitech M-RCL124 MX Revolution

## [3.5.0-beta0] - 2022-05-30
### New
- Mouse support:
  Bluetooth BR/EDR mice supported. Only a few are supported at the moment.
  This is to fine-tune the DPI. Mouse data is populated in the gamepad structure:
  - axis_x / axis_y contains the mouse delta movements normalized from -127/127.
  - Left button: A
  - Right button: B
  - Middle button: X
- Mouse: Added "quadrature encoder" driver. Supports up to two
   mice at the same time. Each mouse requires 2 timers. So if two mice are used
   at the same time, that means that there are no HW timers left.
- Unijoysticle: Supports Amiga/AtariST mouse. Two mice can be used at the same time.
  - Unijoysticle v2+ / v2 A500: fully functional mouse on both ports
  - Unijoysticle v2: fully functional mouse on Port #1. Right/Middle button not available on Port #2
- Unijoysticle: Amiga/AtariST mouse emulation can be changed from `menuconfig` or console. See below.
- Unijoysticle: Support for Unijoysticle v2 A500 (a new board that fits in the Amiga 500/1200)
- Unijoysticle: New supported combinations to swap ports by pressing "system" gamepad button:
  - Gamepad and Mouse
  - Gamepad and Gamepad while both press "system" at the same time
- Doc: List of supported mice is here: [supported mice][supported_mice]
- console: Added a "USB console". Access it from `idf.py monitor` or `minicom`
  It has the following commands:
  - `set_mouse_emulation` / `get_mouse_emulation`
  - `set_mouse_scale` / `get_mouse_scale`
  - `set_gamepad_mode` / `get_gamepad_mode`
  - `swap_ports`
  - `devices`
  - `tasks`, `version`, `help` and more

[supported_mice]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/develop/docs/supported_mice.md

### Changed
- Unijoysticle v2: Added support for 2nd and 3rd button in Joystick #1.
- Unijoysticle v2+: Swap 2nd with 3rd button.
  - X is 2nd joystick button
  - Y is 3rd joystick button

### Fixed
- DualSense/DualShock: reconnect works Ok [Bug #14][gitlab_bug_14]
- Nintendo Switch Pro/JoyCon: reconnect works Ok [Bug #15][gitlab_bug_15]

[gitlab_bug_14]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/14
[gitlab_bug_15]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/15

## [3.1.1] - 2022-04-22
### Fixed
- NINA/AirLift: Binaries are generated with UART console disabled. [Bug bp32-arduino #3][gitlab_bp32_arduino_03]
   UART console can be enabled/disabled from menuconfig and only when NINA/AirLift are not selected.
- Cleanup connect/disconnect code
- pc_debug: Enable extra compiler-warnings to check possible bugs

[gitlab_bp32_arduino_03]: https://gitlab.com/ricardoquesada/bluepad32-arduino/-/issues/3

## [3.1.0] - 2022-04-14
### New
- Wii: Add support for Rumble
- Arduino,AirLift,NINA: Add support for "get gamepad properties" and
  "enable Bluetooth connections"
- Gamepad: Add support for Nintendo SNES (Swicth Online)

### Changed
- 8BitDo: "Dinput" mode mappings updated to match the other modes' mappings:
  Button "-" / "select" maps to BUTTON_BACK
  Button "+" / "start" maps to BUTTON_HOME
  BUTTON_SYSTEM is not mapped any more.
- Nintendo Joycons mappings updated for the "-", "+", "home" and "capture".
  The button that is at the left, is mapped to BUTTON_BACK
  The button that is at the right, is mapped to BUTTON_HOME
- Arduino: Example code supports multiple gamepad connections
  Internally ArduinoGamepad.h was refactored to better support "gamepad properties".
- Wii: When in accelerometer mode (wheel):
  - Dpad can be used, useful for menu navigation
  - Button "1" is brake (Dpad down)
  - Button "2" is Throttle (Dpad up)
  - "Up" and "Down" disabled from Accelerometer
- Unijoysticle: Disable Bluetooth connections once two gamepads are connected,
  or when in "Enhanced Mode". Re-enable when that's not the case.

### Fixed
- Arduino/NINA/AirLift: Assign gamepad type at connection time: [Bug #10][gitlab_bug_10]
- Compile-time error if ACL connections are not least 2 [Bug #11][gitlab_bug_11]
- "Enable/Disable Bluetooth connections" works everytime it is called.

[gitlab_bug_10]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/10
[gitlab_bug_11]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/11

## [3.0.1] - 2022-03-15
### New
- Arduino: Add function to return Gamepad model

### Changed
- Update BTStack to latest stable. Hash: 45e937e1ce34444aabbc21ec35504e1c50ad528d
- Compiler option: Using -O2 (optimize for performance) by default. Debug disabled.

### Fixed
- 8BitDo: Added SF30 Pro to the DB. It correctly identifies it as 8BitDo gamepad.

## [3.0.0] - 2022-02-27
### New
- Docs: Added CONTRIBUTING.md file

### Changed
- Update ESP-IDF v4.4 to hash: 000d3823bb2ff697c327958e41e9dfc6b772822a

### Fixed
- Docs: Minor fixes in documentation, moved images to its own folder.
- 8BitDo: Added SFC30 to the DB. It correctly identifies it as 8BitDo gamepad.

## [3.0.0-rc1] - 2022-02-06
### Changed
- Bluetooth: To err on the safe side, new connection have a bigger window time to connect,
  and re-connects (incoming connections) a slightly smaller window. I'll revisit this in the future.
- Wii: Accelerometer mode works as if the Wii Remote is being inside a Wii Wheel.
  This is because the "old" mode was not rather difficult to use, and using it for the
  Wii Whell is very usable in C64 racing games.

### Fixed
- Bluetooth: use page scan / clock offset if available when requesting name.
  This used to be the case in v2.5, but disabled during the big refactoring.
- Unijoysticle: correclty set Player LED when second player connects
- Unijoysticle: reject new connections if two gamepads are already connected.
- Unijoysticle: On DualSense gamepads, update Player LEDs when gamepad connects.
- Nintendo Switch: Pressing "system" button has a delay of 200ms before being able
  to press it again. This helps switching the Joystick port in Unijoysticle.
- Nintendo Switch: While setting up the controller, each step has its own timeout.
  This helps setup clones that might need certain steps in order to enable report 0x30.
- 8BitDo: Added SNES30 to the DB. It correctly identifies it as 8BitDo gamepad.
- MightyMiggy: All internal functions declared static to avoid collision with other projects.

## [3.0.0-rc0] - 2022-01-31
### New
- Devices: Support for feature reports.
- DualSense / DualShock 4: add support for get firmware version and calibration values.
  Although calibration is ignored ATM.
- Switch JoyCon clones: Added support for clones (at least some of them).
- DualShock3: Add partial support for DualShock 3 clones, like PANHAI.
- 8BitDo Arcade Stick: Add support for it.
  It keeps responding to inquiries even after the device is connected.
- API to change mappings. Although this feature is not complete yet.
  Missing save/restore, BLE service. Consider it WIP.

### Changed
- DualSense: Player LEDs are reported in the same way as PlayStation 5.
- Switch: Don't turn ON Home Light,  8BitDo devices don't support it,
  and might break the gamepad setup sequence.
  Change order of setup commands.
  Change mappings in JoyCons so that it is possible to press "System" button.
- Xbox: Highly improved reliability (based on the new auto delete code)
  Compared to before, it is like 10x more reliable, but the Xbox Wireless Controller
  is still somewhat unreliable.
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
- Bluetooth: SDP query + state machine refactor.
  SDP query timeout using btstack timers instead of ad-hoc one.
  SDP query moved to its ow file.
  Perform VID/PID query before HID-descriptor.
  If HID-descriptor is not needed, don't do the query. Reduces connection latency.
  DualShock 3 and Nintendo Pro Controllers are guessed by their names
  The state machine was rewritten. Although it is still complex, it is much
  easier to understand. In the process, many bugs were fixed.
  Gamepad name is requested to all gamepads. If it fails, a fake one is assigned.
  This is because the "name" is now used to identify certain devices.
  Switch Pro Controller clone improved.
  Re-connection works in many devices, including Switch clones!
  (but not all devices support reconnection yet).
  Overall, cleaner code.
- Bluetooth: Add set_event_filter.
  When doing the inquiry, only devices with a Class Of Device HID are shown.
  This reduces the noise in the console
- Tested with latest ESP-IDF v4.4 (eb3797dc3ffebd9eaf873a01df63aed89fad58b6)

### Fixed
- Bluetooth: add log about Feature Report not being supported ATM.
- Bluetooth: Incoming connections are more reliable. Using HCI periodic inquiry mode.
- Bluetooth: fix when parsing device name. Does not reuse previous discovered device.
- DualSense: doesn't not disconnect randomly
- Switch: Don't enable "home light" at setup time.
  8BitDo gamepads don't implement it and might not be able to finish setup.
  Fixed mappings of button "+" in report 0x30.

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
