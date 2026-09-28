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
 * @file controllerui_util.h
 * @brief Dear ImGui rendering primitives for 2D gamepad buttons, thumbsticks, and trigger bars.
 *
 * Architectural Role & Coordinate-Space Semantics:
 *   Executed exclusively on Thread 1 (the GLFW/ImGui UI thread) during `DemoScene::RenderPanel_ControlsTab()`.
 *
 *   - `Button()` and `Thumbstick()` position sprites via `ImGui::SetCursorPos()`, which
 *     operates in **window-local** coordinates (`screen_pos = window->Pos - window->Scroll + local_pos`).
 *   - `TriggerBar()` renders filled progress rectangles via `ImDrawList::AddRectFilled()` and
 *     `AddRect()`, which operate in **screen-space** coordinates. Therefore, `TriggerBar()`
 *     explicitly translates window-local extents into screen space by adding
 *     `ImGui::GetWindowPos() - ImGui::GetScroll()` so the green L1/L2/R1/R2 fill bars remain
 *     locked to the button sprites regardless of window position or scroll state.
 */

#pragma once

#include "controllerui_data.h"
#include "imgui.h"

/// Scaling and window-local origin parameters for the 2D controller sprite canvas.
struct ControllerUIPanelParams {
    float panelBaseX;       ///< Window-local X origin (`ImGui::GetCursorPosX()` at canvas start).
    float panelBaseY;       ///< Window-local Y origin (`ImGui::GetCursorPosY()` at canvas start).
    float panelImageScale;  ///< Uniform scale multiplier applied to all sprite positions and sizes.
};

class ControllerUIUtil {
   public:
    /**
     * @brief Renders a single button or D-Pad sprite (`Idle` or `Active` texture) centered at
     *        its scaled `buttonInfo.basePosition` in window-local space.
     *
     * @param panelParams Origin and scale parameters for the controller canvas.
     * @param buttonId    Button identifier (`UIBUTTON_A` .. `UIBUTTON_DPAD_RIGHT`).
     * @param buttonInfo  Position, mask, and active/idle state for the button.
     */
    static void Button(const ControllerUIPanelParams& panelParams,
                       const ControllerUIButtons buttonId,
                       const ControllerButtonInfo& buttonInfo);

    /**
     * @brief Renders a thumbstick's circular region background sprite and the deflected
     *        thumbstick cap sprite (`Idle`, `Active`, or `Depressed` on L3/R3).
     *
     * Note: Because `Thumbstick()` calls `ImGui::SetCursorPos()` at `basePos + stickVals`,
     * vertical deflection of the thumbstick alters the Dear ImGui cursor Y position. Callers
     * rendering widgets below the canvas must explicitly re-anchor `ImGui::SetCursorPos()`.
     *
     * @param panelParams Origin and scale parameters for the controller canvas.
     * @param basePos     Unscaled center position of the thumbstick (`ControllerUIData::getStickPosition()`).
     * @param stickVals   Unscaled stick deflection offset `(normX * STICK_SCALE, normY * STICK_SCALE)`.
     * @param stickState  Visual state (`UISTICK_STATE_IDLE`, `ACTIVE`, or `DEPRESSED`).
     */
    static void Thumbstick(const ControllerUIPanelParams& panelParams,
                           const ImVec2& basePos,
                           const ImVec2& stickVals,
                           const ControllerUIStickStates stickState);

    /**
     * @brief Renders an analog/digital fill bar and border frame next to `L1`, `L2`, `R1`, or `R2`.
     *
     * Left triggers (`L1`/`L2`) fill left-to-right; right triggers (`R1`/`R2`) fill right-to-left.
     * Translates window-local `panelParams` coordinates into screen-space coordinates for `draw_list`.
     *
     * @param panelParams  Origin and scale parameters for the controller canvas.
     * @param draw_list    Active window `ImDrawList*` (`ImGui::GetWindowDrawList()`); safely no-ops if null.
     * @param buttonId     Trigger button identifier (`UIBUTTON_L1`, `L2`, `R1`, or `R2`).
     * @param triggerValue Normalized fill fraction in `[0.0f, 1.0f]`.
     * @param offsetY      Additional vertical offset in pixels (`0.0f` when `panelBaseY` is window-local).
     */
    static void TriggerBar(const ControllerUIPanelParams& panelParams,
                           ImDrawList* draw_list,
                           const ControllerUIButtons buttonId,
                           const float triggerValue,
                           const float offsetY);
};
