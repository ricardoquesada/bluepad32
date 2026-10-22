# Bluepad32 for Unijoysticle

![Unijoysticle 2](https://lh3.googleusercontent.com/pw/ABLVV859WZpHjZH8U9XC8veB8h42R_UZ7o8VQ4QTochbCWiSX7YutqnLAP-jkjUUIjn-O_DRN1F9SoMF59t-RvPk5dHSGaq-r_ifd-qL0LJsC2tCl6ePQqjYfbTXnGfYm2_FegLKXaoEqdZ1i5thebTTa4X_Hg=-no-gm?authuser=0)

## What is Unijoysticle?

Unijoysticle is a Bluetooth adapter for retro computers such as the Commodore
64, Amiga, and Atari. You can play Commodore 64 games with modern Bluetooth
gamepads.

For project details, see:

* <http://retro.moe/unijoysticle2>

## Compile and flash the firmware

Run these commands to build and flash the Unijoysticle firmware:

```sh
cd ${BLUEPAD32}/src

# Select the Unijoysticle platform:
# Component config -> Bluepad32 -> Target Platform -> Unijoysticle
idf.py menuconfig

# Compile the firmware
idf.py build

# Set the serial port (port name can vary) and flash the firmware
export ESPPORT=/dev/ttyUSB0
idf.py flash
```
