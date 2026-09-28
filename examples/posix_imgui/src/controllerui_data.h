/*
 * Copyright 2021 The Android Open Source Project
 * Copyright 2026 Ricardo Quesada
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file controllerui_data.h
 * @brief Sprite metadata, 2D canvas coordinate tables, and Bluepad32 input normalization.
 *
 * Architectural Role & Threading Context:
 *   Ported from the Android Game Development Kit (AGDK) `game_controller` sample and
 *   adapted for Bluepad32's `uni_gamepad_t` input model.
 *
 *   - Defines the 19 logical UI button sprites (`ControllerUIButtons`), thumbstick visual
 *     states (`ControllerUIStickStates`), and unified 32-bit UI button bitmask (`UIButtonMask`).
 *   - Provides pure, stateless input mapping and normalization functions
 *     (`MapGamepadToUIButtonMask`, `NormalizeStickAxis`, `NormalizeTriggerAxis`) and
 *     layout reconfiguration (`ControllerUIData::ConfigureButtonLayout`) that can be
 *     exercised both by the 60 Hz Dear ImGui UI thread (`Thread 1`) and by headless
 *     unit tests (`test_posix_imgui`).
 *   - Manages the OpenGL texture handles for all 44 PNG sprites (`LoadControllerUIData` /
 *     `UnloadControllerUIData`) on `Thread 1`.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "imgui.h"
#include "posix_imgui_platform.h"
#include "texture_asset_loader.h"

/// Logical button sprite indices rendered on the 2D controller canvas.
enum ControllerUIButtons {
    UIBUTTON_A = 0,
    UIBUTTON_B,
    UIBUTTON_X,
    UIBUTTON_Y,
    UIBUTTON_CROSS,
    UIBUTTON_CIRCLE,
    UIBUTTON_SQUARE,
    UIBUTTON_TRIANGLE,
    UIBUTTON_L1,
    UIBUTTON_L2,
    UIBUTTON_R1,
    UIBUTTON_R2,
    UIBUTTON_MODE,
    UIBUTTON_SELECT,
    UIBUTTON_START,
    UIBUTTON_DPAD_UP,
    UIBUTTON_DPAD_LEFT,
    UIBUTTON_DPAD_DOWN,
    UIBUTTON_DPAD_RIGHT,
    UIBUTTON_COUNT
};

/// Visual illumination states for a button sprite (`Idle` vs. `Active`).
enum ControllerUIButtonStates { UIBUTTON_STATE_IDLE = 0, UIBUTTON_STATE_ACTIVE, UIBUTTON_STATE_COUNT };

/// Visual illumination states for an analog thumbstick sprite (`Idle`, deflected `Active`, or `Depressed` on L3/R3).
enum ControllerUIStickStates {
    UISTICK_STATE_IDLE = 0,
    UISTICK_STATE_ACTIVE,
    UISTICK_STATE_DEPRESSED,
    UISTICK_STATE_COUNT
};

/// Unified 32-bit button bitmask combining Bluepad32 `dpad`, `buttons`, and `misc_buttons`.
enum UIButtonMask : uint32_t {
    UI_BUTTON_MASK_DPAD_UP = (1u << 0),
    UI_BUTTON_MASK_DPAD_LEFT = (1u << 1),
    UI_BUTTON_MASK_DPAD_DOWN = (1u << 2),
    UI_BUTTON_MASK_DPAD_RIGHT = (1u << 3),
    UI_BUTTON_MASK_A = (1u << 4),  ///< Physical South button (`BUTTON_A` in Bluepad32).
    UI_BUTTON_MASK_B = (1u << 5),  ///< Physical East button (`BUTTON_B` in Bluepad32).
    UI_BUTTON_MASK_X = (1u << 6),  ///< Physical West button (`BUTTON_X` in Bluepad32).
    UI_BUTTON_MASK_Y = (1u << 7),  ///< Physical North button (`BUTTON_Y` in Bluepad32).
    UI_BUTTON_MASK_L1 = (1u << 8),
    UI_BUTTON_MASK_L2 = (1u << 9),
    UI_BUTTON_MASK_L3 = (1u << 10),
    UI_BUTTON_MASK_R1 = (1u << 11),
    UI_BUTTON_MASK_R2 = (1u << 12),
    UI_BUTTON_MASK_R3 = (1u << 13),
    UI_BUTTON_MASK_SELECT = (1u << 14),
    UI_BUTTON_MASK_START = (1u << 15),
    UI_BUTTON_MASK_SYSTEM = (1u << 16),
    UI_BUTTON_MASK_CAPTURE = (1u << 17),
};

constexpr uint32_t MAX_UITEXTURE_STATES = 4;
constexpr size_t kTotalControllerSpriteAssets = 44;

/// Relative asset file paths for a button's active and idle PNG sprites.
struct ControllerUIButtonDefinition {
    const char* assetName_Active;
    const char* assetName_Idle;
};

/// Relative asset file paths for a thumbstick's active, depressed (L3/R3), and idle PNG sprites.
struct ControllerStickUIDefinition {
    const char* assetName_Active;
    const char* assetName_Depressed;
    const char* assetName_Idle;
};

/// OpenGL texture handles and pixel dimensions for a multi-state UI sprite.
struct UITextureInfo {
    TextureAssetHandle textureHandles[MAX_UITEXTURE_STATES];
    uint32_t textureWidth;
    uint32_t textureHeight;
};

/// Unscaled canvas coordinates, trigger bitmask, current state, and visibility for a button sprite.
struct ControllerButtonInfo {
    ImVec2 basePosition;                   ///< Unscaled center position `(x, y)` in canvas space.
    uint32_t buttonMask;                   ///< `UIButtonMask` bit that activates this button sprite.
    ControllerUIButtonStates buttonState;  ///< Current visual state (`IDLE` or `ACTIVE`).
    bool enabled;                          ///< True if this sprite is rendered for the current layout.
};

/**
 * @brief Translates a Bluepad32 `uni_gamepad_t` report into unified `UI_BUTTON_MASK_*` flags.
 *
 * Combines `gp.dpad`, `gp.buttons` (plus `gp.brake > 0` for `L2` and `gp.throttle > 0`
 * for `R2`), and `gp.misc_buttons` into a single 32-bit bitmask.
 *
 * Example:
 * @code
 * uni_gamepad_t gp{};
 * gp.dpad = DPAD_UP;
 * gp.buttons = BUTTON_A;
 * uint32_t mask = MapGamepadToUIButtonMask(gp);
 * // mask == (UI_BUTTON_MASK_DPAD_UP | UI_BUTTON_MASK_A)
 * @endcode
 */
