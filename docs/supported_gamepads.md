# Supported gamepads

![Supported gamepads][Supported gamepads]

[Supported gamepads]:https://lh3.googleusercontent.com/pw/AMWts8BB7wT51jpn3HxWHuZLiEM2lX05gmTDsnldHszkXuYqxbowNvtxPtpbHh3CNjv1OBzeyadZjNLNBgE4w2tl2WmP8M9gGBCfWhzmZGQnHBlERSoy5W2dj6-EYmT84yteKTFjp4Jz2H3DgByFiKXaxfFC2g=-no

This page lists supported gamepads, extra features, and pairing steps.

## Sony DualSense (PS5)

[![ddualsense_gamepad][dualsense]][11]

Supported extra features:

| Rumble           | Lightbar color   | Player LEDs      | Adaptive trigger effects | Gyro / Accelerometer | virtual mouse (touchpad) |
|------------------|------------------|------------------|--------------------------|----------------------|--------------------------|
| :material-check: | :material-check: | :material-check: | :material-check:         | :material-check:     | :material-check:         |

- Pair instructions: Hold the **Create** button and press the **PS** button
    until the light bar blinks
    ([Sony official instructions][dualsense_pair_instructions]).
- Protocol: BR/EDR
- Supported models: DualSense (`054c:0ce6`), DualSense Edge (`054c:0df2`)

[dualsense]: https://lh3.googleusercontent.com/pw/ACtC-3d1CVA-e5srBTDhTD6D-3BSWYi7MncfECPj_9bQJfcGOAKIHrP6g6Ha7xAD0trE59eN-Qv_U33MklCFPskPWRLjfFI4ITHEol8RAmTYNHFNrA-gwhoXEn2ks_M7UDDbxiKhNdaPNXexxgj5zzOtpBjiyw=-no

[dualsense_pair_instructions]: https://www.playstation.com/en-us/support/hardware/pair-dualsense-controller-bluetooth/

## Sony DualShock 4 (PS4)

[![ds4_gamepad][dualshock_4]][14]

Supported extra features:

| Rumble           | Lightbar color   | Gyro / Accelerometer | virtual mouse (touchpad) |
|------------------|------------------|----------------------|--------------------------|
| :material-check: | :material-check: | :material-check:     | :material-check:         |

- Supports both the 2013 (`CUH-ZCT1`) and 2016 (`CUH-ZCT2`) models.
- Supports compatible third-party clones.
- Pair instructions: Hold the **SHARE** button and press the **PS** button
    until the light bar flashes
    ([Sony official instructions][dualshock4_pair_instructions]).
- Protocol: BR/EDR

[dualshock_4]: https://lh3.googleusercontent.com/_0sAxKXbSkk4g8rWJzTNxAirz2hD632jW4TGjGVOwjoac8sD4AfiN9PA1HdGWhm_ujcVygDlEG-LENPemF7IyFhqVsHgVHfCMVeFVjBbeDl-fUUjdMbRYAE8FiKdyWM_UBNUNmVy9Ro=-no

[dualshock4_pair_instructions]:  https://www.playstation.com/en-us/support/hardware/ps4-pair-dualshock-4-wireless-with-pc-or-mac/

## Sony DualShock 3 (PS3)

[![ds3_gamepad][dualshock_3]][12]

Supported extra features:

| Rumble           | Player LEDs      | Gyro / Accelerometer |
|------------------|------------------|----------------------|
| :material-check: | :material-check: | :material-check:     |

- Requires [manual USB pairing][pair_ds3] before first Bluetooth connection.
- Supports compatible third-party clones.
- Protocol: BR/EDR

[dualshock_3]: https://lh3.googleusercontent.com/pw/ACtC-3dEBJYkdIpF5_icFUY7n7otgw5LPpLyviKS14JL2EJtuiDBt6Kk-XcKlIIP9JCgT0yxJVN1KuW-JICpilYKhMGfDxQt6vjJm8r_lRS1R9IyOX4iBlUQoIrflt9KmSjXBjcJlS81cmKaGB166HF608v5MA=-no

[pair_ds3]: pair_ds3.md

## Sony Motion Controller

[![Sony Motion Controller][psmove_img]][psmove_ebay]

Supported extra features:

| Rumble           | LED              | Gyro / Accelerometer |
|------------------|------------------|----------------------|
| :material-check: | :material-check: | :material-check:     |

- Requires [manual USB pairing][psmove_pair] before first Bluetooth connection.
- Protocol: BR/EDR

