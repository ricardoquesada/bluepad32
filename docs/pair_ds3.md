# How to pair a DualShock 3 (sixaxis) & Motion Controller

The DualShock 3 gamepad does not implement the entire Bluetooth stack. It requires "manual pairing" in order to work.

## Manual pair

### Fetch the ESP32 / Pico W Bluetooth Address

* Connect a terminal to the ESP32 / Pico W using 115200 baud 8N1.

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

In this example, the BT address is "CC:50:E3:AF:E2:96".

### Plug in the DS3 gamepad

Plug in the DS3 gamepad to your PC. Should work on all Linux, Mac and Windows,
although I only tested it on Linux.

### Pair DS3

Install [HIDAPI][hidapi]. For Debian-based OSs, do:

```
$ sudo apt install libhidapi-dev
```

On Gentoo Linux run:

```sh
sudo emerge dev-libs/hidapi
```

On macOS run:

```sh
brew install hidapi
```

on macOS you will also have to modify the Makefile to look like so

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

I got the INCLUDES and LIBS paths using `pkg-config --cflags hidapi` and `pkg-config --libs hidapi` respectively
(note how I went up to the parent directory in the include)

For Windows: I don't know, you are in your own.

* Compile the "sixaxis pairer":

```sh
cd bluepad32/tools
make sixaxispairer
sudo ./sixaxispairer XX:XX:XX:XX:XX:XX  # Following our example, it should be CC:50:E3:AF:E2:96
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
