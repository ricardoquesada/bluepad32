# How to pair a DUALSHOCK3 (sixaxis) & Motion Controller with the ESP32

The DUALSHOCK3 gamepad does not implement the entire Bluetooth stack. It requires "manual pairing" in order to work.

## Only vlaid for Bluepad32 v3.5 or newer

DS3 support is enabled by default. It is no longer needed to patch Bluepad32.

Also, since v3.5, it is possible to change the `GAP Security` from the console by using the
`set_gap_security_level` command. But again, this is no longer needed in versions >= 3.5.

## Only valid for Bluepad32 < 3.5

By default, DS3 is disabled since it requires `gap_set_security_level(0)`.
But that change breaks Nintendo Switch and other gamepads, so it is disabled.
To enable it, you should change on setting in `menuconfig`:

```sh
idf.py menuconfig
```

And select `Component config` -> `Bluepad32` -> `Enable GAP Security` (must be disabled)

And then you have to re-compile the firmware (see here for [firmware compilation instructions]).

[firmware compilation instructions]: firmware_setup.md

## Manual pair

### Fetch the ESP32 Bluetooth Address

* Connect a terminal to the ESP32 using 115200 baud 8N1.

* Watch the logs. You should see something like:

```
Bluepad32 (C) 2016-2020 Ricardo Quesada and contributors.
Firmware version: v2.0.0-beta
BTStack: Copyright (C) 2017 BlueKitchen GmbH.
Dual port / 1-button mode enabled
Gap security level: 0
BTstack up and running at CC:50:E3:AF:E2:96     <------ THIS IS THE BLUETOOTH ADDRESS
Btstack ready!
```

In this example, the ESP32 Address is "CC:50:E3:AF:E2:96".

### Plug in the DS3 gamepad

Plug in the DS3 gamepad to your PC. Should work on all Linux, Mac and Windows,
although I only tested it on Linux.

### Pair DS3

Install [HIDAPI][hidapi]. For Debian-based OSs, do:
```
$ sudo apt install libhidapi-dev
```

On Gentoo Linux run:
```
$ sudo emerge dev-libs/hidapi
```

On MacOS run:
```
$ brew install hidapi
```

on Mac you will also have to modify the Makefile to look like so
```
INCLUDES := -I/opt/homebrew/Cellar/hidapi/0.13.1/include
LIBS := -L/opt/homebrew/Cellar/hidapi/0.13.1/lib -lhidapi

sixaxispairer: sixaxispairer.o
	${CC} $^ $(LIBS) -o $@

sixaxispairer.o: sixaxispairer.c
	${CC} -c $(INCLUDES) $< -o $@

clean:
	rm sixaxispairer sixaxispairer.o
```
I got the INCLUDES and LIBS paths using `pkg-config --cflags hidapi` and `pkg-config --libs hidapi` respectivly (note how I went up to the parent directory in the include)

For  Windows: I don't know, you are in your own.

* Compile the "sixaxis pairer":

```sh
$ cd bluepad32/tools
$ make sixaxispairer
$ sudo ./sixaxispairer XX:XX:XX:XX:XX:XX  # Following our example, it should be CC:50:E3:AF:E2:96
```

[hidapi]: https://github.com/signal11/hidapi

### Pair Motion Controller / Navigator

Use [PS Move API][psmoveapi].
E.g.:

```sh
# To list the connected devices to your Linux machine
$ sudo psmove list
```

```sh
# To list the connected devices to your Linux machine
$ sudo psmove pair XX:XX:XX:XX:XX:XX  # Following our example, it should be CC:50:E3:AF:E2:96
```

[psmoveapi]: https://github.com/thp/psmoveapi

### Unplug DS3 from computer

Unplug the DS3 gamepad from your computer and press the "Play" button on the DS3
to establish a connection with the ESP32. It might take a few seconds
(perhaps more than 10 seconds) to establish the connection.