[psmove_img]: https://lh3.googleusercontent.com/pw/AMWts8BzubFwU-wHy0En5sqr1Rezbu9wj_kAM4Bchu86-P7EgDOS2SSyt490IeGHYp2a50JrG6WgSXEVQyUX_qgdpsn8rSjCCdc968WhOAMGKOsqYe6ZjvG1hraET1VNzoVfZrNJkuWNuBpkezLDAt9exoXvZg=-no?authuser=0

[psmove_ebay]: https://www.ebay.com/sch/i.html?_from=R40&_trksid=p2380057.m570.l1313&_nkw=sony+playstation+ps3+move+motion+controller&_sacat=0

[psmove_pair]: https://github.com/thp/psmoveapi

## Nintendo Switch Pro

[![Switch Pro][switch_pro_img]][switch_pro]

Supported extra features:

| Rumble           | Player LEDs      | Gyro / Accelerometer |
|------------------|------------------|----------------------|
| :material-check: | :material-check: | :material-check:     |

- Supports compatible third-party clones.
- Protocol: BR/EDR

[switch_pro]: https://en.wikipedia.org/wiki/Nintendo_Switch_Pro_Controller

[switch_pro_img]: https://lh3.googleusercontent.com/33hrGYM117T9pPrW0L-wr7bl0trLXooqmp4I78XV9vWkumHE8mK8Z_5KJZFzWKNpJg31gvrxHC0agF5BUgVr2f6awNYb98R-xPIWaawG6b0XwxHzm8hzz39Wnnv6qLmeEXsRaeoybsA=-no

## Nintendo Switch Joy-Con

[![Switch JoyCon][switch_joycon_img]][switch_joycon]

- Supported models: Left and Right Joy-Cons, Nintendo Switch Online NES
    Controllers (Left `0x09` and Right `0x0a`)

Supported extra features:

| Player LEDs      | Gyro / Accelerometer | Orientation mode      |
|------------------|----------------------|-----------------------|
| :material-check: | :material-check:     | Sideways (horizontal) |

- Each Joy-Con acts as one standalone gamepad. Bluepad32 does not combine left
    and right Joy-Cons into one virtual controller.
- Supports compatible third-party clones.
- Protocol: BR/EDR

[switch_joycon]: https://en.wikipedia.org/wiki/Joy-Con

[switch_joycon_img]: https://lh3.googleusercontent.com/pw/ACtC-3cN7JVNm3SvOM3IeKiAg4Ex03Dg7yxozBRNNV95Ycr_0J1eHF03_oDz8ydwpTZCFcPPfFuSzroK4UQ-3KcM0Y2XKew8deuYTqu_q5Q0nEEjA_KTQJCioVRU0IEbBGXHqy2ybtTP7EXp3p-7_RfjYK7Wjg=w360-no

## Nintendo Switch 2 Pro

Supported extra features:

| Rumble           | Player LEDs      | Gyro / Accelerometer |
|------------------|------------------|----------------------|
| :material-check: | :material-check: | :material-check:     |

- Supported models: Nintendo Switch 2 Pro Controller (`057e:2069`)
- Supported features:
    - Connects over the Nintendo BLE GATT service (`ab7de9be-89fe-49ad-828f-118f09df7fd0`).
    - Supports D-pad, face buttons, calibrated thumbsticks, `ZL` and `ZR`
        triggers, `C` button, and `GL` and `GR` back grip buttons.
    - Supports 6-axis IMU (gyro and accelerometer), HD rumble, player LEDs,
        and battery reporting.
- Pair instructions: Hold the **Sync** button on the top of the controller
    until the player LEDs flash.
- Protocol: BLE

## Nintendo Switch 2 Joy-Con

- Supported models: Nintendo Switch 2 Joy-Con Left (`057e:2067`),
    Nintendo Switch 2 Joy-Con Right (`057e:2066`)

Supported extra features:

| Rumble           | Player LEDs      | Gyro / Accelerometer | Orientation mode      |
|------------------|------------------|----------------------|-----------------------|
| :material-check: | :material-check: | :material-check:     | Sideways (horizontal) |

- Each Joy-Con 2 acts as one standalone horizontal gamepad (`SL` maps to Left
    Shoulder, `SR` maps to Right Shoulder).
- Bluepad32 rotates the stick, face buttons, and 6-axis IMU into horizontal
    orientation. It does not combine two Joy-Cons into one pair.
- Pair instructions: Hold the **Sync** button on the inner rail until the LEDs
    flash.
- Protocol: BLE

## Nintendo Wii U Pro controller

[![Wii U Pro][wii_u_pro]][27]

Supported extra features:

