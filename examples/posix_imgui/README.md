# Bluepad32 POSIX Dear ImGui Controller Tester (`examples/posix_imgui`)

Interactive desktop GUI controller tester for Bluepad32 on Linux and macOS,
combining Bluepad32's POSIX `libusb` Bluetooth stack with **Dear ImGui**
(`external/imgui`, using the GLFW + OpenGL3 backend) and the visual gamepad
layout and sprite assets from the Android Game Development Kit (AGDK)
`game_controller` sample.

Supports up to 4 concurrent Bluetooth gamepads (`Controller #1` ..
`Controller #4`, corresponding to `GAMEPAD_SEAT_A` .. `GAMEPAD_SEAT_D`) with 5
interactive tabs per controller:

1. **Controls:** Real-time 2D button, D-Pad, thumbstick, and analog trigger
   visualization (automatically adapting face buttons between Xbox `A/B/X/Y`,
   PlayStation `Cross/Circle/Square/Triangle`, and Nintendo Switch reversed
   `B/A/Y/X` layouts) plus a post-canvas live numeric telemetry readout (`axis_x`, `axis_y`, `axis_rx`, `axis_ry`,
   `brake`, `throttle`, `dpad`,
   `buttons`, `misc_buttons`, and `Capture`).
2. **Rumble:** Configurable start delay (`0..1000 ms`), duration (`0..2000
    ms`), and weak/strong motor intensities (`0.00..1.00` -> `0..255`) with **Vibrate**, **Stop Rumble**, and one-click
   haptic presets wired to
   `d->report_parser.play_dual_rumble`.
3. **IMU:** Real-time 6-axis motion visualization featuring a 2D circular
   bullseye radar widget for the **Accelerometer** (`m/s²` and raw `accel[0..2]`
   readouts) and three circular compass/needle dials (`X`, `Y`, `Z`) for the
   **Gyroscope** showing integrated rotation angles (`°` with `Reset` button)
   and live angular velocity (`rad/s` and raw `gyro[0..2]`), plus 240-sample
   scrolling history plots (`ImGui::PlotLines`) with pause/resume and clear
   controls.
4. **Lights:** Interactive Player ID LEDs (`1..4` slider, raw 4-bit LED
   checkboxes, and seat presets wired to `d->report_parser.set_player_leds`),
   RGB Lightbar color picker and color swatches (`d->report_parser.set_lightbar_color` for DualShock 4, DualSense, and
   PS
   Move), and a Switch Pro Controller / Joy-Con Right Brightness LED UI
   placeholder.
5. **Info:** Hardware and Bluetooth link diagnostics including Vendor/Product
   IDs, Bluetooth MAC address (`bd_addr_t`), Bluepad32 model name, controller
   subtype, face button layout class, RSSI, battery level progress bar, input
   report interval delta (`ms` / `Hz`), and capability badges.

## Architecture & Threading Model

Because Bluepad32 and BTstack are single-threaded and non-thread-safe while
GLFW + OpenGL3 + Dear ImGui must own the main thread for window event polling
and rendering, `examples/posix_imgui` decouples execution across two threads:

* **Thread 1 (Main / UI Thread):** Runs the 60 Hz GLFW + OpenGL3 + Dear ImGui
  render loop (`main.cpp`, `demo_scene.cpp`, `controllerui_data.cpp`,
  `controllerui_util.cpp`, `texture_asset_loader.cpp`). Reads a lock-protected
  snapshot of all 4 controller slots once per frame via
  `posix_imgui_get_snapshots()` and enqueues output commands (rumble, player
  LEDs, RGB lightbar, shutdown) via non-blocking `posix_imgui_request_*()`
  dispatchers.
* **Thread 2 (BTstack / Bluepad32 Run-Loop Thread):** Executes
  `btstack_run_loop_execute()` over `libusb` file descriptors and timers (`posix_imgui_platform.cpp`). Updates
  `g_snapshots[slot]` under
  `g_state_mutex` inside `uni_platform` callbacks and drains queued UI
  commands under `g_cmd_mutex` when woken via BTstack's POSIX pipe callback
  `btstack_run_loop_execute_on_main_thread()`.

## Prerequisites

* **Linux (Debian/Ubuntu):**

  ```bash
  sudo apt install build-essential cmake pkg-config libusb-1.0-0-dev libpng-dev libgl1-mesa-dev libglfw3-dev
  ```

  *(Note: If `libglfw3-dev` is not installed on the system, CMake will
  automatically fetch and build GLFW 3.4 statically via `FetchContent`,
  provided X11/Wayland development headers are present.)*

* **macOS (Homebrew):**

  ```bash
  brew install cmake pkg-config libusb libpng glfw
  ```

### ImGui

1. Open a terminal and set the working directory to `external/`
2. `git clone https://github.com/ocornut/imgui`

## Building

```bash
mkdir -p build
cmake -S . -B build
cmake --build build -j
```

## Running Unit Tests

A headless CTest unit test suite (`test_posix_imgui`) verifies platform slot
management, pre-ready disconnect guards, 5th-controller rejection,
layout/capability classification, command queue draining, CPU-side `libpng`
decoding of all 44 PNG sprites, and Switch reversed button/mask mapping without
requiring a USB Bluetooth dongle or X11/Wayland display:

```bash
ctest --test-dir build --output-on-failure -V
```

## Running the Application

Plug in a supported USB Bluetooth controller dongle and run:

```bash
sudo ./build/bluepad32_posix_imgui_example_app
```

### Command-Line Options

Pass `--help` (or `-h`) to inspect command-line options:

| Option              | Argument  | Description                                                                           |
|---------------------|-----------|---------------------------------------------------------------------------------------|
| `-h`, `--help`      | *(none)*  | Print usage help and exit with code `0` (`EXIT_SUCCESS`)                              |
| `-u`, `--usbpath`   | `USBPATH` | Colon- or hyphen-separated hex USB port path to the Bluetooth dongle (e.g., `1-2\:3`) |
| `-l`, `--logfile`   | `LOGFILE` | Path to store PacketLogger (`.pklg`) HCI trace (default `/tmp/hci_dump.pklg`)         |
| `-r`, `--reset-tlv` | *(none)*  | Reset bonding information stored in `/tmp/btstack_<bd_addr>.tlv`                      |
| `-b`, `--ble`       | `0\|1`    | Disable (`0`) or enable (`1`, default) Bluetooth Low Energy (BLE) scanning            |
| `-d`, `--delete`    | *(none)*  | Delete stored Bluetooth bonding keys on startup                                       |
| `-e`, `--enhanced`  | *(none)*  | Enable Bluepad32 enhanced  controller mode                                            |