uint32_t MapGamepadToUIButtonMask(const uni_gamepad_t& gp);

/**
 * @brief Normalizes a Bluepad32 thumbstick axis in `[-512, 511]` into `[-1.0f, 1.0f]`.
 *
 * When `dont_trim_deadzone` is `false`, raw deflections with `std::abs(raw_axis) < AXIS_THRESHOLD`
 * are trimmed to `0.0f`. Values outside `[-512, 512]` are clamped to `[-1.0f, 1.0f]`.
 *
 * Examples:
 *   - `NormalizeStickAxis(5, false)`   -> `0.0f` (trimmed by `AXIS_THRESHOLD`)
 *   - `NormalizeStickAxis(5, true)`    -> `5.0f / 512.0f` (`~0.00976f`, raw untrimmed)
 *   - `NormalizeStickAxis(-512, false)` -> `-1.0f`
 *
 * @param raw_axis           Raw Bluepad32 stick axis (`axis_x`, `axis_y`, `axis_rx`, or `axis_ry`).
 * @param dont_trim_deadzone If true, bypasses `AXIS_THRESHOLD` deadzone trimming.
 * @return Normalized axis deflection in `[-1.0f, 1.0f]`.
 */
float NormalizeStickAxis(int32_t raw_axis, bool dont_trim_deadzone);

