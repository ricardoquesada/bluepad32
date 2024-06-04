# Supported keyboards

Remember:

* Keyboard support is a **BETA feature**. Expect bugs.
* Most importantly, [file a bug when you find a bug][file_bug].

The list of **tested** keyboards is this one:

[file_bug]: https://github.com/ricardoquesada/bluepad32/issues

## 8BitDo Retro Mechanical Keyboard

[![8BitDo Keyboard][8bitdo_keyboard_photo]][8bitdo_keyboard_link]

* Supports: all keys
* Purchase link: [8BitDo store][8bitdo_keyboard_link]
* Technical info: BLE, up to 16 simultaneous keys.

[8bitdo_keyboard_photo]: https://lh3.googleusercontent.com/pw/ADCreHdkkpmH7M3NIo00JwCfMdVEN3EsBLx7Gy5HfGJoqHKsMn_35_8uIW1fkvQinagIlwNbOf68IFCK4KlVykVpYGFfywLrdcT_sU114VLKDxdGoCPAbXQCg6VizyPahEaioY2uuOAbEO9s1nMls-NZB-0Pvg=-no

[8bitdo_keyboard_link]: https://www.8bitdo.com/retro-mechanical-keyboard/

## Apple Magic Keyboard 2 Model A1644

[![Apple Magic Keyboard A1644][magic_keyboard_a1644_photo]][magic_keyboard_a1644_link]

* Supports: all keys
* Purchase link: [eBay][magic_keyboard_a1644_link]
* Technical info: BR/EDR, up to four simultaneous keys

[magic_keyboard_a1644_photo]: https://lh3.googleusercontent.com/pw/ADCreHfu_Mpr9vo72AFaAhpBgJ8VkLXDvoiJuGs9ZeEJAcNsW6vJTY3OD0HYsMEyHB43ZIHO_39q1xkpnee59qp2LCaB9yiZuXGlTERjH3NRFbwYJ1oFv_JJo47xUF6hKY9ImClyXCB0xmnbG-jdtH80WcWK4Q=-no

[magic_keyboard_a1644_link]: https://www.ebay.com/sch/i.html?_nkw=apple+magic+keyboard+a1644

## Microsoft Designer Bluetooth Keyboard

[![Microsoft][microsoft_designer_photo]][microsoft_designer_link]

* Supports: all keys
* Purchase link: [eBay][microsoft_designer_link]
* Technical info: BLE, up to 10 simultaneous keys
* Model: Microsoft Designer Desktop

[microsoft_designer_photo]: https://lh3.googleusercontent.com/pw/ADCreHd6sI3xeSxU4JFZ0wGpGVqUlPfhcwNHIZNRuTCNJEEaQm0r5qAAJl9hoA4nk0Pq_A7YP_24jF0UPT9SuH3YGD4HOOqA5Pq-Fu7fIDVbVOsD1EVERif272rngfH8XKyVSX7t2V3npU3A0yUUnL-rGGZBhA=-no

[microsoft_designer_link]: https://www.microsoft.com/en/accessories/products/keyboards/designer-bluetooth-desktop

## Air Mouse with Touchpad Keyboard

[![AliExpress][aliexpress_kb_1_photo]][aliexpress_kb_1_link]

* Supports: all keys
* Purchase link: [AliExpress][aliexpress_kb_1_link]
* Technical info: BLE, up to 6 simultaneus keys
* Model: 2.4G Air mouse with Touchpad keyboard i8 English French Spanish Russian Backlit Mini Wireless Keyboard for PC
  Android TV Box

[aliexpress_kb_1_photo]: https://lh3.googleusercontent.com/pw/ADCreHcXVB7qwKG-A_FxCeM5ix2EGrPSmIuhK0dLwcGU8a4_My_ZCf1tdgALO9UvtNjVcSN934YnyS_pf90YXTxpoylQ4LpZ82uNxchzyVwONS_8gHKrTHFnwK8eo3I7p2iwILX1xNY99hYKZ1kDxTL_ttI-aQ=-no

[aliexpress_kb_1_link]: https://www.aliexpress.us/item/3256805614460629.html?spm=a2g0o.order_list.order_list_main.5.622d1802klrMWF&gatewayAdapt=glo2usa

## Misc Controllers

These "misc" controllers are reported as keyboards, although they might not look like one:

* [Tik Tok Remote Controller][tiktok_remote_controller]: See [Github Issue #68][github_issue_68] for mappings
* [Handlebar Media Controller][handlebar_media_controller]: See [Github Issue #104][github_issue_104] for mappings

[tiktok_remote_controller]: https://www.aliexpress.us/item/3256805596713243.html
[handlebar_media_controller]: https://www.aliexpress.us/item/3256804080291439.html
[github_issue_68]: https://github.com/ricardoquesada/bluepad32/issues/68
[github_issue_104]: https://github.com/ricardoquesada/bluepad32/issues/104

## Other supported

In theory, any Bluetooth keyboard should work.

Most modern keyboards are BLE only, and BLE keyboards are supported,
although BLE is beta as of Bluepad32 v4.1.

Regarding non-BLE keyboards, some might work, some might not. See:

* https://github.com/espressif/arduino-esp32/issues/6193 (Please, go and say "please fix it")

Summary:

| Protocol  | Status        | Description                               | Search for                       | Date   |
|-----------|---------------|-------------------------------------------|----------------------------------|--------|
| BLE       | Should work   | BLE **only**, AKA "Low Energy",  "BT 5.0" | "BLE keyboard"                   | > 2010 |
| BR/EDR    | Should work   | BR/EDR **only**, AKA "Classic", "BT 3.0"  | "Keyboard Bluetooth Windows XP"  | < 2010 |
| Dual Mode | Might work[*] | Supports both BLE and BR/EDR              | "Tri-Mode BT 3.0, BT 5.0, 2.4Gz" | > 2010 |

*: See [Issue#18][gitlab_issue_18], [Issue BK3632][bk3632_bug]


[bk3632_bug]: https://github.com/espressif/arduino-esp32/issues/6193

[gitlab_issue_18]: https://gitlab.com/ricardoquesada/bluepad32/-/issues/18


