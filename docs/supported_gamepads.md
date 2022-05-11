# Supported controllers

![Supported gamepads][Supported gamepads]

[Supported gamepads]:https://lh3.googleusercontent.com/pw/AM-JKLXpmyDvNXZ_LmlmBSYObRZDhwuY6hHXXBzAicFw1YH1QNSgZrpiPWXZMiPNM0ATgrockqGf5bLsI3fWceJtQQEj2_OroHs1SrxsgmS8Rh4XHlnFolchomsTPVC7o5zi4pXGQkhGEFbinoh3-ub_a4lQIw=-no?authuser=0

## Bluetooth Classic controllers

List of supported Bluetooth Classic controllers

- DualSense (PS5)
- DUALSHOCK 4 (PS4) controller, both 2013 and 2016 editions
- DUALSHOCK 3 (PS3) controller
- Nintendo Switch Pro controller
- Nintendo Switch JoyCons
- Nintendo Wii U Pro controller
- Nintendo Wii Remote and Remote Motion Plus, including Nunchuk and Classic Controller
- Xbox Wireless controller (model 1708)
- 8BitDo controllers
- Android controllers
- iCade controllers
- Nimbus SteelSeries
- OUYA controllers
- Some TV remote controls, like the Amazon Fire TV

If you have a Bluetooth Classic gamepad and is not on that list, please file a bug.

## Bluetooth BLE: Not supported ATM

Not every Bluetooth HID controller is "Classic". Some of them are "BLE" (low energy).
"BLE" controllers are **not supported** at the moment, like:

- Xbox Wireless controller model 1914

# Supported gamepads

List of supported gamepads with supported features, known bugs, etc.

## Sony DualSense (PS5)

[![ddualsense_gamepad][dualsense]][11]

- [Must be in PC mode][25]
- Supported features: rumble, lightbar color, player LEDs

[dualsense]: https://lh3.googleusercontent.com/pw/ACtC-3d1CVA-e5srBTDhTD6D-3BSWYi7MncfECPj_9bQJfcGOAKIHrP6g6Ha7xAD0trE59eN-Qv_U33MklCFPskPWRLjfFI4ITHEol8RAmTYNHFNrA-gwhoXEn2ks_M7UDDbxiKhNdaPNXexxgj5zzOtpBjiyw=-no

## Sony DUALSHOCK 4 (PS4)

[![ds4_gamepad][dualshock_4]][14]

- [Must be in PC mode][25].
- Supports both the 2013 (CUH-ZCT1) and 2016 (CUH-ZCT2) editions
- Supported features: rumble, lightbar color
- Some clones are known to work Ok

[dualshock_4]: https://lh3.googleusercontent.com/_0sAxKXbSkk4g8rWJzTNxAirz2hD632jW4TGjGVOwjoac8sD4AfiN9PA1HdGWhm_ujcVygDlEG-LENPemF7IyFhqVsHgVHfCMVeFVjBbeDl-fUUjdMbRYAE8FiKdyWM_UBNUNmVy9Ro=-no

## Sony DUALSHOCK 3 (PS3)

[![ds3_gamepad][dualshock_3]][12]

- Supported features: rumble, player LEDs
- Requires that the [DS3 is manually paired][pair_ds3] to the Unijoysticle device.
- Disabled by default since it conflicts with Nintendo Switch. Enable it via `idf.py menuconfig`
- Some clones are known to work Ok

[dualshock_3]: https://lh3.googleusercontent.com/pw/ACtC-3dEBJYkdIpF5_icFUY7n7otgw5LPpLyviKS14JL2EJtuiDBt6Kk-XcKlIIP9JCgT0yxJVN1KuW-JICpilYKhMGfDxQt6vjJm8r_lRS1R9IyOX4iBlUQoIrflt9KmSjXBjcJlS81cmKaGB166HF608v5MA=-no
[pair_ds3]: pair_ds3.md

## Nintendo Switch Pro

[![Switch Pro][switch_pro_img]][switch_pro]

- Supported features: rumble, player LEDs
- Some clones are known to work Ok

[switch_pro]: https://en.wikipedia.org/wiki/Nintendo_Switch_Pro_Controller
[switch_pro_img]: https://lh3.googleusercontent.com/33hrGYM117T9pPrW0L-wr7bl0trLXooqmp4I78XV9vWkumHE8mK8Z_5KJZFzWKNpJg31gvrxHC0agF5BUgVr2f6awNYb98R-xPIWaawG6b0XwxHzm8hzz39Wnnv6qLmeEXsRaeoybsA=-no

