# Programmer's Guide: Arduino API

The Bluepad32 Arduino API (`<Bluepad32.h>`) provides an event-driven C++
interface for Bluetooth gamepads, mice, keyboards, and Wii Balance Boards on:

- **[ESP32, ESP32-S3, and ESP32-C3 boards](../plat_arduino/)** (via the
    *ESP32 + Bluepad32* Arduino board package or the ESP-IDF + Arduino
    template)

---

## 1. Execution Model and Lifecycle (`BP32`)

On ESP32, the Bluetooth stack (BTstack and Bluepad32 core) runs on **Core 0**,
and Arduino `setup()` and `loop()` run on **Core 1**. The global `BP32`
singleton synchronizes state across both cores:

1.  **`BP32.setup(&onConnectedController, &onDisconnectedController)`**:
    Registers connection and disconnection callbacks and initializes the
    Bluetooth host controller. Call this method once inside `setup()`.
2.  **`BP32.update()`**:
    Synchronizes controller state from the Bluepad32 core and runs connection
    or disconnection callbacks. Call this method once per frame in `loop()`. It
    returns `true` when new controller data arrives.

```mermaid
sequenceDiagram
    autonumber
    participant Core0 as Core 0 (BTstack / Bluepad32 Task)
    participant Shared as BP32 Shared State
    participant Core1 as Core 1 (Arduino setup() / loop())

    Core1->>Shared: BP32.setup(&onConnect, &onDisconnect)
    Core0->>Shared: HID Input Report Parsed -> Normalize Controller
    Core1->>Shared: BP32.update()
    Shared-->>Core1: Invoke onConnectedController(ControllerPtr)
    Shared-->>Core1: Return true (Controller->hasData() == true)
    Core1->>Shared: ctl->axisX(), ctl->buttons(), ctl->playDualRumble(...)
```

---

## 2. Complete Working Example

```cpp
#include <Bluepad32.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// Runs when a new Bluetooth controller connects
void onConnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            Serial.printf("CALLBACK: Controller connected, index=%d, model=%s\n",
                          i, ctl->getModelName().c_str());
            ControllerProperties properties = ctl->getProperties();
            Serial.printf("VID=0x%04x, PID=0x%04x, BTAddr=%02x:%02x:%02x:%02x:%02x:%02x\n",
                          properties.vendor_id, properties.product_id,
                          properties.btaddr[0], properties.btaddr[1], properties.btaddr[2],
                          properties.btaddr[3], properties.btaddr[4], properties.btaddr[5]);
            myControllers[i] = ctl;
            // Set player indicator LED to match slot index (1-based bitmask)
            ctl->setPlayerLEDs(1 << i);
            return;
        }
    }
    Serial.println("CALLBACK: Controller connected, but no empty slot available");
}

// Runs when a controller disconnects
void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            Serial.printf("CALLBACK: Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            return;
        }
    }
}

void processGamepad(ControllerPtr ctl) {
    // Analog sticks: normalized to [-511, 512]
    // Pedals (L2/brake, R2/throttle): normalized to [0, 1023]
    Serial.printf(
        "idx=%d, dpad=0x%02x, buttons=0x%04x, misc=0x%02x, "
        "LX=%4d, LY=%4d, RX=%4d, RY=%4d, brake=%4d, throttle=%4d\n",
        ctl->index(), ctl->dpad(), ctl->buttons(), ctl->miscButtons(),
        ctl->axisX(), ctl->axisY(), ctl->axisRX(), ctl->axisRY(),
        ctl->brake(), ctl->throttle());

    // Play a 250 ms rumble burst when the user presses Cross / A
    if (ctl->a()) {
        ctl->playDualRumble(0 /* delayedStartMs */, 250 /* durationMs */,
                            0x80 /* weakMagnitude */, 0x40 /* strongMagnitude */);
    }

    // Cycle the RGB lightbar when the user presses Square / X
    if (ctl->x()) {
        static uint8_t r = 255, g = 0, b = 128;
        ctl->setColorLED(r, g, b);
        r += 32;
        g += 64;
        b += 16;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.printf("Bluepad32 Firmware: %s\n", BP32.firmwareVersion());

    // Initialize Bluepad32 and register connection lifecycle callbacks
    BP32.setup(&onConnectedController, &onDisconnectedController);

    // Optional: Enable virtual devices so DS4/DualSense touchpads appear as a separate Mouse
    BP32.enableVirtualDevice(false);
}

void loop() {
    // Poll Bluepad32; returns true if at least one controller updated
    bool dataUpdated = BP32.update();
    if (dataUpdated) {
        for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
            ControllerPtr ctl = myControllers[i];
            if (ctl && ctl->isConnected() && ctl->hasData()) {
                if (ctl->isGamepad()) {
                    processGamepad(ctl);
                }
            }
        }
    }
    delay(15);
}
```

