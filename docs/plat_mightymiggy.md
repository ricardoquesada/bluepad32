# MightyMiggy: Bluepad32 for Amiga/CD32

## What is MightyMiggy

MightyMiggy is version of Bluepad32 aimed at the Commodore Amiga line of computers.

MightyMiggy originated as a port of
SukkoPera's [OpenPSX2AmigaPadAdapter firmware](https://github.com/SukkoPera/OpenPSX2AmigaPadAdapter), so it has all of
its features and even more:

- Controls a joystick and a mouse with a single controller
- Controls two joysticks with two controllers
- Supports 2 buttons in joystick mode and 7 buttons in CD32 mode
- Has 4 different built-in button mappings and 4 more which can be customized at will
- Has 2 different button layouts in CD32 mode

While it can run on an unmodified Unijoysticle2 board (Rev F), to take advantage of all its features you will
need [an alternative version of the board](https://gitlab.com/SukkoPera/unijoysticle2), which plugs directly in an Amiga
500 or 1200. On the 600 you will have to use extension cables. Other models have not been tested yet.

The board and firmware retain backwards compatibility and **should** work wherever an Atari-style joystick is supported,
including the Commodore VIC-20, Commodore 16 (through an [adapter](https://github.com/SukkoPera/OpenC16JoyAdapter)),
Commodore 64, etc. Probably the board will not plug directly in these computers, but extension cables will always do the
trick. Note that on some platforms you might have to provide external power through the USB connector on your ESP32. On
Amigas this should not be necessary, but in doubt do it, it won't hurt.

DO NOT USE on MSX computers, these have a different electrical interface, even though it appears similar.

## Operating Modes

MightyMiggy should support all the controllers supported by the standard Bluepad32, which have the following general
layout:

![virtual_gamepad][1]

- 1: <kbd>D-Pad</kbd> (including <kbd>&uarr;</kbd> <kbd>&darr;</kbd> <kbd>&larr;</kbd> <kbd>&rarr;</kbd>)
- 2: <kbd>Left Analog</kbd>, Button <kbd>Thumb Left</bkd>
- 3: <kbd>Right Analog</kbd>, Button <kbd>Thumb Right</bkd>
- 4: Button <kbd>X</bkd>
- 5: Button <kbd>A</bkd>
- 6: Button <kbd>Y</bkd>
- 7: Button <kbd>B</bkd>
- 8: Button <kbd>Shoulder Right</bkd>
- 9: Accelerator, <kbd>Trigger Button Right</bkd>
- 10: Brake, <kbd>Trigger Button Left</bkd>
- 11: Button <kbd>Shoulder Left</bkd>
- M1: Button <kbd>System</bkd>
- M2: Button <kbd>Home</kbd>
- M3: Button <kbd>Back</kbd>

MightyMiggy supports up to two controllers at the same time. The first controller connecting will control the joystick
in Port 2 (the port controlling Player 1 on all Amiga games) and the mouse in Port 1. When a second controller connects,
it will control the joystick in Port 1. The first controller can regain control of the mouse by moving the <kbd>Right
Analog</kbd> stick. The second controller can then regain control of the second joystick by pressing a button on
the <kbd>D-Pad</kbd>.

The exact function of each button depends on the operating mode.

### Two-Button Joystick Mode

When the adapter is powered on, it defaults to Atari-style Two-Button Mode. While in this mode, the adapter supports
different button mappings, which have been carefully designed and tailored to different game genres. The mappings can be
switched by pressing <kbd>Back</kbd> in combination with other buttons. The player LED will blink quickly a few times to
indicate what mapping has been activated.

#### Standard Mapping: <kbd>Back</kbd> + <kbd>X</kbd>

Standard Mapping is the simplest mapping possible: both the D-Pad and <kbd>Left Analog</kbd> work as direction
buttons. <kbd>X</kbd> is <kbd>Button 1</kbd> and <kbd>A</kbd> is <kbd>Button 2</kbd>. This is the default mapping as it
should be usable just about in every game out there. It might not be the most natural these days, but it's the way the
game was meant to be played by the developers, thus it should never fail you.

Note that very few games were originally made to take advantage of two buttons, as even fewer controllers had that
many (!) those days. [Here is a list](http://eab.abime.net/showthread.php?t=57540) of Amiga games that somehow support
two buttons, if it can be any useful.

The player LED will blink once when this mapping is activated.

#### Racing Mapping 1: <kbd>Back</kbd> + <kbd>Y</kbd>

Racing Mapping 1 is useful for all those racing games that use <kbd>&uarr;</kbd> to accelerate and <kbd>&darr;</kbd> to
brake. These have been mapped to <kbd>X</kbd> and <kbd>A</kbd> respectively, which should make them much more natural to
play. When accelerating and braking at the same time, braking wins. <kbd>Left Analog</kbd> can be used to steer but its
vertical axis is ignored, in order to avoid accidental accelerating/braking. The D-Pad is fully functional and is handy
when moving through menus. <kbd>Button 1</kbd> can be found on <kbd>Right Shoulder</kbd>, <kbd>Right Trigger</kbd>
or <kbd>Y</kbd>, while <kbd>Button 2</kbd>  is on  <kbd>Left Shoulder</kbd>, <kbd>Left Trigger</kbd> or <kbd>B</kbd>.

This mode is probably best suited to games that do not involve shifting gears, as downshifting is usually performed
through <kbd>&darr;</kbd> + <kbd>Button 1</kbd> which is pretty cumbersome to achieve (<kbd>Y</kbd> + <kbd>A</kbd>).

The player LED will blink twice when this mapping is activated.

#### Racing Mapping 2: <kbd>Back</kbd> + <kbd>B</kbd>

Racing Mapping 2 is an alternative mapping for racing games that was inspired by GTA V. It lets you use <kbd>Trigger
Right</kbd> (or <kbd>Shoulder Right</kbd>) to accelerate and <kbd>Trigger Left</kbd> (or <kbd>Shoulder Left</kbd>) to
brake (which means they map to <kbd>&uarr;</kbd> and <kbd>&darr;</kbd>, respectively). <kbd>Button 1</kbd> is mapped to
its natural <kbd>X</kbd> position. Steering and the <kbd>D-Pad</kbd> work as in Racing Mode 1.

Accidentally, this control scheme was found out to be very comfortable with games that use <kbd>Button 1</kbd> to
accelerate and <kbd>&uarr;</kbd> and <kbd>&darr;</kbd> to shift gears. Since <kbd>&darr;</kbd> is probably used for
braking as well, it has also been mapped to <kbd>A</kbd>, while <kbd>Button 2</kbd> has been moved to <kbd>Y</kbd>.

The player LED will blink three times when this mapping is activated.

#### Platform Mapping: <kbd>Back</kbd> + <kbd>A</kbd>

Platform Mapping is very similar to Standard Mapping, it just makes jumping way easier on a joypad and more natural to
all the Mario players out there, by replicating <kbd>&uarr;</kbd> on <kbd>A</kbd>. Consequently, <kbd>Button 2</kbd> has
been moved to <kbd>Y</kbd>.

The player LED will blink four times when this mapping is activated.

#### Custom Mappings: <kbd>Back</kbd> + <kbd>Shoulder Right</kbd>/<kbd>Trigger Right</kbd>/<kbd>Shoulder Left</kbd>/<kbd>Trigger Left</kbd>

What if the built-in mappings are not enough? MightyMiggy allows you to make your own! You can have up to four different
ones, which are stored internally so that they can be recalled at any time. By default they behave similarly to the
Standard Mapping, but they can be customized so that any button produces either the press of a single button or even of
a button combo!

The programming procedure is as follows:

1. Press and hold <kbd>Back</kbd>, then press and hold one of <kbd>Shoulder Right</kbd>/<kbd>Trigger Right</kbd>/<kbd>
   Shoulder Left</kbd>/<kbd>Trigger Left</kbd> until the player LED starts blinking, finally release both buttons. You
   are now in Programming Mode.
2. Press the button you want to configure. The player LED will flash quickly a few times.
3. Press and hold the single button or button combo you want to be assigned to the button you pressed before. At this
   stage the <kbd>D-Pad</kbd> directions have their obvious meaning, while <kbd>X</kbd> represents <kbd>Button 1</kbd>
   and <kbd>A</kbd> represents <kbd>Button 2</kbd>. The player LED will again flash quickly a few times.
4. Release the button or combo you were holding.
5. Repeat steps 2-4 for every button you want to customize.
6. When you are done, press <kbd>Back</kbd> to store the mapping and leave Programming Mode. The player LED will stop
   blinking and you will be back to Two-Button Joystick Mode.

Note that a mapping you have just programmed is not activated automatically, so you will have to press <kbd>Back</kbd>
and one of <kbd>Shoulder Right</kbd>/<kbd>Trigger Right</kbd>/<kbd>Shoulder Left</kbd>/<kbd>Trigger Left</kbd> (and
release them quickly) to switch to it.

The Custom Mappings **cannot** be configured so that <kbd>&darr;</kbd> overrides <kbd>&uarr;</kbd> or so that the
vertical axis of <kbd>Left Analog</kbd> is ignored, still they might be useful here and there. For instance,
having <kbd>Button 1</kbd> + <kbd>&uarr;</kbd> on <kbd>A</kbd> and <kbd>Button 1</kbd> + <kbd>&darr;</kbd> on <kbd>
Y</kbd> makes the Amiga version of *Golden Axe* much more playable.

#### Commodore 64 Mode

<kbd>Button 2</kbd> on Commodore 64 usually behaves in the opposite way at the electrical level, with respect to the
other buttons. So a tweak can be enabled to invert the behavior of the button, use it if you find that your game of
choice always sees it pressed or if it triggers on release rather than on press.

Just hold <kbd>Back</kbd> and press <kbd>Home</kbd> briefly. The player LED will flash once when this tweak is enabled
and twice when it is disabled.

### Mouse Mode

In all operating modes, <kbd>Right Analog</kbd> on the first connected controller emulates the movements of a mouse.
Movement speed is somewhat proportional to how far the stick is moved.

The left and right mouse buttons are mapped to <kbd>Left Thumb</kbd> and <kbd>Right Thumb</kbd> respectively.

### CD<sup>32</sup> Controller Mode

When a game supports CD<sup>32</sup> joypads, MightyMiggy will automatically switch into this mode, which supports all
the 7 buttons of the original CD<sup>32</sup> controller.

By default, buttons are mapped as follows:

- <kbd>X</kbd>: Red
- <kbd>A</kbd>: Blue
- <kbd>B</kbd>: Yellow
- <kbd>Y</kbd>: Green
- <kbd>Shoulder Left</kbd>/<kbd>Trigger Left</kbd>: L
- <kbd>Shoulder Right</kbd>/<kbd>Trigger Right</kbd>: R
- <kbd>Home</kbd>: Home/Pause

If you press <kbd>Back</kbd>, the 4 main buttons get "rotated":

- <kbd>A</kbd>: Red
- <kbd>B</kbd>: Blue
- <kbd>Y</kbd>: Yellow
- <kbd>X</kbd>: Green

Both the D-Pad and <kbd>Left Analog</kbd> always work as direction buttons.

## Getting MightyMiggy firmware

### 1. Get a pre-compiled firmware

You can grab a precompiled firmware from here (choose latest version):

* https://github.com/ricardoquesada/bluepad32/releases

And download the `bluepad32-mightymiggy.zip`. It includes a README with instructions.

### 2. Or compile it yourself

Read [README.md][readme] for the ESP-IDF requirements. And choose `mightymiggy` as the target platform:

```sh
# Select MightyMiggy platform:
# Components config -> Bluepad32 -> Target Platform -> MightyMiggy
idf.py menuconfig

# Compile it
idf.py build
```

Please refer to the [general instructions][flashing] for more information on compiling and flashing.

[readme]: https://github.com/ricardoquesada/bluepad32/blob/main/README.md

[flashing]: firmware_setup.md

[1]: https://lh3.googleusercontent.com/sfRd1qSHaxe4he4lt63Xjsr_ejmrthB00bPpIj4CwuUOyzKy3otIrdsPqhy_Y0U78Ibcw5bssuUOgKxNsvhvq6AQGlmigtj2tWA67HQHEaDU4tEmq850Z47rwRW9EzAhFGi6XrgUhUI=-no