## Nintendo Switch JoyCon

[![Switch JoyCon][switch_joycon_img]][switch_joycon]

Both Left and Right JoyCon are supported.

- Supported features: player LEDs
- They must be used in "horizontal" mode.
- Each JoyCon represents one gamepad. Cannot be used as a single/combined gamepad.
- Some clones are known to work Ok

[switch_joycon]: https://en.wikipedia.org/wiki/Joy-Con
[switch_joycon_img]: https://lh3.googleusercontent.com/pw/ACtC-3cN7JVNm3SvOM3IeKiAg4Ex03Dg7yxozBRNNV95Ycr_0J1eHF03_oDz8ydwpTZCFcPPfFuSzroK4UQ-3KcM0Y2XKew8deuYTqu_q5Q0nEEjA_KTQJCioVRU0IEbBGXHqy2ybtTP7EXp3p-7_RfjYK7Wjg=w360-no

## Nintendo Wii U Pro controller

[![Wii U Pro][wii_u_pro]][27]

- Supported features: player LEDs
- Some clones are known to work Ok

[wii_u_pro]: https://lh3.googleusercontent.com/kfKAySKzV-lLG7VmQGfCES1KuhtjBcTIfMzo59FgABcL7Ir9Tp7fQqrTP2iFqf8UVIhce1JhIXyBN_EH9eXpjlf5Q4b9NhhyxrFX9H0yVVRF0_pghjjz3pVqmY4uxS-FMgr7FC7egNo=w360-no

## Nintendo Wii Remote

[![Wii Remote][wii_remote]][29]

- Supports both Wii Remote (RVL-003) and Wii Remote Motion Plus (RVL-036)
- Supports 3 modes:
  - Sideways mode (default)
  - Accelerometer mode
  - Vertical mode
- Supported features: player LEDs
- To start pairing, use the "Sync" method (press "Sync" button).

### Sideways mode

- Default mode. No need to press anything special to enter this mode.
- Use Wii Remote in [horiontal position][wii_sideways].
- DPAD for regular movements.
- Button "1" for fire.
- Button "2" for jump.

### Accelerometer mode

- Enter this mode by pressing "A" (A == accelerometer, easy to remember) while connecting or reconnecting.
- Tilt up/down for up/down movements
- Rotate left/right for left/right movements
- Button "A" for fire.
- Button "shoulder" to jump.
- LED #3 will be on in this mode.

### Vertical mode

- Enter this mode by pressing button "+" while connecting or reconnecting.
- DPAD for regular movements.
- Button "shoulder" for fire.
- Button "A" to jump.
- LED #4 will be on in this mode.

[wii_remote]: https://lh3.googleusercontent.com/HtQgfME-mwm59JFfASHLwHRzvrfesY_FkJTZKfUSAD5XTsPhi-r7Veqvs3n5zr5igm5ug9WmL3rKWuoA7AToeXKIsHpSeMhFRfHfTi53mqu5boTHRtzQSrUex8kHEd8Ny6CS0bpUhEk=w380-no
[wii_sideways]: https://forums.dolphin-emu.org/Thread-how-to-hold-the-wii-remote

## Nintendo Wii Remote + Nunchuk

[![Wii Remote Nunchuk][wii_nunchuk_img]][wii_nunchuk]

When Nunchuk is attached to the Wii Remote, the Nunchuk can be used.

### Nunchuk as regular joystick

- Default mode. No need to press anything special to enter this mode.
- Use Nunchuk joystick for regular movement.
- Nunchuck button C for fire.
- Nunchuck button Z for jump.

Wii Remote can also be used, but both of them control the same joystick.

### Nunchuk as second joystick

To control both Commodore joysticks (useful when in *Unijoysticle Enhanced mode*), you
have to press the "+" button in the Wii Remote while connecting or reconnecting.

- Enter this mode by pressing button "+" while connecting or reconnecting.
- Use Nunchuk joystick for "right" movement.
- Nunchuck button C for "right" fire.
- Use Wii Remote DPAD for "left" movement.
- Use Wii Remote shoulder button for "left" fire.

[wii_nunchuk_img]: https://lh3.googleusercontent.com/DtCjBt0zrNEDBSgTmaP4BhPlDFfJePFtyBbLvqhEnxG5wjlIjbL1j3akOqbb4_tsSEuVGq1VaBZ_2T94TYNG8tjzxthE-Theo-gphrnG7AW8GEzd7vrmNqjVtGJjDcdhTnkJbsdCCFk=-no
[wii_nunchuk]: https://en.wikipedia.org/wiki/Wii_Remote#Nunchuk

