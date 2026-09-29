// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 The Android Open Source Project
// Copyright 2026 Ricardo Quesada
// http://retro.moe/unijoysticle2

/**
 * @file demo_scene.h
 * @brief Dear ImGui 4-controller, 5-tab interactive controller tester scene (`DemoScene`).
 *
 * Architectural Role & Threading Context:
 *   `DemoScene` is instantiated and executed exclusively on Thread 1 (the main GLFW +
 *   OpenGL3 + Dear ImGui thread). It never touches `uni_hid_device_t*` pointers or
 *   BTstack APIs directly:
 *     - At the start of each 60 Hz frame, `DoFrame()` calls `posix_imgui_get_snapshots()`
 *       to copy the latest lock-protected `ControllerSnapshot` array from Thread 2.
 *     - When the user interacts with output widgets on the **Rumble** or **Lights** tabs,
 *       `DemoScene` calls the non-blocking `posix_imgui_request_*()` dispatchers, which
 *       enqueue commands and wake the BTstack run loop via POSIX pipe.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "imgui.h"
#include "posix_imgui_platform.h"

/**
 * @brief Manages UI state, IMU history buffers, and Dear ImGui rendering across 4 controller
 *        slots (`#1`..`#4`) and 5 category tabs (`Controls`, `Rumble`, `IMU`, `Lights`, `Info`).
 */
class DemoScene {
   public:
    /// Number of orthogonal motion axes per sensor (X = 0, Y = 1, Z = 2).
    static constexpr size_t kMotionAxisCount = 3;
    /// Number of historical IMU samples stored per axis in the circular plot ring buffer.
    static constexpr size_t kImuHistoryLen = 240;

    DemoScene();
    ~DemoScene();

    /**
     * @brief Executes one UI frame: snapshots controller state from the platform bridge,
     *        updates IMU circular history buffers, and renders the full Dear ImGui window.
     */
    void DoFrame();

   private:
    /// Configures the root full-viewport Dear ImGui window spanning `io.DisplaySize`.
    void SetupUIWindow();
    /// Renders the `Preferences...` button or modal panel (Font scale, Raw deadzone).
    /// Returns true if the Preferences panel is currently open (suppressing the main tab bar).
    bool RenderPreferences();
    /// Renders the top summary bar (connected count, deadzone mode, and FPS).
    void RenderStatusBar();
    /// Renders the 4 top-level controller tabs (`Controller #1` .. `Controller #4`), auto-focusing
    /// newly connected controllers via `ImGuiTabItemFlags_SetSelected`.
    void RenderControllerTabs();
    /// Renders the header summary and 5 category tabs for a connected controller in `slot`.
    void RenderPanel(int slot, const ControllerSnapshot& snap);

    /// Tab 1 — `Controls`: Multi-card `ImDrawList` vector dashboard for buttons, sticks, and triggers.
    void RenderPanel_ControlsTab(int slot, const ControllerSnapshot& snap);
    /// Tab 2 — `Rumble`: Dual-motor force-feedback parameters, Vibrate/Stop buttons, and presets.
    void RenderPanel_VibrationTab(int slot, const ControllerSnapshot& snap);
    /// Tab 3 — `IMU`: Circular Accelerometer bullseye & Gyroscope needle dials + 240-sample plots.
    void RenderPanel_MotionTab(int slot, const ControllerSnapshot& snap);
    /// Tab 4 — `Lights`: Player ID LEDs, RGB Lightbar color picker/swatches, and Brightness LED placeholder.
    void RenderPanel_LightsTab(int slot, const ControllerSnapshot& snap);
    /// Tab 5 — `Info`: Hardware/Bluetooth diagnostics, battery progress bar, and capability badges.
    void RenderPanel_InfoTab(int slot, const ControllerSnapshot& snap);

    /// Appends a new IMU sample for `slot` when `snap.last_report_timestamp_us` advances.
    void UpdateImuHistory(int slot, const ControllerSnapshot& snap);
    /// Zeroes the circular IMU history buffers for `slot` (on disconnect or user clear).
    void ClearImuHistory(int slot);
    /// Tracks newly activated buttons, D-Pad directions, triggers, and sticks for the "Last Detected Input" card.
    void UpdateLastDetectedInput(int slot, const ControllerSnapshot& snap);
    /// Dynamically pulses dual-motor rumble proportional to LT/RT pressure when Trigger Rumble Mode is active.
    void UpdateTriggerRumble(int slot, const ControllerSnapshot& snap);

