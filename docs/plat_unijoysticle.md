# Bluepad32 for Unijoysticle

![Unijoysticle 2](https://lh3.googleusercontent.com/pw/ABLVV859WZpHjZH8U9XC8veB8h42R_UZ7o8VQ4QTochbCWiSX7YutqnLAP-jkjUUIjn-O_DRN1F9SoMF59t-RvPk5dHSGaq-r_ifd-qL0LJsC2tCl6ePQqjYfbTXnGfYm2_FegLKXaoEqdZ1i5thebTTa4X_Hg=-no-gm?authuser=0)

## What is Unijoysticle

Unijoysticle is a device that connects to retro computers like the Commodore 64, Amiga or Atari.
Basically you can play Commodore 64 games using modern gamepads.

Detailed information about the project can be found here:

* <http://retro.moe/unijoysticle2>

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