| Player LEDs      | Rumble           |
|------------------|------------------|
| :material-check: | :material-check: |

- Supports compatible third-party clones.
- Protocol: BR/EDR

[wii_u_pro]: https://lh3.googleusercontent.com/kfKAySKzV-lLG7VmQGfCES1KuhtjBcTIfMzo59FgABcL7Ir9Tp7fQqrTP2iFqf8UVIhce1JhIXyBN_EH9eXpjlf5Q4b9NhhyxrFX9H0yVVRF0_pghjjz3pVqmY4uxS-FMgr7FC7egNo=w360-no

## Nintendo Wii Remote

[![Wii Remote][wii_remote]][29]

Supported extra features:

| Player LEDs      | Rumble           | Orientation modes                                                    |
|------------------|------------------|----------------------------------------------------------------------|
| :material-check: | :material-check: | Sideways mode (default) / Wheel mode (Accelerometer) / Vertical mode |

- Supports both Wii Remote (`RVL-003`) and Wii Remote MotionPlus (`RVL-036`).
- Supports 3 orientation modes:
    - Sideways mode (default)
    - Wheel mode (Accelerometer)
    - Vertical mode
- Supported features: player LEDs, rumble, and accelerometer.
- Pair instructions: Press the **Sync** button on the controller.
- Protocol: BR/EDR

### Sideways mode

- Default mode. Connect the controller without holding any buttons.
- Hold the Wii Remote in a [horizontal position][wii_sideways].
- Use the D-pad for movement.
- Press button **1** to fire.
- Press button **2** to jump.

### Wheel mode (Accelerometer)

- Hold button **A** while the controller connects or reconnects.
- Tilt the controller left or right to steer.
- Use the D-pad for up and down movement.
- Press button **A** to fire.
- Press button **B** (trigger) to jump.
- LED #3 turns on in this mode.

### Vertical mode

- Hold button **+** while the controller connects or reconnects.
- Use the D-pad for movement.
- Press button **B** (trigger) to fire.
- Press button **A** to jump.
- LED #4 turns on in this mode.

[wii_remote]: https://lh3.googleusercontent.com/pw/AM-JKLVMaoR_vkTyY3z1WBu2ZkdnfcaRZ_hbti95vT1-V57NjMidxB8XacACXdZy_Qa-mAg_8vhv-zkV2CZpbW338qEUys0z1KF4iqdD25JygowZXN2OJ08GbYirPe-FjfQGMzKP7zVQOcg2M8d5jGIpf3zItA=-no

[wii_sideways]: https://forums.dolphin-emu.org/Thread-how-to-hold-the-wii-remote

## Nintendo Wii Remote + Nunchuk

[![Wii Remote Nunchuk][wii_nunchuk_img]][wii_nunchuk]

Attach a Nunchuk to the Wii Remote to enable analog stick and C/Z button inputs.

### Nunchuk as primary joystick

- Default mode. Connect the controller without holding any buttons.
- Use the Nunchuk stick for movement.
- Press Nunchuk button **C** to fire.
- Press Nunchuk button **Z** to jump.

Both the Nunchuk and the Wii Remote control the same joystick port in this mode.

### Nunchuk as second joystick

To control two Commodore joystick ports (in *Unijoysticle Enhanced mode*), hold
button **+** on the Wii Remote while it connects:

- Hold button **+** while the controller connects or reconnects.
- Use the Nunchuk stick for right joystick movement.
- Press Nunchuk button **C** for right fire.
- Use the Wii Remote D-pad for left joystick movement.
- Press the Wii Remote **B** trigger for left fire.

[wii_nunchuk_img]: https://lh3.googleusercontent.com/DtCjBt0zrNEDBSgTmaP4BhPlDFfJePFtyBbLvqhEnxG5wjlIjbL1j3akOqbb4_tsSEuVGq1VaBZ_2T94TYNG8tjzxthE-Theo-gphrnG7AW8GEzd7vrmNqjVtGJjDcdhTnkJbsdCCFk=-no

[wii_nunchuk]: https://en.wikipedia.org/wiki/Wii_Remote#Nunchuk

## Nintendo Wii Remote + Classic Controller / Classic Controller Pro

![wii_classic_controller][wii_classic_controller_img]

Attach a Nintendo Classic Controller or Classic Controller Pro to the Wii
Remote. When you attach the Classic Controller, Bluepad32 reads inputs from the
Classic Controller instead of the Wii Remote buttons.

- LEDs: supported

