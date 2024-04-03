# Bluepad32 for ESP32

Supports the different ESP32 chips:

* ESP32
* ESP32-S3
* ESP32-C3 / ESP32-C6
* ESP32-H2

## Install dependencies

1. Install ESP-IDF

    Install the ESP32 toolchain. Use version **4.4** or **5.1**. Might work on newer / older
    ones, but not tested.

    * <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/>

2. Integrate BTstack into ESP32

   ```sh
   cd ${BLUEPAD32}/external/btstack/port/esp32
   # This will install BTstack as a component inside Bluepad32 source code (recommended).
   # Remove "IDF_PATH=../../../../src" if you want it installed in the ESP-IDF folder
   IDF_PATH=../../../../src ./integrate_btstack.py
   ```

3. Compile Bluepad32

    ```sh
    # Possible options: esp32, esp32-s3 or esp32-c3
    idf.py set-target esp32
    ```

    And compile it:

    ```sh
    idf.py build
    ```

4. Flash it

    ```sh
    idf.py flash monitor
    ```

## License

- Example code: licensed under Public Domain.
- Bluepad32: licensed under Apache 2.
- BTstack:
  - Free to use for open source projects.
  - Paid for commercial projects.
  - <https://github.com/bluekitchen/btstack/blob/master/LICENSE>