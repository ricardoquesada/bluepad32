# Supported mice

TL;DR: Only a few mice are supported at the moment. Eventually the list will grow.

Remember:
* Mouse support is a **BETA feature**. Expect bugs.
* Most importantly, [file a bug when you find a bug][file_bug].

So far, this is the list of recommended mouse to try:

[file_bug]: https://gitlab.com/ricardoquesada/bluepad32/-/issues

## Apple Magic Mouse A1296 (1st gen)

[![Apple Magic Mouse A1296][magic_mouse_a1296_photo]][magic_mouse_a1296_link]

* Supports: movement; left & right click
* Purchase link: [eBay][magic_mouse_a1296_link]

[magic_mouse_a1296_photo]: https://lh3.googleusercontent.com/pw/AM-JKLUDJzou9y7_78LP1E17L8C1gL6tHBfmfJ8NE3IzCfXAMwfEba3iYj01HaWyIg3ELUXu4mkGN2SM7rj7CjRiZiJnyRJxb4pHvH-oy-pC0X74k5qyz-v3-ywurhqYc-zQ3aHNToV_IU54SyH_i0valsPv6A=-no
[magic_mouse_a1296_link]: https://www.ebay.com/sch/i.html?_from=R40&_trksid=p2380057.m570.l1312&_nkw=apple+magic+mouse+a1296&_sacat=0

## Logitech M336 / M337 / M535

[![Logitech M535][logitech_m535_photo]][logitech_m535_link]

* Supports: movement; left, middle & right click
* Purchase link (It seems that M336/M337/M535 are the same mice, just with different model name):
  * M535: [Amazon][logitech_m535_link]
  * M337: [Amazon][logitech_m337_link]
  * M336: [NewEgg][logitech_m336_link]

[logitech_m535_link]: https://www.amazon.com/Logitech-M535-Bluetooth-Mouse-Wireless/dp/B0148NPJ3W
[logitech_m336_link]: https://www.newegg.com/logitech-m336/p/0TP-000C-00828
[logitech_m337_link]: https://www.amazon.com/910-004521-M337-BLUETOOTH-MOUSE-BLACK/dp/B017IW92J2
[logitech_m535_photo]: https://lh3.googleusercontent.com/pw/AM-JKLVraEYI1NNMrNSFopSAlgHXgYUvU7ibRshgS1KHSjQ494jArv0Wz8Z2dydepzQdwrVlOiwXGh2BBBC0llpfrJFSCf6NTlDl2gMgOYitbEtts3jVwHL2_p4hUmkx-HaBpXw_R6W99TSd3coqCFek20_EDA=-no

## Bornd C170B

[![Bornd C170B][bornd_c170b_photo]][bornd_c170b_link]

* Supports: movement; left, middle & right click
* Purchase link: [Amazon][bornd_c170b_link]

[bornd_c170b_link]: https://www.amazon.com/Bornd-C170B-Bluetooth-wireless-BLACK/dp/B009FD55SU
[bornd_c170b_photo]: https://lh3.googleusercontent.com/pw/AM-JKLWhgr0VyJ2LoErHi3U8dzedoILDyguhDJfYb86K3izqETUGyxtSfheRpxw-yD_dbbdYzoeHUT5oU_45XBjBBWFFWh-CENKZ0Xf29PIfiPMCFiz3lSaCFQET1-c6SqL2T8hECmvEmlweFmbXbEo5HmTYNw=-no

## TECKNET 2600DPI

[![TECKNET 2600DPI][tecknet_2600dpi_photo]][tecknet_2600dpi_link]

* Supports: movement; left, middle & right click
* Purchase link: [Amazon][tecknet_2600dpi_link]

[tecknet_2600dpi_link]: https://www.amazon.com/dp/B01EFAGMRA
[tecknet_2600dpi_photo]: https://lh3.googleusercontent.com/pw/AM-JKLVFAtADCvTltimDJQWO0iXGf-RpVUQBx5LD1gJnVplYuCyjW3n-I-RTKGc-nWiYJGvGfFW2u_Uy4CCzdaxKjMhm2qebloiiniMXzn0IAUY_yPcPixwDwNoy-rwbd1tsArJqk1kyXM3GbP7gHFVBdzCXRg=-no

## Adesso iMouse M300

[![Adesso iMouse M300][adesso_imouse_m300_photo]][adesso_imouse_m300_link]

* Supports: movement; left, middle & right click
* Purchase link: [Amazon][adesso_imouse_m300_link]


[adesso_imouse_m300_photo]: https://lh3.googleusercontent.com/pw/AM-JKLX_jhwfDQIeBdwFqGBt8h9AlP6MpiInG2Yreox0ADkvUmYIFC8x3ftoIVr7_JFk4OolkA7x50WyUyhteh_4sImUiwX18dmiB1hoO7FSJzAgJtC1V9uNlOzKKvask6lzEMIuVzdnfTgUe-OoyhZRyXcfaA=-no
[adesso_imouse_m300_link]: https://www.amazon.com/dp/B00CIZXX8Q

## Other supported / not supported mice

In theory any Bluetooth BR/EDR (AKA "Classic") mouse shuold work. Notice that mice that only support Bluetooth LE (AKA BLE) are not supported ATM.

Most modern mice are BLE only. But there are still some modern mice that support BR/EDR.
But not all modern BR/EDR mice work Ok. There is an bug in ESP32 with BK3632-based mice. See:

* https://github.com/espressif/arduino-esp32/issues/6193 (Please, go and say "please fix it")

So, which mice should work:

|            | Should work                                           |
| ---------- | ----------------------------------------------------- |
| Bluetooth  | BR/EDR (AKA "Classic", AKA "BT 3.0")                  |
| Search for | "Mouse Bluetooth 3.0" or "Mouse Bluetooth Windows XP" |
|            | Bluetooth mice from early 2010's                      |

|           | Should NOT work                                                |
| --------- | -------------------------------------------------------------- |
| Bluetooth | BLE **only** (AKA "LE", or "Low Energy" or "BT 5.0")           |
| Avoid     | "Tri-Mode BT 3.0, BT 5.0, 2.4Gz". See [BK3632 bug][bk3632_bug] |


[bk3632_bug]: https://github.com/espressif/arduino-esp32/issues/6193


