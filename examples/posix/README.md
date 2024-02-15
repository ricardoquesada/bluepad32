## Bluepad32 for Linux, macOS and other POSIX systems

Useful while adding new gamepads and/or testing new features that don't require a microcontroller.

Make sure that libusb-1.0-dev is installed and do:

```
$ mkdir build
$ cd build
$ cmake ..
$ make -j
```

To run it do:
```
$ cd build
$ sudo ./bluepad32_posix_example_app
```