[wii_classic_controller_img]: https://lh3.googleusercontent.com/nX-CyjcmorkW90mP8RybO_pJ7ezM4EJk1tsqkz8HAuLkHBAasccZzq5h-A74Ez-h7Zmv5hpsuBu5n66EeThwRUnLTIu8ffk2MstEMBjHiGrcNoyq-XAC9zeh97Kz8GDBDLqmujmm2J0=-no

## Nintendo Wii Remote + uDraw Tablet

Attach a Nintendo Wii uDraw GameTablet to the Wii Remote to use it as an input
device.

- Protocol: BR/EDR

## Nintendo Wii Balance Board

[![Balance Board][balance_board_img]][balance_board_ebay]

[balance_board_ebay]: https://www.ebay.com/sch/i.html?_from=R40&_trksid=p2380057.m570.l1313&_nkw=wii+balance+board&_sacat=0

- Supports all 4 corner load sensors.
- Supports the internal temperature sensor.
- Protocol: BR/EDR
- Supported models: Wii Balance Board (`057e:0306`)

[balance_board_img]: https://lh3.googleusercontent.com/pw/AMWts8A_VYYwZteJp4zrYRuvmYeVQrGFB14g7p7SzPRLPaCmENNA2kF5ylG-MZDOPrf5FAWXGd5oN--BxQ0viilUtZOpr4v0p0ELZLWXM064Z78_QlGW65Ks5N1yyOwUf4SmSRRCQHhezKKX5HyklUtlAsw9TA=-no?authuser=0

## Xbox Wireless (model 1708, 2 buttons)

[![Xbox 1708][xbox_1708]][xbox_1708_ebay]

Supported extra features:

| Rumble           | Trigger Rumble   |
|------------------|------------------|
| :material-check: | :material-check: |

- Uses the 2-button center layout.
- Requires *model 1708* (2016) or *model 1797*. Earlier Xbox One controllers
    do not support Bluetooth.
- Supports Xbox Controller firmware v3.1 (BR/EDR), v4.8 (BR/EDR), and v5.15 or
    newer (BLE):
    - [Update Xbox firmware][xbox_instructions_update]
    - [Revert Xbox firmware][xbox_instructions_revert]
- Pair instructions: Follow the model 1914 pairing steps below.
- Protocol:
    - BLE: Xbox firmware v5.x and newer
    - BR/EDR: Xbox firmware v3.x and v4.x

[xbox_1708_ebay]: https://www.ebay.com/sch/i.html?_from=R40&_trksid=p2380057.m570.l1313&_nkw=xbox+wireless+controller+1708&_sacat=0

[xbox_1708]: https://lh3.googleusercontent.com/YmONc-MhVZhnE8HVRgzH7FKSpT_29MLeIF70U5AfrcBuCtuNJ2Ln5xkmSpNqO0myrFpnDLbFvR2TRTRu0xcqvP3cLNaq1BBpruEAn-Z7vBbwzNtaXx7eQaLLF7aa8tt2Wa0IcYxeD08=-no

[xbox_instructions_update]: https://support.xbox.com/en-US/help/hardware-network/controller/update-xbox-wireless-controller

[xbox_instructions_revert]: https://support.xbox.com/en-US/help/hardware-network/accessories/controller-firmware-reversion

## Xbox Wireless (model 1914, 3 buttons)

[![Xbox 1914][xbox_1914]][xbox_1914_ebay]

Supported extra features:

| Rumble           | Trigger Rumble   |
|------------------|------------------|
| :material-check: | :material-check: |

- Uses the 3-button center layout (*model 1914* and compatible models).
- Requires Xbox Controller firmware v5.15 or newer:
    - [Update Xbox firmware][xbox_instructions_update]
    - [Revert Xbox firmware][xbox_instructions_revert]
- Pair instructions:
    - Press the **Xbox** button to turn on the controller.
    - Hold the **Pair** button for 3 seconds and release it.
    - Bluepad32 connects automatically when it receives the BLE advertisement.
- Protocol: BLE

[xbox_1914_ebay]: https://www.ebay.com/sch/i.html?_from=R40&_trksid=p2380057.m570.l1313&_nkw=xbox+wireless+controller+1914&_sacat=0

[xbox_1914]: https://lh3.googleusercontent.com/pw/AMWts8BjOU-jTHHhK6S7PFfyvRpBr8FlUhKdoebHK83tp536cRWaX2yfo1CiXnDyWe8RGg7CMtjc4DiU-Fji0Er-88Vtr7RXSGfF3xKoel6VRmogMwSWWdKbQswxzB3wULe06nOYRrVpcqJUkgeKF4hotwyxaw=-no?authuser=0

[github_issue_69]: https://github.com/ricardoquesada/bluepad32/issues/69

