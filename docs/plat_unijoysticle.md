# Bluepad32 for Unijoysticle

![Unijoysticle 2](https://lh3.googleusercontent.com/DChZhkyEl-qqZ3r9N7_RhzvF4zDkSdgNyZwczBofnp28D6ncXcbGq3CXBc5SeC5zooUbBCRo87stuAx-4Q7FwItz1NfaZ4_EJjX3pIroiiR-fcXPzZWk0OifvtaoA8iUJsQQnhkC9q4=-no)

## What is Unijoysticle

Unijoysticle is a device that connects to retro computers like that Commodore 64.
Basically you can play Commodore 64 games using modern gamepads.

Detailed information about the project can be found here:

* http://retro.moe/unijoysticle2

## How to compile it & flash it

```sh
# Compile it...

$ cd {BLUEPAD32}/src
$ export PLATFORM=unijoysticle
$ make -j

# ... and flash it!

# Port my vary
$ export ESPPORT=/dev/ttyUSB0
$ make flash
```