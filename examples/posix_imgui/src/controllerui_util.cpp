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
 * @file controllerui_util.cpp
 * @brief Implementation of 2D gamepad sprite and trigger bar rendering helpers.
 *
 * Key Porting & Coordinate-Space Notes:
 *   1. `ImTextureID` in Dear ImGui 1.93.0 WIP (`external/imgui`):
 *      Unlike older Dear ImGui versions where `ImTextureID` was `void*`, Dear ImGui 1.93+
 *      defines `typedef ImU64 ImTextureID;`. In C++17, `reinterpret_cast<ImTextureID>(handle)`
 *      between two 64-bit unsigned integer types is ill-formed; `static_cast<ImTextureID>(handle)`
 *      is used throughout `Button()` and `Thumbstick()`.
 *   2. Window-Local vs. Screen-Space Coordinates in `TriggerBar()`:
 *      `ImGui::SetCursorPos()` (used by `Button()` and `Thumbstick()`) positions widgets
 *      in window-local space (`screen_pos = window->Pos - window->Scroll + cursor_pos`),
 *      whereas `ImDrawList::AddRectFilled()` / `AddRect()` require absolute screen-space
 *      coordinates. `TriggerBar()` adds `ImGui::GetWindowPos() - ImGui::GetScroll()` to
 *      `frameMin` and `frameMax` so the trigger fill bars align with the `L1/L2/R1/R2` sprites.
 */

#include "controllerui_util.h"
#include "controllerui_data.h"

namespace {

ImVec2 CalcPosFromCenter(const ImVec2& basePos, const ImVec2& texSize) {
    return ImVec2(basePos.x - (texSize.x * 0.5f), basePos.y - (texSize.y * 0.5f));
}

ImVec2 GetScaledBasePos(const ControllerUIPanelParams& panelParams, const ImVec2& basePos) {
    return ImVec2((basePos.x * panelParams.panelImageScale) + panelParams.panelBaseX,
                  (basePos.y * panelParams.panelImageScale) + panelParams.panelBaseY);
}

ImVec2 GetScaledTextureSize(const ControllerUIPanelParams& panelParams, const UITextureInfo& texInfo) {
    return ImVec2(static_cast<float>(texInfo.textureWidth) * panelParams.panelImageScale,
                  static_cast<float>(texInfo.textureHeight) * panelParams.panelImageScale);
}

}  // namespace

void ControllerUIUtil::Button(const ControllerUIPanelParams& panelParams,
                              const ControllerUIButtons buttonId,
                              const ControllerButtonInfo& buttonInfo) {
    const UITextureInfo& buttonTextures = ControllerUIData::getUIButtonTextures(buttonId);
    const TextureAssetHandle handle = buttonTextures.textureHandles[buttonInfo.buttonState];
    if (handle != TextureAssetLoader::INVALID_TEXTURE && handle != 0) {
        const ImVec2 buttonBasePos = GetScaledBasePos(panelParams, buttonInfo.basePosition);
        const ImVec2 buttonTextureSize = GetScaledTextureSize(panelParams, buttonTextures);
        ImGui::SetCursorPos(CalcPosFromCenter(buttonBasePos, buttonTextureSize));
        // Use `static_cast<ImTextureID>` because `ImTextureID` is `ImU64` in Dear ImGui 1.93+.
        ImGui::Image(static_cast<ImTextureID>(handle), buttonTextureSize);
    }
}

void ControllerUIUtil::Thumbstick(const ControllerUIPanelParams& panelParams,
                                  const ImVec2& basePos,
                                  const ImVec2& stickVals,
                                  const ControllerUIStickStates stickState) {
    const UITextureInfo& stickRegionTexture = ControllerUIData::getUIStickRegionTexture();
    const TextureAssetHandle regionHandle = stickRegionTexture.textureHandles[0];
    if (regionHandle != TextureAssetLoader::INVALID_TEXTURE && regionHandle != 0) {
        const ImVec2 stickRegionBasePos = GetScaledBasePos(panelParams, basePos);
        const ImVec2 stickRegionTextureSize = GetScaledTextureSize(panelParams, stickRegionTexture);
        ImGui::SetCursorPos(CalcPosFromCenter(stickRegionBasePos, stickRegionTextureSize));
        ImGui::Image(static_cast<ImTextureID>(regionHandle), stickRegionTextureSize);
    }

    const UITextureInfo& stickTextures = ControllerUIData::getUIStickTextures();
    const TextureAssetHandle stickHandle = stickTextures.textureHandles[stickState];
    if (stickHandle != TextureAssetLoader::INVALID_TEXTURE && stickHandle != 0) {
        const ImVec2 stickTextureSize = GetScaledTextureSize(panelParams, stickTextures);
        const ImVec2 adjustedStickPos =
            GetScaledBasePos(panelParams, ImVec2(basePos.x + stickVals.x, basePos.y + stickVals.y));
        ImGui::SetCursorPos(CalcPosFromCenter(adjustedStickPos, stickTextureSize));
        ImGui::Image(static_cast<ImTextureID>(stickHandle), stickTextureSize);
    }
}

void ControllerUIUtil::TriggerBar(const ControllerUIPanelParams& panelParams,
                                  ImDrawList* draw_list,
                                  const ControllerUIButtons buttonId,
                                  const float triggerValue,
                                  const float offsetY) {
    if (draw_list == nullptr) {
        return;
    }

    const ImU32 frameColor = ImColor(255, 255, 255, 255);
    const ImU32 fillColor = ImColor(20, 255, 20, 255);
    ImVec2 rawFrameMin, rawFrameMax;
    ControllerUIData::getTriggerRectExtents(buttonId, &rawFrameMin, &rawFrameMax);
    ImVec2 frameMin = GetScaledBasePos(panelParams, rawFrameMin);
    ImVec2 frameMax = GetScaledBasePos(panelParams, rawFrameMax);

    // Why: `GetScaledBasePos()` returns window-local coordinates (matching `ImGui::SetCursorPos()`),
    // whereas `ImDrawList::AddRectFilled()` and `AddRect()` expect screen-space coordinates.
    // Adding `ImGui::GetWindowPos() - ImGui::GetScroll()` converts `frameMin`/`frameMax` into
    // screen space so the bars stay locked to the L1/L2/R1/R2 sprites even when scrolled.
    const ImVec2 winOffset(ImGui::GetWindowPos().x - ImGui::GetScrollX(),
                           ImGui::GetWindowPos().y - ImGui::GetScrollY() + offsetY);
    frameMin.x += winOffset.x;
    frameMin.y += winOffset.y;
    frameMax.x += winOffset.x;
    frameMax.y += winOffset.y;

    const float frameWidth = frameMax.x - frameMin.x;
    const float fillAdjust = frameWidth * triggerValue;
    ImVec2 fillMin, fillMax;
    fillMin.y = frameMin.y;
    fillMax.y = frameMax.y;
    if (buttonId == UIBUTTON_L1 || buttonId == UIBUTTON_L2) {
        // Fill left to right
        fillMin.x = frameMin.x;
        fillMax.x = fillMin.x + fillAdjust;
        draw_list->AddRectFilled(fillMin, fillMax, fillColor);
        draw_list->AddRect(frameMin, frameMax, frameColor);
    } else if (buttonId == UIBUTTON_R1 || buttonId == UIBUTTON_R2) {
        // Fill right to left
        fillMax.x = frameMax.x;
        fillMin.x = fillMax.x - fillAdjust;
        draw_list->AddRectFilled(fillMin, fillMax, fillColor);
        draw_list->AddRect(frameMin, frameMax, frameColor);
    }
}
