# Pair a DualShock 3 (SIXAXIS) and Motion Controller

The DualShock 3 gamepad does not implement standard Bluetooth discovery. You
must pair it over USB before it connects over Bluetooth.

## Manual pairing steps

### 1. Find the ESP32 or Pico W Bluetooth address

* Connect a serial terminal to the ESP32 or Pico W at `115200` baud (`8N1`).
* Read the boot log to find the local Bluetooth address:

```
Bluepad32 (C) 2016-2020 Ricardo Quesada and contributors.
Firmware version: v2.0.0-beta
BTStack: Copyright (C) 2017 BlueKitchen GmbH.
Dual port / 1-button mode enabled
Gap security level: 0
BTstack up and running at CC:50:E3:AF:E2:96     <------ THIS IS THE BLUETOOTH ADDRESS
Btstack ready!
```

In this example, the Bluetooth address is `CC:50:E3:AF:E2:96`.

### 2. Connect the DualShock 3 to your computer

Connect the DualShock 3 gamepad to your computer with a USB cable.

### 3. Pair the DualShock 3

Install [HIDAPI][hidapi] for your operating system.

On Debian or Ubuntu, run:

```sh
sudo apt install libhidapi-dev
```

On Gentoo Linux, run:

```sh
sudo emerge dev-libs/hidapi
```

On macOS, run:

```sh
brew install hidapi
```

On macOS, update `tools/Makefile` to include the Homebrew `hidapi` paths:

```makefile
INCLUDES := -I/opt/homebrew/Cellar/hidapi/0.13.1/include
LIBS := -L/opt/homebrew/Cellar/hidapi/0.13.1/lib -lhidapi

sixaxispairer: sixaxispairer.o
	${CC} $^ $(LIBS) -o $@

sixaxispairer.o: sixaxispairer.c
	${CC} -c $(INCLUDES) $< -o $@

clean:
	rm sixaxispairer sixaxispairer.o
```

Run `pkg-config --cflags hidapi` and `pkg-config --libs hidapi` to find your
local `INCLUDES` and `LIBS` paths.

Compile and run `sixaxispairer`:

```sh
cd bluepad32/tools
make sixaxispairer
sudo ./sixaxispairer XX:XX:XX:XX:XX:XX  # Example: CC:50:E3:AF:E2:96
```

[hidapi]: https://github.com/signal11/hidapi

### 4. Pair a PS Move Motion Controller or Navigation Controller

Use the [PS Move API][psmoveapi] tool to pair a PS Move controller:

```sh
# List connected PS Move devices
sudo psmove list
```

```sh
# Write the target Bluetooth host address to the controller
sudo psmove pair XX:XX:XX:XX:XX:XX  # Example: CC:50:E3:AF:E2:96
```

[psmoveapi]: https://github.com/thp/psmoveapi

### 5. Disconnect the USB cable

Unplug the controller from your computer and press the **PS** button. The
controller connects to the ESP32 or Pico W within 10 seconds.