## Xbox Adaptive Controller

[![Xbox adaptive][xbox_adaptive_img]][xbox_adaptive_site]

- Requires Xbox Controller firmware v5.15 or newer.
- Pair instructions:
    - Press the **Pair** button on the back of the controller.
    - Wait until the Xbox logo blinks.
    - Bluepad32 connects automatically when it receives the BLE advertisement.
- Protocol: BLE

[xbox_adaptive_site]: https://www.xbox.com/en-US/accessories/controllers/xbox-adaptive-controller

[xbox_adaptive_img]: https://lh3.googleusercontent.com/pw/AJFCJaVs47b7HLG5IR7uBp3Gyi9XOiSaSZgW2Ux1_qgdJpPayy5gHRt5sg0atyjayhOMFI9cobKHtrsEqHdunrCj6V3sxHuFvAZbLhVHgssRvqW0FFA3KMlV71zrRDNEueQv3-Jb7IpfFZJJKUmCtwCpOjEobg=-no?authuser=0

## Android

![Android][android_gamepad]

- Tested controllers: [ASUS][15], [Moga Pro 2][16], [Amazon Fire TV gamepads][17],
    [SteelSeries Stratus Duo][stratus_duo], and
    [DIY ESP32 gamepad][diy_esp32_gamepad].
- Protocol: Most Android gamepads use BR/EDR, and some use BLE.

[android_gamepad]: https://lh3.googleusercontent.com/S3H1pEGYGT5aVTwF3ySWHF7vqbonDYR0UxOLJBxFe5At6Q4AP_4TQUCaNOiEXD22U4H3C0lVP1E3m26H3QM4rIbgp1wysbQoSt1NpD61snlWES5N5zGUgx20c2sfFCKZL4w_Gl66Y1s=-no

[stratus_duo]: https://steelseries.com/gaming-controllers/stratus-duo

[diy_esp32_gamepad]: https://github.com/lemmingDev/ESP32-BLE-Gamepad

## Steam Controller

[![Steam Controller][steam_controller_img]][steam_ebay]

Supported extra features:

| Gyro / Accelerometer |
|----------------------|
| :material-check:     |

- Flash the controller with the [Steam Bluetooth firmware][steam_instructions]
    before use.
- Supported models: Steam Controller (`28de:1106`)
- Supported features:
    - Disables lizard mode (keyboard and mouse emulation).
    - Supports D-pad, triggers, buttons, thumbstick, right pad, and 6-axis BLE
        IMU (gyro and accelerometer).
- Pair instructions:
    - Press **Y** + **Steam** button.
- Protocol: BLE

[steam_ebay]: https://www.ebay.com/sch/i.html?_from=R40&_trksid=p2380057.m570.l1313&_nkw=steam+controller+1001&_sacat=0

[steam_instructions]: https://help.steampowered.com/en/faqs/view/1796-5FC3-88B3-C85F

[steam_controller_img]:  https://lh3.googleusercontent.com/pw/AJFCJaX2KZ4NOFbcc0QuI-qgHReYsfsogpRIL1--86cZsMzrVmpDKuFPNHyGKeFXaCiQLtdCSey0SaH9fcj-OG7zDLUGZuZQ1B6wbLQ-pQrD883iMdH6g7fT7oQ-HfVkQXQGH-ZQRTD-LGITxIdI3Gx6VvdD-A=-no

## Steam Controller (2nd Gen / "Triton", 2026)

Supported extra features:

| Rumble           | Gyro / Accelerometer |
|------------------|----------------------|
| :material-check: | :material-check:     |

- Supported models: Steam Controller 2nd Gen ("Triton") (`28de:1303` BLE,
    `28de:1302` USB, `28de:1304` Wireless Puck, `28de:1305` Nereid)
- Supported features:
    - Connects over the Valve BLE GATT service (`100F6C32-1735-4313-B402-38567131E5F3`) and disables lizard mode.
    - Supports D-pad, analog triggers, face buttons, dual thumbsticks, trackpad
        clicks, 6-axis IMU (gyro and accelerometer), rumble, and battery
        reporting.
- Protocol: BLE

## Stadia Controller

[![Stadia Controller][stadia_controller]][stadia_ebay]

Supported extra features:

| Rumble           |
|------------------|
| :material-check: |

- Flash the controller with the [Stadia Bluetooth firmware][stadia_instructions]
    before use.
- Pair instructions:
    - Press **Y** + **Stadia** button.
- Protocol: BLE

[stadia_ebay]: https://www.ebay.com/sch/i.html?_from=R40&_trksid=p2380057.m570.l1313&_nkw=stadia+controller&_sacat=0

