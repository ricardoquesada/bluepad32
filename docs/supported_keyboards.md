# Supported keyboards

Remember:
* Keyboard support is a **BETA feature**. Expect bugs.
* Most importantly, [file a bug when you find a bug][file_bug].

The list of **tested** keyboards is this one:

[file_bug]: https://gitlab.com/ricardoquesada/bluepad32/-/issues

## 8BitDo Retro Mechanical Keyboard

[![8BitDo Keyboard][8bitdo_keyboard_photo]][8bitdo_keyboard_link]

* Supports: all keys
* Purchase link: [8BitDo store][8bitdo_keyboard_link]
* Technical info: BLE, up to 

[8bitdo_keyboard_photo]: 
[8bitdo_keyboard_link]: https://www.8bitdo.com/retro-mechanical-keyboard/

## Apple Magic Keyboard 2 Model A1644

[![Apple Magic Keyboard A1644][magic_keyboard_a1644_photo]][magic_keyboard_a1644_link]

* Supports: all keys
* Purchase link: [eBay][magic_mouse_a1657_link]
* Technical info: BR/EDR, up to 4 simultaneus keys

[magic_keyboard_a1644_photo]: 
[magic_keyboard_a1644_link]: https://www.ebay.com/sch/i.html?_nkw=apple+magic+keyboard+a1644


## Microsoft Bluetooth Keyboard

[![Apple Magic Trackpad A1339][magic_trackpad_a1339_photo]][magic_trackpad_a1339_link]

* Supports: movement; left click
* Purchase link: [eBay][magic_trackpad_a1339_link]
* Bugs: Right/middle click not working [issue #17][gitlab_issue_17]

[magic_trackpad_a1339_photo]: https://lh3.googleusercontent.com/pw/AM-JKLV-N1Imj77WsdXc-wPtlwxYubS6BAb_X5ipI-gpk2XagClmdcbGyvPddp5F9zy6bsH9Q0ICrEvrd6PdF7EzmsfejmbI7WAaeMBQqI69UIYId5Ehvw4vDAC8CAHsaFpz4veUsgs2_jnjyix2wTdA7PjgAA=-no?
[magic_trackpad_a1339_link]: https://www.ebay.com/sch/i.html?_nkw=apple+magic+trackpad+a1339
[gitlab_issue_17]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/17


## Other supported

In theory any Bluetooth keyboard should work.

Most modern keyboards are BLE only, and BLE mice are supported,
although BLE is beta as of Bluepad32 v3.10.

Regarding non-BLE keyboards, some might work, some might not. See:

* https://github.com/espressif/arduino-esp32/issues/6193 (Please, go and say "please fix it")

Summary:

| Protocol   | Status      | Description                                 | Search for  |  Date  |
| ---------- | ------------|-------------------------------------------- | ----------- | ------ |
| BLE        | Should work | BLE **only**, AKA "Low Energy",  "BT 5.0"   | "BLE keyboard" | > 2010 |
| BR/EDR     | Should work | BR/EDR **only**, AKA "Classic", "BT 3.0"    | "Keyboard Bluetooth Windows XP" | < 2010 |
| Dual Mode  | Might work[*]  | Supports both BLE and BR/EDR             | "Tri-Mode BT 3.0, BT 5.0, 2.4Gz"  | > 2010 |

*: See [Issue#18][gitlab_issue_18], [Issue BK3632][bk3632_bug]


[bk3632_bug]: https://github.com/espressif/arduino-esp32/issues/6193
[gitlab_issue_18]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/18