    ControllerSnapshot mSnapshots[kMaxControllers];  ///< Frame-local copy of all 4 controller slots.
    bool mPrevConnected[kMaxControllers];            ///< Previous frame's connection state per slot.
    int mMostRecentConnectedSlot;                    ///< Slot index (`0..3`) pending auto-focus, or `-1`.
    int mCurrentControllerSlot;                      ///< Currently active controller tab index (`0..3`).
    int mActiveControllerPanelTab;                   ///< Currently active category tab index (`0..4`).

    float mFontScale;             ///< Dear ImGui font scale (`style.FontScaleMain`).
    float mRadialDeadzone;        ///< Radial stick deadzone fraction (`0.00..0.35`, default `0.10`).
    bool mDontTrimDeadzone;       ///< If true, bypasses stick deadzone trimming.
    bool mPreferencesActive;      ///< True while the `Preferences...` view is open.
    bool mVirtualDevicesEnabled;  ///< If true, enables virtual child devices (e.g., DS4/DS5 touchpad mouse).
    bool mAutoAcceptGamepads;     ///< If true, auto-accepts Bluetooth gamepads and joysticks (default: true).
    bool mAutoAcceptMice;         ///< If true, auto-accepts physical Bluetooth mice (default: false).
    bool mAutoAcceptKeyboards;    ///< If true, auto-accepts physical Bluetooth keyboards (default: false).

    // Per-controller Controls tab "Last Detected Input" tracking
    char mLastDetectedInput[kMaxControllers][64];  ///< Most recently triggered input label per slot.
    uint16_t mPrevButtons[kMaxControllers];        ///< Previous frame's `gp.buttons` bitmask.
    uint8_t mPrevDpad[kMaxControllers];            ///< Previous frame's `gp.dpad` bitmask.
    uint8_t mPrevMiscButtons[kMaxControllers];     ///< Previous frame's `gp.misc_buttons` bitmask.
    bool mPrevL2Active[kMaxControllers];           ///< Previous frame's `LT / L2` active state.
    bool mPrevR2Active[kMaxControllers];           ///< Previous frame's `RT / R2` active state.
    bool mPrevLeftStickActive[kMaxControllers];    ///< Previous frame's Left Stick deflection state.
    bool mPrevRightStickActive[kMaxControllers];   ///< Previous frame's Right Stick deflection state.

    // Per-controller Rumble tab parameters & Trigger Rumble Mode state
    float mRumbleDurationMs[kMaxControllers];           ///< Vibration duration in ms (`50..5000`, default `1000`).
    float mRumbleWeakIntensity[kMaxControllers];        ///< Right motor (Light/High Freq) intensity (`0.0..1.0`).
    float mRumbleStrongIntensity[kMaxControllers];      ///< Left motor (Heavy/Low Freq) intensity (`0.0..1.0`).
    bool mTriggerRumbleEnabled[kMaxControllers];        ///< True when LT/RT Trigger Rumble Mode is toggled on.
    bool mTriggerRumbleActive[kMaxControllers];         ///< True while Trigger Rumble Mode is actively vibrating.
    double mLastTriggerRumbleTimeSec[kMaxControllers];  ///< Timestamp of last Trigger Rumble command dispatch.
    uint8_t mLastTriggerStrongU8[kMaxControllers];      ///< Last sent Left Motor (strong) amplitude from LT.
    uint8_t mLastTriggerWeakU8[kMaxControllers];        ///< Last sent Right Motor (weak) amplitude from RT.

    // Per-controller IMU circular history ring buffers & integrated gyro dial angles
    float mGyroHistory[kMaxControllers][kMotionAxisCount][kImuHistoryLen];
    float mAccelHistory[kMaxControllers][kMotionAxisCount][kImuHistoryLen];
    float mGyroAngleDeg[kMaxControllers][kMotionAxisCount];  ///< Integrated gyro angle in `[-180, +180]` deg.
    size_t mImuHistoryOffset[kMaxControllers];               ///< Next write index in `[0, kImuHistoryLen - 1]`.
    uint64_t mLastImuTimestampUs[kMaxControllers];           ///< Timestamp of the last recorded IMU report.
    bool mImuPlotPaused;                                     ///< If true, freezes IMU history ring buffers.

    // Per-controller Lights tab parameters
    int mPlayerLedIndex[kMaxControllers];     ///< Selected player index (`1..4`).
    bool mPlayerLedBits[kMaxControllers][4];  ///< Individual LED checkbox states (`LED 1`..`LED 4`).
    float mRgbColor[kMaxControllers][3];      ///< Normalized RGB float components (`0.0..1.0`).
    bool mRgbLiveUpdate[kMaxControllers];     ///< If true, streams RGB updates while dragging picker.
    int mBrightnessPercent[kMaxControllers];  ///< Switch Pro Brightness placeholder slider (`0..100%`).
};