---

## 3. Read Controller Inputs (`ControllerPtr`)

### 3.1 Device Classification and Metadata

| Method | Return Type | Description |
| :--- | :--- | :--- |
| `ctl->isConnected()` | `bool` | Returns `true` if the Bluetooth link is active and ready. |
| `ctl->hasData()` | `bool` | Returns `true` if new input reports arrived during the last `BP32.update()`. |
| `ctl->index()` | `int` | Returns the slot index (`0` to `BP32_MAX_GAMEPADS - 1`). |
| `ctl->isGamepad()` | `bool` | Returns `true` if the device is a Gamepad (`UNI_CONTROLLER_CLASS_GAMEPAD`). |
| `ctl->isMouse()` | `bool` | Returns `true` if the device is a Mouse (or virtual touchpad mouse). |
| `ctl->isKeyboard()` | `bool` | Returns `true` if the device is a Keyboard (`UNI_CONTROLLER_CLASS_KEYBOARD`). |
| `ctl->isBalanceBoard()` | `bool` | Returns `true` if the device is a Nintendo Wii Balance Board. |
| `ctl->getModelName()` | `String` | Returns the controller model name (such as `"DualSense"` or `"Switch Pro"`). |
| `ctl->getProperties()` | `ControllerProperties` | Returns a struct with `vendor_id`, `product_id`, `btaddr[6]`, `type`, `subtype`, and capability `flags`. |
| `ctl->battery()` | `uint8_t` | Returns the battery level (`0`: unavailable, `1`: empty, `255`: full). |

### 3.2 Gamepad Axes, Pedals, Buttons, and IMU

Bluepad32 normalizes all gamepads to a standard positional layout:

| Category | Method(s) | Range / Bitmask Constants |
| :--- | :--- | :--- |
| **Left Thumbstick** | `ctl->axisX()`, `ctl->axisY()` | `int32_t` in **`[-511, 512]`** (Right is `+X`, Down is `+Y`). |
| **Right Thumbstick** | `ctl->axisRX()`, `ctl->axisRY()` | `int32_t` in **`[-511, 512]`** (Right is `+RX`, Down is `+RY`). |
| **Analog Triggers** | `ctl->brake()` (L2), `ctl->throttle()` (R2) | `int32_t` in **`[0, 1023]`** (`0` = released, `1020`/`1023` = full press). |
| **D-Pad** | `ctl->dpad()` | Bitmask of `DPAD_UP` (`0x01`), `DPAD_DOWN` (`0x02`), `DPAD_RIGHT` (`0x04`), `DPAD_LEFT` (`0x08`). |
| **Face Buttons** | `ctl->a()`, `ctl->b()`, `ctl->x()`, `ctl->y()`, `ctl->buttons()` | Positional helpers (`a()`=Bottom/Cross, `b()`=Right/Circle, `x()`=Left/Square, `y()`=Top/Triangle) or `BUTTON_A`..`BUTTON_THUMB_R` bitmask. |
| **Shoulder & Stick Clicks** | `ctl->l1()`, `ctl->r1()`, `ctl->l2()`, `ctl->r2()`, `ctl->thumbL()`, `ctl->thumbR()` | `bool` digital state for bumpers, digital triggers, and L3/R3 clicks. |
| **Misc / System Buttons** | `ctl->miscSystem()`, `ctl->miscSelect()`, `ctl->miscStart()`, `ctl->miscCapture()`, `ctl->miscButtons()` | `MISC_BUTTON_SYSTEM` (`0x01`), `MISC_BUTTON_SELECT` (`0x02`), `MISC_BUTTON_START` (`0x04`), `MISC_BUTTON_CAPTURE` (`0x08`). |
| **Gyroscope & Accelerometer** | `ctl->gyroX()`, `ctl->gyroY()`, `ctl->gyroZ()`, `ctl->accelX()`, `ctl->accelY()`, `ctl->accelZ()` | 3-axis angular velocity in **`rad/s`** (`X`=pitch, `Y`=yaw, `Z`=roll) and linear acceleration in **`m/s²`** (`X`=right, `Y`=up `+9.81 m/s²` at rest, `Z`=back) in the right-handed Y-up frame. |