[stadia_instructions]: https://stadia.google.com/controller/index_en_US.html

[stadia_controller]: https://lh3.googleusercontent.com/pw/AMWts8BsCTnzKGc0UEQ7JaXXG84UIwCwXMFOnF0_JC6pTKptxh8xFIxyxsilC1hZmJ8z9U102-MR60ecBoHiHCSH5hINvsWcQ1Q4l1iFgJaMISAawpCgJlEShlAg0-SIw9rjAmTrSXhEHo0uUNZzFV9MVmxUBA=-no?authuser=0

## SteelSeries Nimbus

[![Nimbus SteelSeries for iOS][nimbus_steelseries_img]][nimbus]

- Tested model: SteelSeries Nimbus for iOS.
- Protocol: BR/EDR

[nimbus_steelseries_img]: https://lh3.googleusercontent.com/QeK4QebBIw4O-vWuyc-oxTGT_eST6BZ_2y6R9X5cuXPsQVQgZRdm5JEYs982dDKkYDs7AqCIGZyCQBRPJgLJ3ZxNqt_7KYMl9uKkWtmR0P89VbYgC4cMtkEFob2ihA8J6UxGHQ_4Tw0=-no

[nimbus]: https://steelseries.com/gaming-controllers/nimbus

## OUYA

[![OUYA 1st gen][ouya_1gen]][19]

- Supports the 1st-generation OUYA controller.
- Protocol: BR/EDR

[ouya_1gen]: https://lh3.googleusercontent.com/FtbQLbt1QrzU59TTPQHIEarGZItlPik0bGWo40iDu0rnMwddCEwKMcy8LAe_fqzklaSKfMbt3-EvFJI4Vcoz3gSPTgC9MnTog3MyGfNWMc0Wq2Idq1kzjPOpRIS5OXeSqSSmIfGa5-w=-no

## 8BitDo Family

[![8bitdo SN30 Pro][8bitdo_sn30_pro_img]][8bitdo_sn30_pro]

Supported extra features:

| Model        | Rumble           | Player LEDs      | Gyro / Accelerometer |
|--------------|------------------|------------------|----------------------|
| Ultimate     |                  | :material-check: | :material-check:     |
| SN30 Pro     | :material-check: | :material-check: |                      |
| Pro 2        | :material-check: | :material-check: |                      |
| Lite         |                  | :material-check: |                      |
| Lite 2       |                  | :material-check: | :material-check:     |
| M30          |                  | :material-check: |                      |
| Arcade Stick |                  |                  |                      |
| Zero 2       |                  |                  |                      |
| NES30        |                  |                  |                      |

- Tested models: [8BitDo SN30 Pro][8bitdo_sn30_pro],
    [8BitDo Pro 2][8bitdo_pro_2], [8BitDo Arcade Stick][8bitdo_arcade_stick],
    [8BitDo Lite][8bitdo_lite], [8BitDo NES30][8bitdo_nes30],
    [8BitDo M30][8bitdo_m30], [8BitDo Zero 2][8bitdo_zero2], and
    [8BitDo Ultimate Controller][8bitdo_ultimate].
- Supports all 8BitDo modes: *Switch*, *Android*, *Windows*, and *macOS*.
- Protocol: BR/EDR

[8bitdo_sn30_pro_img]: https://lh3.googleusercontent.com/KX3q2kT7UZcEDGN8953RB7msPV343Gworbgaq-eLeKtqSzjTlOIUkoCf0QAf2GrnroQm0ADOCDgj3rK8EWpl2tfqScqExsiSorWZFf7lzA8-m1EoYYkVyjYaeFsSxzcC17kw9CkMNWQ=-no

[8bitdo_nes30]: https://www.google.com/search?q=8bitdo+nes30

[8bitdo_lite]: https://www.8bitdo.com/lite/

[8bitdo_sn30_pro]: https://www.8bitdo.com/sn30-pro-g-classic-or-sn30-pro-sn/

[8bitdo_arcade_stick]: https://www.8bitdo.com/arcade-stick/

[8bitdo_m30]: https://www.8bitdo.com/m30/

[8bitdo_zero2]: https://www.8bitdo.com/zero2/

[8bitdo_ultimate]: https://www.8bitdo.com/ultimate-bluetooth-controller/

[8bitdo_pro_2]: https://www.8bitdo.com/pro2/

## Atari VCS Joystick

[![Atari Joystick][atari_joystick_photo]][atari_joystick_link]

- Supports joystick axes and buttons.
- Reports joystick twist (paddle rotation) as `THROTTLE`.
- Protocol: BR/EDR

