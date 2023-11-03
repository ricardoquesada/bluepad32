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
* Technical info: BLE, up to 16 simultaneus keys.

[8bitdo_keyboard_photo]: https://lh3.googleusercontent.com/pw/ADCreHdkkpmH7M3NIo00JwCfMdVEN3EsBLx7Gy5HfGJoqHKsMn_35_8uIW1fkvQinagIlwNbOf68IFCK4KlVykVpYGFfywLrdcT_sU114VLKDxdGoCPAbXQCg6VizyPahEaioY2uuOAbEO9s1nMls-NZB-0Pvg
[8bitdo_keyboard_link]: https://www.8bitdo.com/retro-mechanical-keyboard/

## Apple Magic Keyboard 2 Model A1644

[![Apple Magic Keyboard A1644][magic_keyboard_a1644_photo]][magic_keyboard_a1644_link]

* Supports: all keys
* Purchase link: [eBay][magic_mouse_a1657_link]
* Technical info: BR/EDR, up to 4 simultaneus keys

[magic_keyboard_a1644_photo]: https://lh3.googleusercontent.com/pw/ADCreHfu_Mpr9vo72AFaAhpBgJ8VkLXDvoiJuGs9ZeEJAcNsW6vJTY3OD0HYsMEyHB43ZIHO_39q1xkpnee59qp2LCaB9yiZuXGlTERjH3NRFbwYJ1oFv_JJo47xUF6hKY9ImClyXCB0xmnbG-jdtH80WcWK4Q
[magic_keyboard_a1644_link]: https://www.ebay.com/sch/i.html?_nkw=apple+magic+keyboard+a1644


## Microsoft Bluetooth Keyboard

[![Microsoft][microsoft_photo]][microsoft_link]

* Supports: all keys
* Purchase link: [eBay][microsoft_link]
* Technical info: BLE, up to 10 simultaneus keys
* Unknown model

[microsoft_photo]: https://lh3.googleusercontent.com/pw/ADCreHd6sI3xeSxU4JFZ0wGpGVqUlPfhcwNHIZNRuTCNJEEaQm0r5qAAJl9hoA4nk0Pq_A7YP_24jF0UPT9SuH3YGD4HOOqA5Pq-Fu7fIDVbVOsD1EVERif272rngfH8XKyVSX7t2V3npU3A0yUUnL-rGGZBhA
[microsoft_link]: https://www.ebay.com/sch/i.html?_nkw=microsoft+bluetooth+keyboard


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


