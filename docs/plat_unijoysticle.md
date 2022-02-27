# Bluepad32 for Unijoysticle

![Unijoysticle 2](https://lh3.googleusercontent.com/DChZhkyEl-qqZ3r9N7_RhzvF4zDkSdgNyZwczBofnp28D6ncXcbGq3CXBc5SeC5zooUbBCRo87stuAx-4Q7FwItz1NfaZ4_EJjX3pIroiiR-fcXPzZWk0OifvtaoA8iUJsQQnhkC9q4=-no)

## What is Unijoysticle

Unijoysticle is a device that connects to retro computers like the Commodore 64.
Basically you can play Commodore 64 games using modern gamepads.

Detailed information about the project can be found here:

* http://retro.moe/unijoysticle2

## How to compile it & flash it

```sh
cd {BLUEPAD32}/src

# Select Unijoysticle platform:
# Components config -> Bluepad32 -> Target Platform -> Unijoysticle
idf.py menuconfig

# Compile it
idf.py build

# ... and flash it!

# Port my vary
export ESPPORT=/dev/ttyUSB0
idf.py flash
```
