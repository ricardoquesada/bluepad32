# Tools

## bluepad32 for Linux

Useful while adding new gamepads and/or testing new features that don't neceseraly
require an ESP32.

Make sure that libusb-1.0-dev is installed and do:

```
$ make bluepad32

# And run it with sudo:
$ sudo ./bluepad32
```

## sixaxispairer

More info in [docs][docs_pairer] folder, but basically it is a tool to let you pair your
DS3 gamepad with ESP32.

```
$ make sixaxispairer
$ ./sixaxispairer XX:XX:XX:XX:XX:XX
```

[docs_pairer]: https://gitlab.com/ricardoquesada/bluepad32/-/blob/master/docs/pair_ds3.md

## Paint for CircuitPython

* Copy `circuitpython_paint.py` to the CIRCUITPY folder, rename it to `code.py`
* Open `circuitpython_paint.py` to check the needed dependencies