[atari_joystick_link]: https://atari.com/products/classic-joystick

[atari_joystick_photo]: https://lh3.googleusercontent.com/pw/ADCreHeBQRnL5M6Yu_GDtgtJCrTNqZPZp7u2RUuwi347V83ZWYE1L3bFAyR79CqpIpA0aI40xl2u4OhdR8NchCwIcvEdGpz4Qu-8cn2cHo1nKWO5ZyyN_QJgysYl9l0N_OkRw_xSIM0OjdtxaAskStxCbBqvFA=-no

## iCade Family

### iCade Cabinet

[![iCade][icade_img]][icade_url]

- Supports the original iCade cabinet and compatible iCade controllers.
- Protocol: BR/EDR

[icade_img]: https://lh3.googleusercontent.com/owslbSElM2BJL5M9h3hqksaCJhjAGf7DyfEwRFxxqjdG3Y73D5V9ScI0zVNokmSJMO6jrHMuX7j437kB-ER7kCAzc8GPX4ir9MPEVdypuxMneoIuzp3yAY8DqvkItbSZY0hlaAUMPn8=-no

[icade_url]: https://www.ebay.com/sch/i.html?_from=R40&_trksid=m570.l1313&_nkw=icade+cabinet&_sacat=0&LH_TitleDesc=0&_osacat=0&_odkw=icade+cabinet

### iCade 8-Bitty

[![iCade 8bitty][8bitty_img]][8bitty_url]

- Protocol: BR/EDR

[8bitty_img]: https://lh3.googleusercontent.com/LKf4C5SDVlE1mx91vyh8S7AhaJgsgiBZlOuLSVlIKMllSzMbWqOj6lXFmYfPn8fFxBblsXmNyEFVreaJFaxKLjBVTTMhJ2k4Z6C-40c8MSSNCCCokPrhWS_rDQoHtVx01Xckqx-62FI=-no

[8bitty_url]: https://www.ebay.com/sch/i.html?_from=R40&_trksid=m570.l1313&_nkw=icade+8-bitty&_sacat=0&LH_TitleDesc=0&_osacat=0&_odkw=icade+8bitty

## Amazon Fire TV Remote

[![Amazon Fire TV Remote 1st gen][fire_tv_remote]][22]

- Supports the *1st-generation* Fire TV Remote.
- Protocol: BR/EDR

[fire_tv_remote]: https://lh3.googleusercontent.com/qnSdv7NM5et0vDhMQsRp7oMniqcjYxGKN9QJY0_gRWT6NXFrdWBf94JKNvP77abBZoykaSQOJBtXUnGW-Z1yF-MWn3q3t2Nt_TUVVV7a2HsPFjRc_DIuLh8tPiQNsEZSWDsb0z6Ys3k=-no

## Miscellaneous controllers

The table below lists additional tested controllers:

- The table lists only verified emulation modes.
- Additional emulation modes for a controller appear on the row immediately
    below it.

| Model                                     | Emulation        | Protocol | Rumble           | Lightbar         | Player LEDs      | Gyro / Accelerometer | Notes                                      |
|-------------------------------------------|------------------|----------|------------------|------------------|------------------|----------------------|--------------------------------------------|
| [DATA FROG P02 (PS4)][datafrog_ps4]       | DualShock 4      | BR/EDR   | :material-check: | :material-check: |                  | :material-check:     |                                            |
| [DATA FROG S80 (Switch)][datafrog_switch] | Switch           | BR/EDR   | :material-check: |                  | :material-check: | :material-check:     |                                            |
|                                           | Xbox             | BR/EDR   | :material-check: |                  |                  | :material-check:     |                                            |
| [Gamesir T3s][gamesir_t3s]                | Switch           | BR/EDR   | :material-check: |                  |                  |                      |                                            |
| [Gamesir Nova Lite][gamesir_nova_lite]    | Switch / Android | BR/EDR   |                  |                  |                  |                      | [Tested by community][nova_lite_community] |
| [HS-SW510][hs_sw510]                      | Switch           | BR/EDR   | :material-check: |                  | :material-check: |                      |                                            |
|                                           | Android          | BR/EDR   |                  |                  |                  |                      |                                            |
| [Terios T3 / ShanWan BM-769][terios_t3]   | Android (iPhone) | BLE      |                  |                  |                  |                      |                                            |

[datafrog_ps4]: https://www.aliexpress.us/item/3256806233659084.html

[datafrog_switch]: https://www.aliexpress.us/item/3256805448827549.html

