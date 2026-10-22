# Bluepad32 for ESP32 with ESP-IDF

## Supported boards

Bluepad32 supports all Bluetooth-enabled SoCs in the [ESP32 family][esp32_socs]:

- ESP32
- ESP32-S3
- ESP32-C3, ESP32-C5, and ESP32-C6
- ESP32-H2

[esp32_socs]: https://www.espressif.com/en/products/socs

## Installation

### 1. Install ESP-IDF

Install ESP-IDF v5.5 or newer. See the official
[ESP-IDF Getting Started guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html):

- [Windows installer](https://dl.espressif.com/dl/esp-idf/)
- [Linux and macOS setup guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html)

### 2. Clone the Bluepad32 repository

```sh
git clone --recursive https://github.com/ricardoquesada/bluepad32.git
```

### 3. Apply the BTstack patches

```sh
cd ${BLUEPAD32_SRC}/external/btstack
git apply ../patches/*.patch
```

### 4. Customize the example

Open the ESP32 example directory:

```sh
# ${BLUEPAD32} is the path to your cloned Bluepad32 repository
cd ${BLUEPAD32}/examples/esp32
```

Edit `main/my_platform.c` to match your application requirements:

- [my_platform.c](https://github.com/ricardoquesada/bluepad32/blob/main/examples/esp32/main/my_platform.c)

### 5. Compile the example

Set the target SoC (default is `esp32`):

```sh
# Options: esp32, esp32-s3, esp32-c3, esp32-c5, esp32-c6, esp32-h2
idf.py set-target esp32
```

Compile the project:

```sh
idf.py build
```

### 6. Flash the binary

```sh
idf.py flash
```

### 7. Open the serial monitor and console

Run `idf.py monitor` to view serial logs and use the interactive console:

```sh
idf.py monitor
```

[![asciicast](https://asciinema.org/a/650459.svg)](https://asciinema.org/a/650459)

