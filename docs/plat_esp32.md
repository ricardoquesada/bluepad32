# Bluepad32 for ESP32 using ESP-IDF

## Supported boards

[ESP32 family of SoCs][esp32_socs]. If it supports Bluetooth, it works. Non-exhaustive list:

- ESP32
- ESP32-S3
- ESP32-C3 / ESP32-C6
- ESP32-H2

[esp32_socs]: https://www.espressif.com/en/products/socs

## Installation

### 1. Download ESP-IDF

The official ESP-IDF documentation is here: <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html>

TL;DR:

- [Windows users](https://dl.espressif.com/dl/esp-idf/)
- [Linux / macOS users](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html)


It works with ESP-IDF v4.4 and v5.x.

### 2. Clone Bluepad32 GitHub repo

   ```sh
   git clone --recursive https://github.com/ricardoquesada/bluepad32.git
   ```

### 3. Patch BTstack and integrate it as a local component

Patch it:

   ```sh
   cd ${BLUEPAD32_SRC}/external/btstack
   git apply ../patches/*.patch
   ```

Integrate it:

   ```sh
   cd ${BLUEPAD32}/external/btstack/port/esp32
   # This will install BTstack as a component inside Bluepad32 source code (recommended).
   # Remove "IDF_PATH=../../../../src" if you want it installed in the ESP-IDF folder
   IDF_PATH=../../../../src ./integrate_btstack.py
   ```

### 4. Modify example

Go to example folder:

   ```sh
   # ${BLUEPAD32} represents the folder where Bluepad32 Github repo was cloned
   cd ${BLUEPAD32}/examples/esp32
   ```

Customize `main/my_platform.c` file to your needs:

- [my_platform.c](https://github.com/ricardoquesada/bluepad32/blob/main/examples/esp32/main/my_platform.c)

### 5. Compile example

Set target SoC (default is `esp32`):

   ```sh
   # Possible options: esp32, esp32-s3, esp32-c3, etc.
   idf.py set-target esp32
   ```

And compile it:

   ```sh
   idf.py build
   ```

### 6. Flash it

   ```sh
   idf.py flash
   ```

### 7. Serial output and interactive console

To see the output and interact with the console, do:

   ```sh
   idf.py monitor
   ```

[![asciicast](https://asciinema.org/a/650459.svg)](https://asciinema.org/a/650459)