### 3.3 Mouse, Keyboard, and Wii Balance Board

- **Mouse (`ctl->isMouse()`)**:
    - `ctl->deltaX()`, `ctl->deltaY()`: Returns relative cursor movement since
        the last report (`int32_t`).
    - `ctl->scrollWheel()`: Returns the vertical scroll wheel step (`int8_t`).
    - `ctl->buttons()`: Returns the mouse button bitmask (`BUTTON_A` = Left
        click, `BUTTON_B` = Right click, `BUTTON_X` = Middle click).
- **Keyboard (`ctl->isKeyboard()`)**:
    - `ctl->isKeyPressed(KeyboardKey key)`: Returns `true` if the user holds a
        specific USB HID keycode (such as `Keyboard_A`, `Keyboard_Spacebar`, or
        `Keyboard_LeftShift`).
- **Wii Balance Board (`ctl->isBalanceBoard()`)**:
    - `ctl->topRight()`, `ctl->bottomRight()`, `ctl->topLeft()`,
        `ctl->bottomLeft()`: Returns calibrated load sensor values (`uint16_t`).
    - `ctl->temperature()`: Returns the board temperature (`int`).

---

## 4. Haptics, LEDs, and Output Reports

- **Dual-Motor Rumble**:

    ```cpp
    ctl->playDualRumble(
        0,      // delayedStartMs: 0 starts immediately
        250,    // durationMs: duration in milliseconds (0 stops rumble)
        0x80,   // weakMagnitude: 0x00..0xFF (high-frequency motor)
        0xc0    // strongMagnitude: 0x00..0xFF (low-frequency motor)
    );
    ```

- **Player Indicator LEDs (Switch Pro, PS3, Wii, Xbox)**:

    ```cpp
    ctl->setPlayerLEDs(0x01); // 4-bit mask (bits 0..3 correspond to LEDs 1..4)
    ```

- **RGB Lightbar (DualShock 4, DualSense, PS Move)**:

    ```cpp
    ctl->setColorLED(0, 128, 255); // Red, Green, Blue (0..255)
    ```

- **Sony DualSense Adaptive Triggers (ESP-IDF + Arduino Template)**:
    Include `uni_hid_parser_ds5.h` to apply adaptive trigger effects
    (`feedback`, `weapon`, `vibration`, `off`) to the left (`L2`) and right
    (`R2`) triggers:

    ```cpp
    #include "parser/uni_hid_parser_ds5.h"

    // Continuous resistance starting at position 2 with strength 6 (range 0..8; 0 turns effect off)
    ds5_adaptive_trigger_effect_t fx = ds5_new_adaptive_trigger_effect_feedback(2, 6);
    // Weapon trigger snap between position 2 and 6 with strength 8
    ds5_adaptive_trigger_effect_t weapon = ds5_new_adaptive_trigger_effect_weapon(2, 6, 8);
    // High-frequency trigger vibration at position 3, amplitude 5 (0..8), frequency 40 Hz
    ds5_adaptive_trigger_effect_t vib = ds5_new_adaptive_trigger_effect_vibration(3, 5, 40);
    ```

---

## 5. Bluetooth Scanning, Bonding, and Allowlist Control

| `BP32` / `Controller` / `uni_bt_allowlist` Method | Description |
| :--- | :--- |
| `BP32.enableNewConnections(bool enabled)` | Starts (`true`) or stops (`false`) Bluetooth scans for new controllers. Bonded controllers can still reconnect when `false`. |
| `BP32.forgetBluetoothKeys()` | Deletes all saved Classic BT and BLE link keys from NVS. Call this method if you pair a controller with another host and want to re-pair it. |
| `BP32.enableVirtualDevice(bool enabled)` | Enables or disables a second `ControllerPtr` (Mouse) for composite gamepads with touchpads (DualShock 4 and DualSense). Call this method before you connect the controller. |
| `BP32.enableBLEService(bool enabled)` | Enables or disables the Bluepad32 BLE GATT configuration and telemetry service. |
| `ctl->disconnect()` | Disconnects an active controller slot. |
| `uni_bt_allowlist_add_addr(bd_addr_t addr)` / `uni_bt_allowlist_set_enabled(bool enabled)` | Restricts Bluetooth connections to an NVS-backed list of 6-byte MAC addresses (`#include <bt/uni_bt_allowlist.h>`). |