## Nintendo Wii Remote + Classic Controller / Classic Controller Pro

![wii_classic_controller][wii_classic_controller_img]

A Nintendo Classic Controller or Classic Controller Pro can be used when it is attached
to the Wii Remote. When attached, the Wii Remote will be "disabled" and only the
Classic Controller can be used.

- LEDs: supported

[wii_classic_controller_img]: https://lh3.googleusercontent.com/nX-CyjcmorkW90mP8RybO_pJ7ezM4EJk1tsqkz8HAuLkHBAasccZzq5h-A74Ez-h7Zmv5hpsuBu5n66EeThwRUnLTIu8ffk2MstEMBjHiGrcNoyq-XAC9zeh97Kz8GDBDLqmujmm2J0=-no

## Xbox Wireless (model 1708)

[![Xbox One S][xbox_one_s]][13]

- Must be the *model 1708* (released in 2016) and not earlier. *Model 1797* might work. Newer models, like 1914, are not supported ATM.
- Supports both "old" (firmware 3.1) and "new" (firmware 4.8) Xbox One mappings
- Supported features: rumble

[xbox_one_s]: https://lh3.googleusercontent.com/YmONc-MhVZhnE8HVRgzH7FKSpT_29MLeIF70U5AfrcBuCtuNJ2Ln5xkmSpNqO0myrFpnDLbFvR2TRTRu0xcqvP3cLNaq1BBpruEAn-Z7vBbwzNtaXx7eQaLLF7aa8tt2Wa0IcYxeD08=-no

## Android

![Android][android_gamepad]

- Tested with: [ASUS][15], [Moga Pro 2][16], [Amazon Fire TV gamepads][17], [SteelSeries Status Duo][stratus_duo]

[android_gamepad]: https://lh3.googleusercontent.com/S3H1pEGYGT5aVTwF3ySWHF7vqbonDYR0UxOLJBxFe5At6Q4AP_4TQUCaNOiEXD22U4H3C0lVP1E3m26H3QM4rIbgp1wysbQoSt1NpD61snlWES5N5zGUgx20c2sfFCKZL4w_Gl66Y1s=-no
[stratus_duo]: https://steelseries.com/gaming-controllers/stratus-duo

## SteelSeries Nimbus

[![Nimbus SteelSeries for iOS][nimbus_steelseries_img]][nimbus]

- Tested with Nimbus SteelSeries for iOS.

[nimbus_steelseries_img]: https://lh3.googleusercontent.com/QeK4QebBIw4O-vWuyc-oxTGT_eST6BZ_2y6R9X5cuXPsQVQgZRdm5JEYs982dDKkYDs7AqCIGZyCQBRPJgLJ3ZxNqt_7KYMl9uKkWtmR0P89VbYgC4cMtkEFob2ihA8J6UxGHQ_4Tw0=-no
[nimbus]: https://steelseries.com/gaming-controllers/nimbus

## OUYA

[![OUYA 1st gen][ouya_1gen]][19]

- Only 1st gen is supported. It is unknown whether the newer version works Ok.

[ouya_1gen]: https://lh3.googleusercontent.com/FtbQLbt1QrzU59TTPQHIEarGZItlPik0bGWo40iDu0rnMwddCEwKMcy8LAe_fqzklaSKfMbt3-EvFJI4Vcoz3gSPTgC9MnTog3MyGfNWMc0Wq2Idq1kzjPOpRIS5OXeSqSSmIfGa5-w=-no
[issue_7]: https://gitlab.com/ricardoquesada/unijoysticle2/issues/7

## 8BitDo Family

[![8bitdo SN30 Pro][8bitdo_sn30_pro_img]][8bitdo_sn30_pro]

- Tested with: [8BitDo SN30 Pro][8bitdo_sn30_pro], [8BitDo Arcade Stick][8bitdo_arcade_stick],
  [8BitDo Lite][8bitdo_lite], [8BitDo NES30][8bitdo_nes30]