/**
 * @brief Normalizes a Bluepad32 analog trigger axis in `[0, 1023]` into `[0.0f, 1.0f]`.
 *
 * If `raw_trigger > 0`, returns `std::clamp(raw_trigger / 1023.0f, 0.0f, 1.0f)`.
 * If `raw_trigger <= 0` (e.g., on controllers with digital-only L2/R2 triggers), falls
 * back to `1.0f` when `digital_pressed` is true, or `0.0f` otherwise.
 *
 * Examples:
 *   - `NormalizeTriggerAxis(512, false)` -> `512.0f / 1023.0f` (`~0.5005f`)
 *   - `NormalizeTriggerAxis(0, true)`    -> `1.0f` (digital L2/R2 fallback)
 *
 * @param raw_trigger     Raw Bluepad32 trigger value (`brake` or `throttle` in `[0, 1023]`).
 * @param digital_pressed True if `BUTTON_TRIGGER_L` or `BUTTON_TRIGGER_R` is set in `gp.buttons`.
 * @return Normalized trigger fill fraction in `[0.0f, 1.0f]`.
 */
float NormalizeTriggerAxis(int32_t raw_trigger, bool digital_pressed);

class ControllerUIData {
   public:
    /// Loads all button, thumbstick, and stick-region PNG sprites into OpenGL 2D textures.
    /// Must be called on Thread 1 with an active OpenGL context.
    static void LoadControllerUIData();

    /// Deletes all OpenGL 2D textures loaded by `LoadControllerUIData()`.
    /// Must be called on Thread 1 before destroying the OpenGL context.
    static void UnloadControllerUIData();

    /**
     * @brief Configures face-button sprite visibility, quad positions, and `buttonMask` bindings
     *        according to the active controller's layout family:
     *
     *   - `CONTROLLER_LAYOUT_STANDARD` (Xbox): `A` (South), `B` (East), `X` (West), `Y` (North).
     *   - `CONTROLLER_LAYOUT_SHAPES` (PlayStation): `Cross` (South), `Circle` (East),
     *     `Square` (West), `Triangle` (North).
     *   - `CONTROLLER_LAYOUT_REVERSE` (Nintendo Switch): Swaps BOTH quad positions and
     *     `buttonMask` fields so `B` (South) responds to `UI_BUTTON_MASK_A` (physical South),
     *     `A` (East) responds to `UI_BUTTON_MASK_B` (physical East), `Y` (West) responds to
     *     `UI_BUTTON_MASK_X` (physical West), and `X` (North) responds to `UI_BUTTON_MASK_Y`
     *     (physical North).
     */
    static void ConfigureButtonLayout(ControllerLayoutType layout);

    static const UITextureInfo& getUIButtonTextures(const ControllerUIButtons uiButton);

    static const UITextureInfo& getUIStickTextures();

    static const UITextureInfo& getUIStickRegionTexture();

    static ControllerButtonInfo& getControllerButtonInfo(const ControllerUIButtons uiButton);

    static ControllerButtonInfo& getControllerButtonInfo_ArcadeStick(const ControllerUIButtons uiButton);

    /// Returns the unscaled canvas center position `(x, y)` of the left or right thumbstick.
    static ImVec2 getStickPosition(const bool isLeftStick);

    /// Returns the unscaled canvas center position `(x, y)` of one of the 4 face-button quad
    /// slots (`UIBUTTON_DPAD_UP`=North, `LEFT`=West, `DOWN`=South, `RIGHT`=East).
    static ImVec2 getButtonQuadPosition(const ControllerUIButtons dpadButton);

    /// Returns the maximum unscaled pixel displacement radius (`64.0f`) for thumbstick deflection.
    static float getStickScale();

    /// Populates the unscaled canvas bounding box `[rectMin, rectMax]` for an L1/L2/R1/R2 trigger bar.
    static void getTriggerRectExtents(const ControllerUIButtons uiButton, ImVec2* rectMin, ImVec2* rectMax);

    /// Returns the array of all 44 sprite asset relative paths in `assets/gamecontroller/`.
    static const char* const* getAllSpriteAssetNames(size_t* out_count);
};
