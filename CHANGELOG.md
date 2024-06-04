# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [4.1.0] - 2024-06-03
### New
- Platform: new callback: `on_device_discovered(bdaddr, name, cod, rssi)`
  - Platforms can override this function to determine whether Bluepad32 should connect to the discovered device.
- BLE: Populates RSSI field.
- Support for 5-key handlebar multimedia keyboard. Fixes [Github Issue #104][github_issue_104]

[github_issue_104]: https://github.com/ricardoquesada/bluepad32/issues/104

### Changed
- BTstack: Updated to v1.6.1

### Fixed
- Service: Returns correct error code when writing values.
- Doc: Improved documentation.

## [4.0.4] - 2024-04-20
### New
- Wii: `uni_hid_parser_wii_set_mode(struct uni_hid_device_s* d, wii_mode_t mode)`,
  to change the mode between vertical or horizontal.

### Changed
- Rumble stops when duration is 0. Before when duration was 0, the rumble command was ignored.
- Wii + Nunchuk:
  - Nunchuk is reported as the "right" axis using buttons X and Y
  - Wii Mode is reported as "left" using buttons A, B, Shoulder L & R, and misc buttons
  - If a Nunchuk is attached, vertical mode is used.
  - Nunchuk + horizontal mode removed (it was broken, and it is not useful)

### Fixed
- Pico W: Allow multiple BLE connections. Fixes [Github Issue #91][github_issue_91]
- Nintendo: Accept reports with more data than requested. Fixes [Github Issue #94][github_issue_94]

[github_issue_91]: https://github.com/ricardoquesada/bluepad32/issues/91
[github_issue_94]: https://github.com/ricardoquesada/bluepad32/issues/94

## [4.0.3] - 2024-04-09
### New
- Keyboard: Add method to change LEDs:
  - `uni_hid_parser_keyboard_set_leds(struct uni_hid_device_s* d, uint8_t led_bitmask)`

### Changed
- Mouse constants: `MOUSE_BUTTON_*` renamed to `UNI_MOUSE_BUTTONS_*`. Avoid conflicts with other libraries, like TinyUSB.
- Stadia/Xbox: retry rumble switch from 25ms to 50ms. And don't retry on errors other than `ERROR_CODE_COMMAND_DISALLOWED`
- BTstack: updated to latest `develop` branch. Hash: b74edc46d02bbb5d44d58cfddd05aa74f9735e5e

### Fixed
- Pico W: Example uses BTstack from Bluepad32 branch. Updated and with custom patches.
- NINA: Works, there was a regression in v4.0-beta2. Fixes [Github Issue #90][github_issue_90]

[github_issue_90]: https://github.com/ricardoquesada/bluepad32/issues/90

## [4.0.2] - 2024-04-03
### Fixed
- Unijoysticle: Fix how to swap ports from gamepad
- Device: correctly identify mouse/keyboards
- DualSense: dump info tabbed.

## [4.0.1] - 2024-04-02
### New
- [Bluepad32 logo][bluepad32_logo]: Official logo for Bluepad32

[bluepad32_logo]: https://github.com/ricardoquesada/bluepad32/blob/5af12059f4b93ed70c704f8736b7e437269a23ad/docs/images/bluepad32_logo_ok_280.png?raw=true

### Changed
- `uni_property_dump_all()` deprecates `uni_property_list_all()`

### Fixed
- Xbox/Stadia: Improvements in rumble. A re-fix of [Github Issue #85][github_issue_85]
- Console: `getprop <property_name>` works as expected
- Property: Correctly dump string properties in ESP32

## [4.0] - 2024-03-24
### Changed

- Example: Move DualSense trigger adaptive effect from Pico to Posix example.
- Example: Add Xbox Trigger rumble in Posix example.

### Fixed
- Xbox: 8BitDo controllers don't rumble forever. Fixes [Github Issue #85][github_issue_85]
- Xbox: Using internal timer for delayed start, making it easier to cancel previous rumble command.
- Switch: IMU and Stick calibration work as expected.
- Switch: Fix crash while parsing IMU calibration data on RP2040. Fixes [Github Issue #86][github_issue_86]

[github_issue_85]: https://github.com/ricardoquesada/bluepad32/issues/85
[github_issue_86]: https://github.com/ricardoquesada/bluepad32/issues/86


## [4.0-rc0] - 2024-03-20
### New
- New Rumble API: `play_dual_rumble(start_delayed_ms, duration_ms, weak_magnitude, strong_magnitude)`. Fixes [Github Issue #83][github_issue_83]
  - The old `set_rumble(magnitude, duration)` was removed from Bluepad32 "raw"
  - In Arduino API, users can still use the old one, but it is deprecated. A compile-time warning will appear.
  - To convert your old code to the new one, do: `play_dual_rumble(0, duration * 4, force, force);`
- DualSense: Support "vibration2" rumble for DualSense Edge and newer regular DualSense

### Fixed
- Virtual Device: Don't remove virtual device when Xbox device is connected. Fixes [Github Issue #77][github_issue_77]
- Xbox: Add rumble support for Xbox Firmware v5.x. Fixes [Github Issue #69][github_issue_69]
- Stadia: Add rumble support. Fixes [Gitlab Issue #36][gitlab_issue_36]
- PSMove: Duration in rumble works as expected.
- PSMove: LEDs don't get overwritten by rumble.

[github_issue_69]: https://github.com/ricardoquesada/bluepad32/issues/69
[github_issue_77]: https://github.com/ricardoquesada/bluepad32/issues/77
[github_issue_83]: https://github.com/ricardoquesada/bluepad32/issues/83
[gitlab_issue_36]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/36

## [4.0-beta2] - 2024-03-09
### New
- BLE Service: Added `uni_bt_enable_service_safe(bool)` API to enable/disable in runtime
- DualSense: Add support for adaptive trigger
- Add support for RX-05 TikTok Ring Controller [Github Issue #68][github_issue_68]

### Changed
- "linux" example renamed to "posix" since it also works on macOS
- UART: UART console cannot be toggled at runtime. It can only be disabled at compile-time via "menuconfig"
  - Needed to use IDF-IDF v5.2
  - Improves Arduino Nano ESP32 [Github Issue #65][github_issue_65] by forwarding Bluepad32 logs to Arduino.
- Documentation: Improved documentation
- Arduino IDE:
  - ESP32-S3 boards, like 'Arduino Nano ESP32' or 'Lolin S3 Mini' can see output using different compile. For details see [Github Issue #65][github_issue_65]
  - Calls `btstack_stdio_init()` to have buffered output

### Fixed
- Pico W/Posix: string properties return default value instead of error.
- Pico W/Posix: TLV uses its own namespace to prevent clash from user code.
- Keyboard: 8BitDo Retro Keyboard volume knob works Ok.

[github_issue_65]: https://github.com/ricardoquesada/bluepad32/issues/65#issuecomment-1987046804
[github_issue_68]: https://github.com/ricardoquesada/bluepad32/issues/68


## [4.0-beta1] - 2024-02-11
### New
- Pico W: Implement property using BTstack TLV
- Linux: Implement property using BTstack TLV
- BLE Service: Add "reset" characteristic
- Arch: Added `uni_system_reboot()` function

### Fixed
- Doc: Many fixes to documentation, including adding/changing/removing pages to MKDocs.
- Misc: Make `const` different variables that needed to be constants.

## [4.0-beta0] - 2024-02-04
### New
- Pico W support
  - Support for Raspberry Pi Pico W microcontroller
  - Supports BLE and BR/EDR
  - Console / NVS not supported yet
  - Internally uses the "custom" platform.
- PS4 Move Controller: Add support for it. Before was only PS3 Move. [Github Issue #41][github_issue_41]
- Mappings: can change between Xbox, Switch and custom mappings. Fixes [Gitlab Issue #26][gitlab_issue_26]
- BLE Service: Can read,write Bluepad32 properties from a BLE client (E.g.: mobile phone). Still WIP.
- Bluetooth: Expose RSSI property. WIP, part of [Github Issue #42][github_issue_42]

[github_issue_41]: https://github.com/ricardoquesada/bluepad32/issues/41
[github_issue_42]: https://github.com/ricardoquesada/bluepad32/issues/42
[gitlab_issue_26]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/26

### Changed
- Platform boot logic changed a bit. Rationale: "don't make magic changes".
  - `uni_main()` does not call `btstack_run_loop_execute()` automatically. Must be called by the user
  - `uni_main()` renamed to `uni_init()`.
    - Rationale:  `uni_init()` just does the initialization. Before `uni_main` was doing init and main loop.
  - BT scan is OFF by default. Platform must call `uni_bt_enable_new_connections` to enable it.
  - `platform->get_properties()` does nothing. To list/delete Bluetooth keys platform must call them explicitly.
  - Custom platform:
    - must call `uni_platform_set_custom(...)` before calling `uni_init()`
    - `uni_platform_custom_create()` removed
  - ESP32 main:
    - `uni_esp32_main()` removed.
    - Users must call the different "init" steps manually. Added helper `uni_esp32_init()`
    - Rationale: Pico W and ESP32 `main()` is almost the same. Easier to setup/customize/understand.
  - All examples updated
- New "unsafe" functions, useful to be called from `platforom->on_init_complete()`
  - `uni_bt_enable_new_connections_unsafe()`
  - `uni_bt_del_keys_unsafe()`
  - `uni_bt_list_keys_unsafe()`
- "PC_DEBUG" platform:
  - Removed the "PLATFORM_PC_DEBUG" option.
  - Instead, "pc_debug" still exists, but internally uses "custom" platform.
- Allowlist: rename `uni_bt_allowlist_allow_addr()` to `uni_bt_allowlist_is_allowed_addr()`
- Folder organization:
  - arch/: includes console, uart, property files (one for each: ESP32, Pico, Linux)
  - bt/: includes all the Bluetooth related files
  - platform/: includes all the platform files
  - parser:/ includes all the parser files
  - controller/: controller files
  - Added <uni.h> file, easier for 3rd party user. Just include that file, and
     don't worry about internal folder re-organization.
- Arduino platform:
  - Removed from "bluepad32" repo. Moved to the [Bluepad32 Arduino template][bluepad32-arduino-template] project.
- Virtual Device: Disabled by default. To enable it call: `void uni_virtual_device_set_enabled(true);`
- BTstack: Using latest develop branch. Hash: 4b3f8617054370b0e96650ee65edea1c23591ed4
- Docs: Documentation hosted in readthedocs: <https://bluepad32.readthedocs.io/>
- Docs: New Bluepad32 logo <https://github.com/ricardoquesada/bluepad32/blob/main/docs/images/bluepad32-logo.png>
- Docs: Using Github as the default repo, not Gitlab

[bluepad32-arduino-template]: https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template

### Fixed
- Switch: Parse calibration correctly. Does not crash. [Fixes Github Issue #61][github_issue_61]
- Arduino & NINA: Disconnect gamepad works Ok. Fixes [Github Issue #8][github_arduino_issue_8] and [Github Issue #50][github_issue_50]

[github_issue_61]: https://github.com/ricardoquesada/bluepad32/issues/61
[github_arduino_issue_8]: https://github.com/ricardoquesada/bluepad32-arduino/issues/8
[github_issue_50]: https://github.com/ricardoquesada/bluepad32/issues/50

## [3.10.3] - 2023-11-26
### New
- Atari Wireless Joystick: Add support for it

### Changed
- Allowlist: rename `uni_bt_allowlist_allow_addr()` to `uni_bt_allowlist_is_allowed_addr()`

## [3.10.2] - 2023-11-13
### Changed
- BTstack: Upgraded to latest develop branch as of 2023-11-13

### Fixed
- Xbox: Axis works Ok (fixed in latest BTstack)

## [3.10.1] - 2023-11-05
### New
- Arduino: API to handle keyboard
  - `Controller.isKeyboard()`
  - `Controller.isKeypressed(KeyboardKey)`
  - Sketch.cpp updated with Keyboard, Mouse, Gamepad, BalanceBoard code

### Changed
- Kconfig: Log Level "choice" and "int" as log level verbosity
  - Easier to handle in code. Doesn't "pollute" the Kconfig options

### Fixed
- Arduino: Correctly report Keyboard/Mouse types [Github Issue #48][github_issue_48]
- Keyboard: Typos in Keyboard constants

[github_issue_48]: https://github.com/ricardoquesada/bluepad32/issues/48

## [3.10.0] - 2023-11-02
### New
- Keyboard support (BETA, might change in the future):
  - Bluetooth keyboard is supported: Up to 10 keys pressed at the same time + modifiers
  - Unijoysticle:
    - Keyboard behaves like a joystick using arrow keys or ASDW
    - Press "Escape" to change Joystick ports
    - Press "Tab" to change Modes

### Changed
- Custom platform:
  - "uni_platform_custom_1" renamed to "my_platform"
  - Removed main/Kconfig. Keeping the configuration simpler.
- Arduino:
  - Virtual Devices disabled by default
  - Added `BP32.enableVirtualDevice(bool)` API in BP32 namespace
  - Sketch shows how to use it

### Fixed
- Tank Mouse (Bluetooth version) works Ok. Good for Amiga users using Unijoysticle.

## [v3.9.1] - 2023-10-13
### New
- "custom platform":
  - It is a new platform useful to start using Bluepad32 withou any dependency like Arduino.
  - Ideal if you are familiar with ESP-IDF and want full control

### Fixed
- Better DualShock3 support
- Better DualShock4 support
- Unijoysticle: Twin Stick mode works with virtual devices

## [v3.9.0] - 2023-09-16
### New
- Add touchpad support for DualSense and DualShock4 controllers.
  When one of these devices connect, it creates a mouse virtual device that is controlled
  by the "gamepad" controller. So, two connections will appear.
  - Can be enabled/disable from console using `virtual_device_enable`
  - Default value is enabled. Can be modified from `idf.py menuconfig`
  - Fixes issue: [Gitlab Issue #33][gitlab_issue_33]
- Unijoysticle A500: Virtual Device support to control Amiga mouse from DualSense and DualShock4.
  It is possible to "swap" the mouse and joystick port, and a connection from a new controller disconnects
  the virtual device (the mouse).
  - Fixes issue: [Gitlab Issue #22][gitlab_issue_22]
- New controller button supported:
  - `MISC_BUTTONS_CAPTURE`. From Arduino call `ctl->miscCapture()`
  - This is the "share" button in the Xbox 1914 model (the "3rd" button).
  - This is the "mute" button in the DualSense controller.
  - This is the "capture" button on Switch Pro controller.
  - Fixes issue: [Gitlab Issue #37][gitlab_issue_37]

### Changed
- Merge set/get console commands into one, making it a less polluted console.
  If no arguments are passed, it behaves like a getter.
  - `set_autofire_cps` / `get_autofire_cps` -> `autofire_cps`
  - `set_gamepad_mode` / `get_gamepad_mode` -> `gamepad_mode`
  - `set_bb_move_threshold` / `get_bb_move_threshold` -> `bb_move_threshold`
  - `set_bb_fire_threshold` / `get_bb_fire_threshold` -> `bb_fire_threshold`
  - `set_c64_pot_mode` / `get_c64_pot_mode` -> `c64_pot_mode`
  - `set_mouse_scale` / `get_mouse_scale` -> `mouse_scale`
  - `set_gap_security_level` / `get_gap_security_level` -> `gap_security_level`
  - `set_gap_periodic_inquiry` / `get_gap_peridic_inquiry` -> `gap_periodic_inquiry`
  - `set_ble_enabled` / `get_ble_enabled` -> `ble_enable`
  - `set_incoming_connections_enabled` / `incoming_connections_enable`
- Constants in uni_gamepad.h:
  - `MISC_BUTTONS_BACK` -> `MISC_BUTTONS_SELECT` (`ctl->miscSelect()`)
  - `MISC_BUTTONS_HOME` -> `MISC_BUTTONS_START` (`ctl->miscStart()`)
  - The old constants / functions are still present, but are deprecated.
  - The rationale is that these names should be easier to remember.

### Fixed
- Switch: Fix crash while parsing IMU data ([Bug GH44][github_issue_44])
- Xbox: Maps R1/R2 to Brake/Gas ([Bug GL34][gitlab_issue_34])
- Xbox: on FW 5.x, "select" button is mapped ([Bug GHBA07][github_ba_issue_7])
- Xbox / Stadia / Steam (other BLE devices): Connection is more reliable.
  - Fixes issue: [Gitlab Issue #35][gitlab_issue_35]

[gitlab_issue_22]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/22
[gitlab_issue_33]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/33
[gitlab_issue_34]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/34
[gitlab_issue_35]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/35
[gitlab_issue_37]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/37
[github_issue_44]: https://github.com/ricardoquesada/bluepad32/issues/44
[github_ba_issue_7]: https://github.com/ricardoquesada/bluepad32-arduino/issues/7

## [v3.8.3] - 2023-08-09
### Fixed
- Allowlist: Don't show "error" message when there is no error.

## [v3.8.2] - 2023-08-02
### Changed
- Unijoysticle: 800XL uses port one as default for joystick and port 2 as default for mouse.
  Each "variant" can easily specify the default ports.

### Fixed
- Allowlist: Can be called from C++ (Arduino).

## [v3.8.1] - 2023-07-30
### New
- Bluetooth Allowlist addresses:
  This feature, when enabled, will allow or decline controller connections based on their address.
  This is useful when using multiple devices running Bluepad32, and you want that certain controller
  connects to only a particular device.
  Useful for "parties", or at teaching classes.
  The following console commands were added to control the allowlist:
  - `allowlist_list`: List addresses in the allow list.
  - `allowlist_add <address>`: Add address to allow list.
  - `allowlist_remove <address>`: Remove address from allow list.
  - `allowlist_enable <0 | 1>`: Enable / Disable the allow list feature

### Changed
- BTstack: Upgraded to 1.5.6.3 (master branch as of July 29)

### Fix
- Unijoysticle: Misc and back buttons have debouncer per device.
  When multiple controllers are connected, debounce works as expected.
  Before, a `static` variable was used, and was reused by all connected
  controllers.
- Unijoysticle: Improved debug messages when trying to swap ports.

## [v3.8.0] - 2023-07-25
### New
- Unijoysticle: Add support for Unijoysticle 2 800XL board.
  Features:
   - Mouse: default AtariST.
   - 2nd and 3rd buttons work. Using [Joy 2B+ protocol][joy2bplus]
   - Twin Stick
   - and the rest of the Unijoysticle features.

[joy2bplus]: https://github.com/ascrnet/Joy2Bplus

### Changed
- Unijoysticle: "Enhanced mode" renamed to "Twin Stick" mode.
  Seems to be the name that has been used for games like Robotron 2084.
- Unijoysticle: Refactor. Each board has its own file, and its own "class".
  Easier to extend / maintain.

### Fixed
- Wii + Accel mode: Buttons "A" and "Trigger" act as fire.
  Before "fire" was not mapped, and it was not possible to play games like Lemans.
- Unijoysticle: Twin Stick works with 2nd buttons

## [v3.7.3] - 2023-06-17
### New
- Nintendo Switch: Gyro/Accel is parsed
- Unijoysticle: Support Nintendo Balance Board.
  Left,Right,Up,Down: just "press" in the right place in the Balance Board.
  Fire: Both Left and Right must be "pressed".
  Threshold (weight) is configurable via the console using:
    - `set_bb_move_threshold`
    - `get_bb_move_threshold`
    - `set_bb_fire_threshold`
    - `get_bb_fire_threshold`
- [![Video for reference](https://img.youtube.com/vi/Nj5fZlt_834/0.jpg)](https://www.youtube.com/watch?v=Nj5fZlt_834)
- Unijoysticle: Possible to swap ports in enhanced mode.
  Either press the gamepad or board "swap" button.
  The blue LED will blink once after the swap.
  The console command `list_devices` will display `mode=enhanced swapped` when it is swapped.
  Otherwise, it just shows `mode=enhanced`.

### Fixed
- Nintendo Switch: reports battery correctly.
- Nintendo Wii Balance Board: reports battery correctly.
- 8BitDo Zero 2:
  - "macOS" mode works. It identifies itself as a DualShock4, but it doesn't support
    report id 0x11, just report 0x01. Added parser for report id 0x01 is DS4 logic.
  - "keyboard" mode works. This is the only 8BitDo Zero 2 that reports "dpad" as "dpad".
    The rest of modes report the dpad as axis x & y.

## [v3.7.2] - 2023-05-20
### New
- Unijoysticle: very experimental paddle support.
  Only for developers. Not ready for public.
- Arduino: Add "disconnect" API.
  This API was already present on the NINA platform, but not in Arduino.
  They are in sync now ([Bug GH36][github_issue_36])

### Changed
- Wii Mote accelerometer exposed to Arduino/Nina/AirLift ([Bug GL28][gitlab_issue_28]).
  Before it was converted inside the Wii parser, preventing platforms to use the accelerometer
  data as they wish.
  Now the accelerometer data is exposed, and platform can do what they pleased.
  Unijoysticle platform parses it a converts it to joystick (previous behavior).


### Fixed
- GameSir T3s gamepad, when in iOS mode works.
  GameSir iOS mode is basically impersonating an Xbox Wireless with FW 4.8.
  Although the recommended for GameSir T3s, is to use it in Switch mode.

[gitlab_issue_28]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/28
[github_issue_36]: https://github.com/ricardoquesada/bluepad32/issues/36

## [v3.7.1] - 2023-04-30
### New
- DualSense / DualShock 4: Report Accelerometer and Gyro data
- Steam Controller: Add support for Steam Controller.
  - Disables "lizard" mode
  - Dpad, buttons, triggers, thumbstick, right pad supported.
  - Gyro/Accel: not supported ATM

### Fixed
- Xbox Adaptive Controller: Works as expected. Removed "unsupported usage" messages.
- Arduino / NINA / CircuitPython: Add APIs to read gyro / accel.

## [v3.7.0] - 2023-04-17
### New
- Arduino/NINA/AirLift: Added BP32.localBdAddress()([Bug GL5][bluepad32_arduino_issue5]).
  Returns the Local BD Address (AKA Mac Address).
  Updated example that shows how to use it.

[bluepad32_arduino_issue5]: https://gitlab.com/ricardoquesada/bluepad32-arduino/-/issues/5

### Fixed
- Arduino documentation: How to use Arduino IDE and other minor fixes

## [v3.7.0-rc.0] - 2023-04-15
### New
- Support ESP32-S3 / ESP32-C3
  Only BLE gamepads are supported, since BR/EDR is not supported on ESP32-S3 / ESP32-C3.
  Notice that ESP32-S3 has two cores, like ESP32. Bluetooth stack runs on one core,
  and Arduino Sketch runs on the other core.
  But ESP32-C3 only has one core. Meaning that both Bluetooth and Arduino Sketch share the same
  core. It works perfectly well for basic applications. But ESP32 or ESP32-S3 is a better option
  for advanced application.s
- Initial Support for Arduino IDE + ESP32.
  See [plat_arduino.md] for details.
  This means that you can use ESP32 / ESP32-S3 / ESP32-C3 with Bluepad32 from  Arduino IDE.

[plat_arduino.md]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/main/docs/plat_arduino.md

### Changed
- `uni_bluetooth_` functions renamed to `uni_bt_`
- `uni_bluetooth.[ch]` files renamed to `uni_bt.[ch]`
- `uni_bt_setup_get/set_properties` functions renamed to `uni_bt_get/set_properties`
- Moved all the BR/EDR logic from `uni_bluetooth.c` to `uni_bt_bredr`
- BR/EDR code is only compiled in ESP32, and not in ESP32-S3/C3 since it is not supported.
- `Makefile` files removed. It has been deprecated for a while.
  To compile Bluepad32 use `idf.py build` instead.

## [v3.6.2] - 2023-04-02
### New
- Add support for Sony Motion Controller
- Unijoysticle: Gamepad button "Start" triggers "gamepad change mode".
  It is useful to switch back and forth from "gamepad" to "mouse" mode without the need
  to press the "black button" on the Unijoysticle.
  - On V2+ / A500 it cycles between: `normal` -> `mouse` -> `enhanced` modes
  - On V2 / C64 it cycles between: `normal` -> `enhanced` modes.
- tools/sixaxixpairer: Supports PS3 Nav controllers

### Changed
- Arduino: `app_main` is part of Bluepad32
  This makes it easier to port Bluepad32 to Arduino IDE.
  Arduino user: need to update the example code, otherwise two `app_main` functions
  will get compiled and the linker wil complain about it.

## [v3.6.1] - 2023-03-04
### New
- C64: Add support for 5button mode, in addition to 3button and rumble.
  - Info about 5 button mode here: https://github.com/crystalct/5plusbuttonsJoystick

### Changed
- BLE is enabled by default
- Unijoysticle C64: C64GS buttons work.
  - Supports the official C64GS 2nd button
  - Supports the unofficial 3rd button

## [v3.6.0] - 2023-02-04
### New
- Arduino: Added ScrollWheel support in Mouse

### Changed
- Added option in "menuconfig" to enable Swap Button in Unijoysticle C64/FlashParty Edition
  - Enabled by default. Before the Swap Button was disabled.

### Fixed
  - Updated documentation regarding ESP-IDF version
  - Updated documentation regarding Controllers and BR/EDR and BLE

## [v3.6.0-rc1] - 2023-01-29
### New
- BLE support (experimental). Enable it from "idf.py menuconfig" or
  from console with `set_ble_enabled`. It is disabled by default
  Tested with:
  - Xbox Wireless Controller model 1914 with Firmware 5.15
  - Xbox Wireless Controller model 1708 with Firmware 5.15
  - Stadia Controller with BLE Firmware
  - Microsoft BLE mouse
  - Generic BLE mouse
 - Mouse: Add "scroll_wheel" property. Updated when the mouse scroll wheel is moved.

### Changed
- Refactored a bit the Bluetooth code to better support BLE
- Console command: "set_bluetooth_enabled" renamed to "set_incoming_connections_enabled"

### Fixed
- "disconnect" console command also deletes the connected device

## [v3.6.0-rc0] - 2023-01-16
### New
- Platforms: Receive "controller" instead of "gamepad". A controller can be a:
  gamepad, mouse, keyboard, balance board. And possible more. Battery is reported
  in the new Controller API.
- Arduino & NINA: Updated to support the new "controller" inteface. The old interface
  is still working, but the client calls the new one.

### Changed
- ESP-IDF: Added support for v5.0 but:
  - Arduino still requires v4.4 until esp32-core supports v5.0
  - NINA requires Legacy Flash SPI that was removed in v5.0
  - Unijoysticle, AirLift and MightyMiggy are the ones that can use v5.0 ATM
- BTstack: Updated to v1.5.5
- 8BitDo: Updated support for SN30 Pro FW v2 and M30

### Fixed
- Unijoysticle: "cycle" button works correctly when mouse and gamepad are
  connected at the same time.
- Switch, DualShock4, DualSense, Wii Balance Board: report battery status ([Github Issue #25][github_issue_25])

[github_issue_25]: https://github.com/ricardoquesada/bluepad32/issues/25

## [v3.5.2] - 2022-12-04
### New
- Arduino: Add "version" console command
- NINA/AirLift: Add command to disconnect gamepad ([Github Issue #26][github_issue_26])
- Console: Add "disconnect" command
- Console: Add "gpio_get", "gpio_set" commands. Useful for testing

### Changed
- BTstack: Updated to v1.5.4
- ESP-IDF: Tested with release/v4.4 hash (9269a536ac123677b165ff531369dfd1338ef103)

### Fixed
- Bluetooth state machine: Don't update name if connection is already established ([Issue #21][gitlab_bug_21])
- Doc: Update instructions about flashing Bluepad32 on UNI WiFi Rev.2
       Improved instructions regarding how to program the ESP32

[github_issue_26]: https://github.com/ricardoquesada/bluepad32/issues/26
[gitlab_bug_21]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/21

## [v3.5.1] - 2022-09-06
### Changed
- ESP-IDF: Using v4.4.2 hash 6d7877fcb8dda2ad971880b4e7ceda5b53c54019

### Fixed
- Unijoysticle can swap ports with "Select" button, in addition to the already suported "Home" button ([Github Issue #20][github_issue_20])
- Xbox: Don't enable left/right actuators for rumble.
  Trigger-right and Trigger-left actuatores are still enabled.
  This is to prevent the rumble-forever in 8BitDo controllers ([Gitlab Issue #10][gitlab_uni_issue_10])

[github_issue_20]: https://github.com/ricardoquesada/bluepad32/issues/20
[gitlab_uni_issue_10]: https://gitlab.com/ricardoquesada/unijoysticle2/-/issues/10

## [v3.5.0] - 2022-07-09
### New
- Console commands: New `list_bluetooth_keys` and `del_bluetooth_keys`

### Changed
- Console commands: Rename `devices` to `list_devices`

### Fixed
- Bluetooth LED works as expected in Unijoysticle

## [v3.5.0-rc0] - 2022-07-04
### New
- Mouse: Added support for:
  - LogiLink ID0078A
- Unijoysticle console commands:
  - Be able to change the autofire "click-per-second" with:
    `set_autofire_cps` and `get_autofire_cps`.
  - Be able to enable/disable incoming Bluetooth connections with:
    `set_bluetooth_enabled`
- Wii: Add support for Wii Balance Board

### Changed
- Console:
  - `set_mouse_scale(value)` where a higher value means faster movement.
     Before it meant "higher value == slower movement"
  - `version` shows Unijoysticle, Bluepad32 and Chip info
- Unijoysticle2 A500:
  Bluetooth LED is also used as status:
  Blink 1, 2 or 3 times means gamepad mode: "normal", "mouse" or "enhanced"
  "Mode button" cycles between those 3 modes, and the BT LED is used as feedback.
  "Swap button", when succeed, it blinks once. It fails it you try to swap when
  a gamepad is in enhanced mode.
- Unijoysticle2 auto-enable-bluetooth:
  If Bluetooth is disabled via the console, then the "auto" feature is disabled.
  The only way to enable bluetooth again is via the console, or with a reset.
- Unijoysticle: `auto` mouse emulation deleted. Use `amiga` or `atarist` instead.
- Platform: renamed `on_device_oob_event` to `on_oob_event`.
  Bluetooth enabled/disabled OOB event is sent ot the platforms.
- DualShock3: Can connect even on GAP Security Level 2.
  No need to change the GAP level.
- `uni_bluetooth_enable_new_connections()`, when disabled, paired devices can connect [Github Issue #21][github_issue_21]

[github_issue_21]: https://github.com/ricardoquesada/bluepad32/issues/21

## [3.5.0-beta1] - 2022-06-17
## New
- Arduino: Add "Console" class. An alternative to Arduino "Serial" that is
  compatible with Bluepad32's USB console.
- Property storage abstraction
  API that allows to store key/values in non-volatile areas
- Console. Added new commands:
  `get` / `set_gap_security_level`: Similar to the `menuconfig` option, but no need to recompile it.
  `get` / `set_gap_periodic_inquiry`: To change the new-connection/reconnection window
- NINA/AirLift: Add support for "enable/disable new bluetooth connections"
  - Add "type of controller" in GamepadProperty flags: gamepad, mouse or keyboard
- Mouse: Added support for:
  - Apple Magic Mouse 2nd gen
  - Logitech M-RCL124 MX Revolution
  - HP Z5000
  - Some BK3632-based mice
  - Updated list of [supported mice][supported_mice]

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
  SDP query timeout using BTstack timers instead of ad-hoc one.
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
  - Compiles & updated to new BTstack code
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
- Firmware: Using BTstack master-branch. Commit: dbb3cbc198393187c63748b8b0ed0a7357c9f190

### Removed
- Firmware: Name discovery disabled for the moment

## [0.3-beta] - 2019-07-27
### Added
- Firmware: Added Wii U Pro controller support.

### Changed
- Firmware: Using ESP-IDF v3.2.2
- Firmware: Using BTstack develop-branch. Commit: a4ea32feba8ca8a16509a75d3d80e8017ca2cf3b

## [0.2.1] - 2019-06-29
### Added
- Firmware: more verbose logs when detecting the type of device
- Firmware: Started Wii U Pro controller support. Not working yet.

### Changed
- Firmware: Gamepad names are fetched correctly.
- Firmware: Using BTstack develop-branch. Commit: 32b46fec1df77000b2e383d209074f4c2866ebdf
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
- Firmware: Updated link to <http://retro.moe/unijoysticle2>
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
- Firmware: Using BTstack develop-branch. Commit: 4ce43359e6190a70dcb8ef079b902c1583c2abe4

## [0.1.0] - 2019-04-15
### Added
- Firmware: v0.1.0
  - Using ESP-IDF v3.1.3. Commit: cf5dbadf4f25b395887238a7d4d8251c279afa8c
  - Using BTstack develop-branch. Commit: 8b22c04ddc425565c8e4002a6d4d26a53426a31f
