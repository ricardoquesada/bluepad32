# Bluepad32 Posix ImGui Controller Tester

Desktop GUI controller tester for Bluepad32 on Linux and macOS, built with
[Dear ImGui](https://github.com/ocornut/imgui), GLFW, and OpenGL3.

<!-- TODO: Add screenshot here -->
<!-- ![Screenshot](screenshot.png) -->

## Features

Supports up to 4 concurrent Bluetooth gamepads with 5 tabs per controller:

* **Controls:** Live visualization of sticks, triggers, buttons, D-pad, and
  radial deadzone.
* **Rumble:** Dual-motor force feedback and trigger rumble testing.
* **IMU:** Real-time 3-axis accelerometer (`m/s²`) and gyroscope (`rad/s`)
  dials and history plots.
* **Lights:** Player LEDs and RGB lightbar controls.
* **Info:** Device VID/PID, Bluetooth address, battery level, RSSI, and report
  rate (`Hz`).

## Prerequisites

* **Linux (Debian/Ubuntu):**

  ```sh
  sudo apt install build-essential cmake pkg-config libusb-1.0-0-dev libgl1-mesa-dev libglfw3-dev
  ```

* **macOS (Homebrew):**

  ```sh
  brew install cmake pkg-config libusb glfw
  ```

* **Dear ImGui:**

  Clone Dear ImGui into the repository's `external/imgui` folder:

  ```sh
  git clone --depth 1 https://github.com/ocornut/imgui.git ../../external/imgui
  ```

## Building and Running

```sh
mkdir build && cd build
cmake ..
make -j
sudo ./bluepad32_posix_imgui_example_app
```

### Command-Line Options

| Option              | Argument  | Description                                                                    |
|---------------------|-----------|--------------------------------------------------------------------------------|
| `-h`, `--help`      | *(none)*  | Print usage help and exit                                                      |
| `-u`, `--usbpath`   | `USBPATH` | USB port path to the Bluetooth dongle (e.g., `1-2:3`)                          |
| `-l`, `--logfile`   | `LOGFILE` | Path to store PacketLogger (`.pklg`) HCI trace (default `/tmp/hci_dump.pklg`)  |
| `-r`, `--reset-tlv` | *(none)*  | Reset bonding information stored in `/tmp/btstack_<bd_addr>.tlv`               |
| `-b`, `--ble`       | `0\|1`    | Disable (`0`) or enable (`1`, default) Bluetooth Low Energy (BLE) scanning     |
| `-d`, `--delete`    | *(none)*  | Delete stored Bluetooth bonding keys on startup                                |
| `-e`, `--enhanced`  | *(none)*  | Enable Bluepad32 enhanced controller mode                                      |