[gamesir_t3s]: https://www.gamesir.hk/products/gamesir-t3s-multi-platform-game-controller

[gamesir_nova_lite]: https://www.gamesir.hk/products/gamesir-nova-lite

[nova_lite_community]: https://discord.com/channels/775177861665521725/1265729139584012371

[hs_sw510]: https://new.tvc-mall.com/details/hs-sw510-wireless-controller-for-nintendo-switch-bluetooth-multi-platform-gamepad-joystick-with-turbo-function-black-white-sku681600362b.html

[terios_t3]: https://www.aliexpress.us/item/2251832602044731.html

## Bluetooth gamepads and their protocols

Most gamepads use `BR/EDR` (Bluetooth Classic), and a smaller number use `BLE`.
ESP32, Pico W, and Pico 2 W support `BR/EDR`. **ESP32-S3**, **ESP32-C3**,
**ESP32-C5**, **ESP32-C6**, and **ESP32-H2** do **not** support `BR/EDR`.

| Chip     | BR/EDR           | BLE              |
|----------|------------------|------------------|
| ESP32    | :material-check: | :material-check: |
| ESP32-S3 |                  | :material-check: |
| ESP32-C3 |                  | :material-check: |
| ESP32-C5 |                  | :material-check: |
| ESP32-C6 |                  | :material-check: |
| ESP32-H2 |                  | :material-check: |
| Pico W   | :material-check: | :material-check: |
| Pico 2 W | :material-check: | :material-check: |

Supported Bluetooth gamepads and their protocols:

| Gamepad                   | BR/EDR               | BLE                  |
|---------------------------|----------------------|----------------------|
| DualSense (PS5)           | :material-check:     |                      |
| DualShock 4 (PS4)         | :material-check:     |                      |
| DualShock 3 (PS3)         | :material-check:     |                      |
| Nintendo Switch Pro       | :material-check:     |                      |
| Nintendo Switch JoyCons   | :material-check:     |                      |
| Nintendo Switch 2 Pro     |                      | :material-check:     |
| Nintendo Switch 2 JoyCons |                      | :material-check:     |
| Nintendo Wii U Pro        | :material-check:     |                      |
| Nintendo Wii Remote       | :material-check:     |                      |
| Nintendo Balance Board    | :material-check:     |                      |
| Xbox Wireless             | :material-check:*    | :material-check:*    |
| Xbox Adaptive             |                      | :material-check:*    |
| Steam                     |                      | :material-check:**   |
| Steam (2nd gen / Triton)  |                      | :material-check:     |
| Stadia                    |                      | :material-check:***  |
| Android                   | :material-check:**** | :material-check:**** |
| 8BitDo                    | :material-check:     |                      |
| Atari                     | :material-check:     |                      |
| iCade                     | :material-check:     |                      |
| Nimbus                    | :material-check:     |                      |
| OUYA 1st gen              | :material-check:     |                      |

*: Xbox Controller Firmware v3.x and v4.x use BR/EDR. Firmware v5.x and newer
use BLE.

**: Requires the [Steam Controller Bluetooth firmware update][steam_instructions].

***: Requires the [Stadia Bluetooth firmware update][stadia_instructions].

****: Android controllers use either BR/EDR or BLE.

[1]: https://lh3.googleusercontent.com/sfRd1qSHaxe4he4lt63Xjsr_ejmrthB00bPpIj4CwuUOyzKy3otIrdsPqhy_Y0U78Ibcw5bssuUOgKxNsvhvq6AQGlmigtj2tWA67HQHEaDU4tEmq850Z47rwRW9EzAhFGi6XrgUhUI=-no

[11]: https://www.playstation.com/en-us/accessories/dualsense-wireless-controller/

[12]: https://www.playstation.com/en-us/explore/accessories/dualshock-3-ps3/

[14]: https://www.playstation.com/en-us/explore/accessories/gaming-controllers/dualshock-4/

[15]: https://www.asus.com/us/Home-Entertainment/Gamepad-TV500BG/

[16]: https://www.amazon.com/PowerA-MOGA-Pro-Power-Electronic-Games/dp/B00FB5RBJM?th=1

[17]: http://www.gamingonfire.com/2014-amazon-fire-gaming-controller-1st-gen/

[19]: https://www.amazon.com/OUYA-Wireless-Controller/dp/B002I0GX38?th=1

[22]: https://www.amazon.com/Alexa-Voice-Remote-Amazon-Stick/dp/B071D41YC3

[27]: https://en.wikipedia.org/wiki/Wii_U_Pro_Controller

[29]: https://en.wikipedia.org/wiki/Wii_Remote