- All 8BitDo modes are supported: *Switch*, *Android*, *Windows* and *macOS*.
- Known issues: [issue #10][issue_10]

[8bitdo_sn30_pro_img]: https://lh3.googleusercontent.com/KX3q2kT7UZcEDGN8953RB7msPV343Gworbgaq-eLeKtqSzjTlOIUkoCf0QAf2GrnroQm0ADOCDgj3rK8EWpl2tfqScqExsiSorWZFf7lzA8-m1EoYYkVyjYaeFsSxzcC17kw9CkMNWQ=-no
[8bitdo_nes30]: https://www.google.com/search?q=8bitdo+nes30
[8bitdo_lite]: https://www.8bitdo.com/lite/
[8bitdo_sn30_pro]: https://www.8bitdo.com/sn30-pro-g-classic-or-sn30-pro-sn/
[8bitdo_arcade_stick]: https://www.8bitdo.com/arcade-stick/
[issue_10]: https://gitlab.com/ricardoquesada/unijoysticle2/issues/10

## iCade Family

### iCade Cabinet

[![iCade][icade_img]][icade_url]

- The original iCade cabinets works great.
- Might work with other controllers that support the iCade protocol.

[icade_img]: https://lh3.googleusercontent.com/owslbSElM2BJL5M9h3hqksaCJhjAGf7DyfEwRFxxqjdG3Y73D5V9ScI0zVNokmSJMO6jrHMuX7j437kB-ER7kCAzc8GPX4ir9MPEVdypuxMneoIuzp3yAY8DqvkItbSZY0hlaAUMPn8=-no
[icade_url]: https://www.ebay.com/sch/i.html?_from=R40&_trksid=m570.l1313&_nkw=icade+cabinet&_sacat=0&LH_TitleDesc=0&_osacat=0&_odkw=icade+cabinet

### iCade 8-bitty

[![iCade 8bitty][8bitty_img]][8bitty_url]

[8bitty_img]: https://lh3.googleusercontent.com/LKf4C5SDVlE1mx91vyh8S7AhaJgsgiBZlOuLSVlIKMllSzMbWqOj6lXFmYfPn8fFxBblsXmNyEFVreaJFaxKLjBVTTMhJ2k4Z6C-40c8MSSNCCCokPrhWS_rDQoHtVx01Xckqx-62FI=-no
[8bitty_url]: https://www.ebay.com/sch/i.html?_from=R40&_trksid=m570.l1313&_nkw=icade+8-bitty&_sacat=0&LH_TitleDesc=0&_osacat=0&_odkw=icade+8bitty

## Amazon Fire TV Remote

[![Amazon Fire TV Remote 1st gen][fire_tv_remote]][22]

- Only *1st gen* is supported

[fire_tv_remote]: https://lh3.googleusercontent.com/qnSdv7NM5et0vDhMQsRp7oMniqcjYxGKN9QJY0_gRWT6NXFrdWBf94JKNvP77abBZoykaSQOJBtXUnGW-Z1yF-MWn3q3t2Nt_TUVVV7a2HsPFjRc_DIuLh8tPiQNsEZSWDsb0z6Ys3k=-no

## Generic HID controllers

![Generic][generic_gamepad]

In general, any Bluetooth Classic (not BLE) controller that supports HID is supported, or should be easy to support it.

There many *generic* Bluetooth controllers that sometimes are offered as gifts in conferences. Usually these *generic* Bluetooth controllers have different connection modes. Use the HID one.

- Select Button + X + Right trigger to enter into Gamepad mode
- Select Button + X + Left trigger to enter in iCade mode.

[generic_gamepad]: https://lh3.googleusercontent.com/JG0sQGQ4lmFIITl_nincUDdPi-mlYPol-RSQrnoxsYZf1_cc16A4WMod_ttuLJoIQigvcZ_ZF6NiA7p54bBQP-Eu52b28mbjfVCwsMjuu_LCQB9Lj0k9e5UkW_PkRM12IB0HrW8ah0k=-no

# Supported mice

## Apple Magic Mouse A1296 (1st gen)

[![Apple Magic Mouse A1296][magic_mouse_a1296_photo]][magic_mouse_a1296_link]

* Supports: movement; left & right click
* Purchase link: [eBay][magic_mouse_a1296_link]

[magic_mouse_a1296_photo]: https://lh3.googleusercontent.com/pw/AM-JKLUDJzou9y7_78LP1E17L8C1gL6tHBfmfJ8NE3IzCfXAMwfEba3iYj01HaWyIg3ELUXu4mkGN2SM7rj7CjRiZiJnyRJxb4pHvH-oy-pC0X74k5qyz-v3-ywurhqYc-zQ3aHNToV_IU54SyH_i0valsPv6A=-no
[magic_mouse_a1296_link]: https://www.ebay.com/sch/i.html?_from=R40&_trksid=p2380057.m570.l1312&_nkw=apple+magic+mouse+a1296&_sacat=0 

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

[tecknet_2600dpi_link]: https://www.amazon.com/dp/B01EFAGMRA?psc=1&ref=ppx_yo2ov_dt_b_product_details
[tecknet_2600dpi_photo]: https://lh3.googleusercontent.com/pw/AM-JKLVFAtADCvTltimDJQWO0iXGf-RpVUQBx5LD1gJnVplYuCyjW3n-I-RTKGc-nWiYJGvGfFW2u_Uy4CCzdaxKjMhm2qebloiiniMXzn0IAUY_yPcPixwDwNoy-rwbd1tsArJqk1kyXM3GbP7gHFVBdzCXRg=-no


## Other supported / not supported mice

In theory any Bluetooth BR/EDR (AKA "Classic") mouse shuold work. Notice that Bluetooth LE (AKA BLE) mice are not supported ATM.

Most modern mice are BLE only. But there are still some modern mice that supports BR/EDR.
The problem is that mice based on BK3632 are not supported ATM due to a bug in ESP-IDF. See:

* https://github.com/espressif/arduino-esp32/issues/6193 (Please, go and say "please fix it")

So, which mice should work:

|            | Should work                                           |
| ---------- | ----------------------------------------------------- |
| Bluetooth  | BR/EDR (AKA "Classic", AKA "3.0")                     |
| Search for | "Mouse Bluetooth 3.0" or "Mouse Bluetooth Windows XP" |
|            | Bluetooth mice from early 2010's                      |

|           | Should NOT work                                                |
| --------- | -------------------------------------------------------------- |
| Bluetooth | BLE **only** (AKA "LE" or "5.0")                               |
| Avoid     | "Tri-Mode BT 3.0, BT 5.0, 2.4Gz". See [BK3632 bug][bk3632_bug] |


[bk3632_bug]: https://github.com/espressif/arduino-esp32/issues/6193


# Technical notes: Virtual gamepad

Internally, all controllers are converted to a virtual gamepad which is very similar
to the Android/Xbox One gamepads layout. The different parsers convert the physical
gamepads to the virtual gamepad.

Button are mapped based on physical position, and not on names. For example, 8bitdo N30 gamepad
uses the Nintendo layout, which is different than the Virtual Gamepad layout:

```
NES30 layout     Virtual Gamepad layout
    X                     Y
    ^                     ^
Y<-   ->A             X<-   ->B
    v                     v
    B                     A
 ```

 So, instead of honoring the button names, N30 will get remapped to match the
 virtual gamepad layout, meaning that:

- Button B -> A
- Button A -> B
- Button Y -> X
- Button X -> Y

## Virtual gamepad mappings

Many of the virtual buttons/pads are left unmapped, but could be mapped in the future.

![virtual_gamepad][1]

- 1: D-pad
- 2: Axis X & Y, Button Thumb Left
- 3: Axis Rx & Ry, Button Thumb Right
- 4: Button X
- 5: Button A
- 6: Button Y
- 7: Button B
- 8: Button Shoulder Right
- 9: Accelerator, Trigger Button Right
- 10: Brake, Trigger Button Left
- 11: Button Shoulder Left
- M1: Button System
- M2: Button Home
- M3: Button Back

[1]: https://lh3.googleusercontent.com/sfRd1qSHaxe4he4lt63Xjsr_ejmrthB00bPpIj4CwuUOyzKy3otIrdsPqhy_Y0U78Ibcw5bssuUOgKxNsvhvq6AQGlmigtj2tWA67HQHEaDU4tEmq850Z47rwRW9EzAhFGi6XrgUhUI=-no
[11]: https://www.playstation.com/en-us/accessories/dualsense-wireless-controller/
[12]: https://www.playstation.com/en-us/explore/accessories/dualshock-3-ps3/
[13]: https://www.xbox.com/en-US/xbox-one/accessories/controllers/xbox-wireless-controller
[14]: https://www.playstation.com/en-us/explore/accessories/gaming-controllers/dualshock-4/
[15]: https://www.asus.com/us/Home-Entertainment/Gamepad-TV500BG/
[16]: https://www.amazon.com/PowerA-MOGA-Pro-Power-Electronic-Games/dp/B00FB5RBJM?th=1
[17]: http://www.gamingonfire.com/2014-amazon-fire-gaming-controller-1st-gen/
[19]: https://www.amazon.com/OUYA-Wireless-Controller/dp/B002I0GX38?th=1
[22]: https://www.amazon.com/Alexa-Voice-Remote-Amazon-Stick/dp/B071D41YC3
[25]: https://www.techradar.com/how-to/gaming/how-to-use-the-ps4-dualshock-4-controller-on-a-pc-1309014
[27]: https://en.wikipedia.org/wiki/Wii_U_Pro_Controller
[29]: https://en.wikipedia.org/wiki/Wii_Remote
