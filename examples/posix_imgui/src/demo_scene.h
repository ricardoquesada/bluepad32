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

#include <array>
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

    /**
     * @brief Cohesive per-controller-slot UI state (Controls, Rumble, IMU, and Lights tabs).
     *
     * Replaces 23 separate parallel C arrays with C++23 Non-Static Data Member Initializers
     * (NSDMI) and a deterministic `ResetForSlot(int slot)` lifecycle hook invoked whenever a
     * slot transitions from disconnected to connected.
     */
    struct ControllerSlotUiState {
        bool prev_connected = false;                       ///< Previous frame's connection state for this slot.
        std::array<char, 64> last_detected_input{"None"};  ///< Most recently triggered input label.
        uint16_t prev_buttons = 0;                         ///< Previous frame's `gp.buttons` bitmask.
        uint8_t prev_dpad = 0;                             ///< Previous frame's `gp.dpad` bitmask.
        uint8_t prev_misc_buttons = 0;                     ///< Previous frame's `gp.misc_buttons` bitmask.
        bool prev_l2_active = false;                       ///< Previous frame's `LT / L2` active state.
        bool prev_r2_active = false;                       ///< Previous frame's `RT / R2` active state.
        bool prev_left_stick_active = false;               ///< Previous frame's Left Stick deflection state.
        bool prev_right_stick_active = false;              ///< Previous frame's Right Stick deflection state.

        // Rumble tab parameters & Trigger Rumble Mode state
        float rumble_duration_ms = 1000.0f;         ///< Vibration duration in ms (`50..5000`, default `1000`).
        float rumble_weak_intensity = 0.40f;        ///< Right motor (Light/High Freq) intensity (`0.0..1.0`).
        float rumble_strong_intensity = 0.80f;      ///< Left motor (Heavy/Low Freq) intensity (`0.0..1.0`).
        bool trigger_rumble_enabled = false;        ///< True when LT/RT Trigger Rumble Mode is toggled on.
        bool trigger_rumble_active = false;         ///< True while Trigger Rumble Mode is actively vibrating.
        double last_trigger_rumble_time_sec = 0.0;  ///< Timestamp of last Trigger Rumble command dispatch.
        uint8_t last_trigger_strong_u8 = 0;         ///< Last sent Left Motor (strong) amplitude from LT.
        uint8_t last_trigger_weak_u8 = 0;           ///< Last sent Right Motor (weak) amplitude from RT.

        // IMU circular history ring buffers & integrated gyro dial angles
        /// Circular ring buffer of angular velocity samples per axis (`rad/s`).
        std::array<std::array<float, kImuHistoryLen>, kMotionAxisCount> gyro_history{};
        /// Circular ring buffer of linear acceleration samples per axis (`m/s^2`).
        std::array<std::array<float, kImuHistoryLen>, kMotionAxisCount> accel_history{};
        std::array<float, kMotionAxisCount> gyro_angle_deg{};  ///< Integrated gyro angle in `[-180, +180]` deg.
        size_t imu_history_offset = 0;                         ///< Next write index in `[0, kImuHistoryLen - 1]`.
        uint64_t last_imu_timestamp_us = 0;                    ///< Timestamp of the last recorded IMU report.

        // Lights tab parameters
        int player_led_index = 1;                                        ///< Selected player index (`1..4`).
        std::array<bool, 4> player_led_bits{true, false, false, false};  ///< Individual LED checkbox states.
        std::array<float, 3> rgb_color{0.0f, 0.25f, 1.0f};               ///< Normalized RGB float components.
        bool rgb_live_update = false;  ///< If true, streams RGB updates while dragging picker.
        int brightness_percent = 100;  ///< Switch Pro Brightness placeholder slider (`0..100%`).

        /**
         * @brief Resets all per-slot UI state to default values for `slot` (`0..kMaxControllers - 1`).
         *
         * Value-initializes all fields via C++23 NSDMI (`*this = ControllerSlotUiState{}`) and seeds
         * `player_led_index = slot + 1` and `player_led_bits[slot] = true` so a newly connected
         * controller starts with clean UI state matching its assigned seat (`#1..#4`).
         *
         * @param slot Controller slot index in `[0, kMaxControllers - 1]`.
         */
        void ResetForSlot(int slot) noexcept;
    };

    /// Constructs the scene, synchronizing initial filter checkboxes and initializing all 4 slots.
    DemoScene();
    /// Default destructor.
    ~DemoScene();

    /**
     * @brief Executes one UI frame: snapshots controller state from the platform bridge,
     *        updates IMU circular history buffers, and renders the full Dear ImGui window.
     */
    void DoFrame();

    /**
     * @brief Requests a one-shot programmatic selection of category sub-tab `tab_index` (`0..4`)
     *        on the next `DoFrame()` call for headless unit testing.
     *
     * @param tab_index Target category tab index (`0` = Controls, `1` = Rumble, `2` = IMU,
     *                  `3` = Lights, `4` = Info), or `-1` to clear.
     */
    void SelectCategoryTabForTest(int tab_index) noexcept;

    /**
     * @brief Opens (`true`) or closes (`false`) the `Preferences...` panel for headless unit testing.
     *
     * @param active True to render the Preferences view on the next `DoFrame()`; false for controller tabs.
     */
    void SetPreferencesActiveForTest(bool active) noexcept;

    /**
     * @brief Returns a read-only reference to the per-slot UI state for `slot` (clamped to `[0, 3]`).
     *
     * @param slot Controller slot index (`0..kMaxControllers - 1`).
     * @return Const reference to `mSlots[slot]`.
     */
    [[nodiscard]] const ControllerSlotUiState& GetSlotStateForTest(int slot) const noexcept;

    /**
     * @brief Returns a mutable reference to the per-slot UI state for `slot` (clamped to `[0, 3]`)
     *        so headless unit tests can inject non-default UI values before testing disconnect/reconnect reset.
     *
     * @param slot Controller slot index (`0..kMaxControllers - 1`).
     * @return Mutable reference to `mSlots[slot]`.
     */
    [[nodiscard]] ControllerSlotUiState& MutateSlotStateForTest(int slot) noexcept;

   private:
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

    std::array<ControllerSnapshot, kMaxControllers> mSnapshots{};  ///< Contiguous frame-local copy of all 4 slots.
    std::array<ControllerSlotUiState, kMaxControllers> mSlots{};   ///< Per-slot UI state across all 4 slots.
    int mMostRecentConnectedSlot = -1;                             ///< Slot index (`0..3`) pending auto-focus, or `-1`.
    int mCurrentControllerSlot = 0;                                ///< Currently active controller tab index (`0..3`).
    int mActiveControllerPanelTab = 0;                             ///< Currently active category tab index (`0..4`).
    int mRequestedCategoryTabForTest = -1;  ///< One-shot category tab override for headless tests.

    float mFontScale = 1.0f;              ///< Dear ImGui font scale (`style.FontScaleMain`).
    float mRadialDeadzone = 0.10f;        ///< Radial stick deadzone fraction (`0.00..0.35`, default `0.10`).
    bool mDontTrimDeadzone = false;       ///< If true, bypasses stick deadzone trimming.
    bool mPreferencesActive = false;      ///< True while the `Preferences...` view is open.
    bool mVirtualDevicesEnabled = false;  ///< If true, enables virtual child devices (e.g., DS4/DS5 touchpad mouse).
    bool mAutoAcceptGamepads = true;      ///< If true, auto-accepts Bluetooth gamepads and joysticks (default: true).
    bool mAutoAcceptMice = false;         ///< If true, auto-accepts physical Bluetooth mice (default: false).
    bool mAutoAcceptKeyboards = false;    ///< If true, auto-accepts physical Bluetooth keyboards (default: false).
    bool mImuPlotPaused = false;          ///< If true, freezes IMU history ring buffers.
};

/// Top-level convenience alias for per-slot UI state inspection in unit tests.
using ControllerSlotUiState = DemoScene::ControllerSlotUiState;
