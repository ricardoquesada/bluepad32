// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 The Android Open Source Project
// Copyright 2026 Ricardo Quesada
// http://retro.moe/unijoysticle2

/**
 * @file demo_scene.cpp
 * @brief Implementation of the 4-controller, 5-tab Dear ImGui controller tester scene.
 *
 * Architectural Role & UI Design Invariants:
 *   1. Executed exclusively on Thread 1 (the GLFW + OpenGL3 + Dear ImGui main thread).
 *   2. Dear ImGui `1.93.0 WIP` Font Scaling (`RenderPreferences`):
 *      Reads and writes `ImGui::GetStyle().FontScaleMain` rather than the deprecated
 *      `ImGui::GetIO().FontGlobalScale`.
 *   3. Post-Canvas Cursor Anchoring (`RenderPanel_ControlsTab`):
 *      Because `ControllerUIUtil::Thumbstick()` calls `ImGui::SetCursorPos()` at a Y
 *      coordinate offset by `rightStickValues.y`, moving the right analog stick vertically
 *      leaves the Dear ImGui layout cursor at a variable Y offset. Before rendering the
 *      live numeric telemetry bar below the controller sprites, `RenderPanel_ControlsTab()`
 *      explicitly anchors `ImGui::SetCursorPos()` at `mControllerPanelBaseY + 250.0f * mControllerPanelScale`
 *      so subsequent text never jitters vertically when the right thumbstick is deflected.
 *   4. Packet-Driven IMU Ring Buffer (`UpdateImuHistory`):
 *      Advances the 240-sample circular buffers (`mAccelHistory`, `mGyroHistory`) only
 *      when `snap.last_report_timestamp_us` changes, ensuring plots reflect genuine
 *      Bluetooth HID input reports rather than 60 Hz frame duplicates.
 */

#include "demo_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kControllerTabNames[kMaxControllers] = {
    " Controller #1 ",
    " Controller #2 ",
    " Controller #3 ",
    " Controller #4 ",
};

const ImVec4 kTextColorWhite = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
const ImVec4 kTextColorGrey = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
const ImVec4 kTextColorGreen = ImVec4(0.20f, 1.0f, 0.20f, 1.0f);
const ImVec4 kTextColorYellow = ImVec4(1.0f, 0.85f, 0.20f, 1.0f);
const ImVec4 kTextColorCyan = ImVec4(0.30f, 0.85f, 1.0f, 1.0f);

constexpr float kFontScaleMin = 0.50f;
constexpr float kFontScaleMax = 4.00f;
constexpr float kFontScaleStep = 0.25f;

constexpr float kVibrationDelayMin = 0.0f;
constexpr float kVibrationDelayMax = 1000.0f;
constexpr float kVibrationDelayStep = 50.0f;

constexpr float kVibrationDurationMin = 0.0f;
constexpr float kVibrationDurationMax = 2000.0f;
constexpr float kVibrationDurationStep = 100.0f;

constexpr float kVibrationIntensityMin = 0.0f;
constexpr float kVibrationIntensityMax = 1.0f;
constexpr float kVibrationIntensityStep = 0.05f;

// Renders a labeled `-` / `+` stepper row for Rumble parameters (`Start Delay`, `Duration`,
// `Weak Motor Intensity`, `Strong Motor Intensity`).
void VibrationParameters(const char* labelText,
                         const char* labelTag,
                         float vMin,
                         float vMax,
                         float vStep,
                         float* vValue) {
    char plusString[32];
    char minusString[32];
    std::snprintf(plusString, sizeof(plusString), " + ##%s", labelTag);
    std::snprintf(minusString, sizeof(minusString), " - ##%s", labelTag);

    ImGui::Spacing();
    ImGui::Text("%s", labelText);
    ImGui::SameLine(190.0f);
    if (ImGui::Button(minusString)) {
        *vValue = std::max(vMin, *vValue - vStep);
    }
    ImGui::SameLine();
    if (vMax > 1.0f) {
        ImGui::Text("%6.0f ms", static_cast<double>(*vValue));
    } else {
        ImGui::Text("  %1.2f (%3d)", static_cast<double>(*vValue), static_cast<int>(std::round(*vValue * 255.0f)));
    }
    ImGui::SameLine();
    if (ImGui::Button(plusString)) {
        *vValue = std::min(vMax, *vValue + vStep);
    }
}

const char* LayoutTypeToString(ControllerLayoutType layout) {
    switch (layout) {
        case CONTROLLER_LAYOUT_STANDARD:
            return "Standard (Xbox: A=South, B=East, X=West, Y=North)";
        case CONTROLLER_LAYOUT_SHAPES:
            return "Shapes (PlayStation: Cross, Circle, Square, Triangle)";
        case CONTROLLER_LAYOUT_REVERSE:
            return "Reversed (Nintendo Switch: B=South, A=East, Y=West, X=North)";
        default:
            return "Unknown";
    }
}

const char* SubtypeToString(uni_controller_subtype_t subtype) {
    switch (subtype) {
        case CONTROLLER_SUBTYPE_NONE:
            return "None (Default)";
        case CONTROLLER_SUBTYPE_WIIMOTE_HORIZONTAL:
            return "Wiimote Horizontal";
        case CONTROLLER_SUBTYPE_WIIMOTE_VERTICAL:
            return "Wiimote Vertical";
        case CONTROLLER_SUBTYPE_WIIMOTE_ACCEL:
            return "Wiimote Accel";
        case CONTROLLER_SUBTYPE_WIIMOTE_NUNCHUK:
            return "Wiimote + Nunchuk";
        case CONTROLLER_SUBTYPE_WIIMOTE_NUNCHUK_ACCEL:
            return "Wiimote + Nunchuk Accel";
        case CONTROLLER_SUBTYPE_WII_CLASSIC:
            return "Wii Classic Controller";
        case CONTROLLER_SUBTYPE_WIIUPRO:
            return "Wii U Pro Controller";
        case CONTROLLER_SUBTYPE_WII_BALANCE_BOARD:
            return "Wii Balance Board";
        case CONTROLLER_SUBTYPE_WIIMOTE_UDRAW_TABLET:
            return "uDraw GameTablet";
        default:
            return "Custom / Other";
    }
}

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kGravityMps2 = 9.80665f;

float AccelCountsPerG(uint16_t vendor_id) {
    // Sony DualShock 4 / DualSense parsers normalize 1g to 8192 counts;
    // Nintendo Switch parsers normalize 1g to ~4096 counts.
    if (vendor_id == 0x057e) {
        return 4096.0f;
    }
    return 8192.0f;
}

float GyroCountsPerDegPerSec(uint16_t vendor_id) {
    // Sony DualShock 4 / DualSense parsers use 1024 counts per deg/s;
    // Nintendo Switch parser uses 1000 counts per deg/s.
    if (vendor_id == 0x057e) {
        return 1000.0f;
    }
    return 1024.0f;
}

void DrawTextCenteredInColumn(float colStartX, float colWidth, const ImVec4& color, const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    const ImVec2 sz = ImGui::CalcTextSize(buf);
    ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - sz.x) * 0.5f));
    ImGui::TextColored(color, "%s", buf);
}

void ApplyRadialDeadzone(int32_t rawX, int32_t rawY, float deadzone, float* outX, float* outY) {
    float nx = std::clamp(static_cast<float>(rawX) / (rawX < 0 ? 512.0f : 511.0f), -1.0f, 1.0f);
    float ny = std::clamp(static_cast<float>(rawY) / (rawY < 0 ? 512.0f : 511.0f), -1.0f, 1.0f);
    const float mag = std::hypot(nx, ny);
    if (deadzone > 0.0f && mag < deadzone) {
        *outX = 0.0f;
        *outY = 0.0f;
        return;
    }
    *outX = nx;
    *outY = ny;
}

float NormalizeTriggerAxis(int32_t raw_trigger, bool digital_fallback) {
    if (raw_trigger > 0) {
        return std::clamp(static_cast<float>(raw_trigger) / 1023.0f, 0.0f, 1.0f);
    }
    return digital_fallback ? 1.0f : 0.0f;
}

void DrawVectorButtonBadge(ImDrawList* drawList,
                           ImVec2 minPos,
                           ImVec2 size,
                           const char* primaryText,
                           const char* subText,
                           bool active) {
    const ImVec2 maxPos(minPos.x + size.x, minPos.y + size.y);
    const ImU32 fillCol = active ? IM_COL32(45, 130, 225, 255) : IM_COL32(36, 42, 54, 255);
    const ImU32 borderCol = active ? IM_COL32(120, 220, 255, 255) : IM_COL32(120, 135, 158, 200);
    const float borderThick = active ? 2.0f : 1.3f;

    drawList->AddRectFilled(minPos, maxPos, fillCol, 6.0f);
    drawList->AddRect(minPos, maxPos, borderCol, 6.0f, 0, borderThick);

    if (subText == nullptr) {
        const ImVec2 pSz = ImGui::CalcTextSize(primaryText);
        const ImVec2 pPos(minPos.x + (size.x - pSz.x) * 0.5f, minPos.y + (size.y - pSz.y) * 0.5f);
        drawList->AddText(pPos, IM_COL32(255, 255, 255, 255), primaryText);
    } else {
        const ImVec2 pSz = ImGui::CalcTextSize(primaryText);
        const ImVec2 sSz = ImGui::CalcTextSize(subText);
        const float totalH = pSz.y + sSz.y + 1.0f;
        const float startY = minPos.y + (size.y - totalH) * 0.5f;
        drawList->AddText(ImVec2(minPos.x + (size.x - pSz.x) * 0.5f, startY), IM_COL32(255, 255, 255, 255),
                          primaryText);
        drawList->AddText(ImVec2(minPos.x + (size.x - sSz.x) * 0.5f, startY + pSz.y + 1.0f),
                          active ? IM_COL32(235, 248, 255, 255) : IM_COL32(150, 165, 185, 230), subText);
    }
}

void DrawStickWellWidget(const char* title,
                         int32_t rawX,
                         int32_t rawY,
                         float normX,
                         float normY,
                         float deadzone,
                         bool thumbPressed) {
    const float colStartX = ImGui::GetCursorPosX();
    const float colWidth = ImGui::GetContentRegionAvail().x;
    DrawTextCenteredInColumn(colStartX, colWidth, thumbPressed ? kTextColorCyan : kTextColorWhite, "%s", title);
    ImGui::Spacing();

    constexpr float kOuterRadius = 46.0f;
    constexpr float kInnerRadius = 40.0f;
    constexpr float kDiameter = kOuterRadius * 2.0f;

    ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - kDiameter) * 0.5f));
    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(kDiameter, kDiameter));

    const ImVec2 center(canvasMin.x + kOuterRadius, canvasMin.y + kOuterRadius);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Circular well backdrop and dual concentric boundary rings
    drawList->AddCircleFilled(center, kOuterRadius, IM_COL32(20, 24, 32, 230), 64);
    drawList->AddCircle(center, kOuterRadius, thumbPressed ? IM_COL32(90, 225, 255, 255) : IM_COL32(140, 155, 180, 200),
                        64, thumbPressed ? 2.4f : 1.5f);
    drawList->AddCircle(center, kInnerRadius, IM_COL32(105, 120, 145, 170), 64, 1.2f);

    if (deadzone > 0.005f) {
        drawList->AddCircle(center, kInnerRadius * deadzone, IM_COL32(90, 105, 125, 100), 48, 1.0f);
    }

    // Crosshairs
    drawList->AddLine(ImVec2(center.x - kInnerRadius, center.y), ImVec2(center.x + kInnerRadius, center.y),
                      IM_COL32(90, 105, 125, 140), 1.0f);
    drawList->AddLine(ImVec2(center.x, center.y - kInnerRadius), ImVec2(center.x, center.y + kInnerRadius),
                      IM_COL32(90, 105, 125, 140), 1.0f);

    // Clamp visual puck position to the circular gate
    float clampedX = normX;
    float clampedY = normY;
    const float mag = std::hypot(clampedX, clampedY);
    if (mag > 1.0f) {
        clampedX /= mag;
        clampedY /= mag;
    }

    const float maxTravel = kInnerRadius - 7.0f;
    const ImVec2 puckPos(center.x + clampedX * maxTravel, center.y + clampedY * maxTravel);

    drawList->AddLine(center, puckPos, IM_COL32(70, 160, 240, 110), 1.5f);
    drawList->AddCircleFilled(puckPos, 7.5f, thumbPressed ? IM_COL32(70, 210, 255, 255) : IM_COL32(55, 110, 195, 255),
                              24);
    drawList->AddCircle(puckPos, 7.5f, IM_COL32(190, 230, 255, 230), 24, 1.2f);
    drawList->AddCircleFilled(puckPos, 2.2f, IM_COL32(255, 255, 255, 255), 12);

    ImGui::Spacing();
    DrawTextCenteredInColumn(colStartX, colWidth, kTextColorGrey, "X: %+0.2f   Y: %+0.2f", static_cast<double>(normX),
                             static_cast<double>(normY));
    DrawTextCenteredInColumn(colStartX, colWidth, kTextColorGrey, "(%d, %d)", rawX, rawY);
}

void DrawDPadCrossWidget(uint8_t dpad) {
    const float colStartX = ImGui::GetCursorPosX();
    const float colWidth = ImGui::GetContentRegionAvail().x;
    DrawTextCenteredInColumn(colStartX, colWidth, kTextColorWhite, "D-Pad");
    ImGui::Spacing();

    constexpr float kCellSize = 30.0f;
    constexpr float kStep = 34.0f;
    constexpr float kCanvasSize = kStep * 2.0f + kCellSize + 4.0f;

    ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - kCanvasSize) * 0.5f));
    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(kCanvasSize, kCanvasSize));

    const ImVec2 center(canvasMin.x + kCanvasSize * 0.5f, canvasMin.y + kCanvasSize * 0.5f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    auto drawCell = [&](float cx, float cy, bool active, bool isCenter) {
        const ImVec2 cMin(cx - kCellSize * 0.5f, cy - kCellSize * 0.5f);
        const ImVec2 cMax(cx + kCellSize * 0.5f, cy + kCellSize * 0.5f);
        const ImU32 fillCol =
            isCenter ? IM_COL32(30, 35, 45, 255) : (active ? IM_COL32(45, 130, 225, 255) : IM_COL32(38, 44, 56, 255));
        const ImU32 borderCol = active ? IM_COL32(120, 220, 255, 255) : IM_COL32(120, 135, 158, 190);
        drawList->AddRectFilled(cMin, cMax, fillCol, 5.0f);
        drawList->AddRect(cMin, cMax, borderCol, 5.0f, 0, active ? 2.0f : 1.2f);
    };

    const bool up = (dpad & DPAD_UP) != 0;
    const bool down = (dpad & DPAD_DOWN) != 0;
    const bool left = (dpad & DPAD_LEFT) != 0;
    const bool right = (dpad & DPAD_RIGHT) != 0;

    // Up cell + chevron ^
    {
        const float cx = center.x;
        const float cy = center.y - kStep;
        drawCell(cx, cy, up, false);
        const ImU32 iconCol = up ? IM_COL32(255, 255, 255, 255) : IM_COL32(185, 198, 215, 230);
        drawList->AddLine(ImVec2(cx - 5.0f, cy + 2.5f), ImVec2(cx, cy - 3.0f), iconCol, 2.0f);
        drawList->AddLine(ImVec2(cx, cy - 3.0f), ImVec2(cx + 5.0f, cy + 2.5f), iconCol, 2.0f);
    }
    // Left cell + chevron <
    {
        const float cx = center.x - kStep;
        const float cy = center.y;
        drawCell(cx, cy, left, false);
        const ImU32 iconCol = left ? IM_COL32(255, 255, 255, 255) : IM_COL32(185, 198, 215, 230);
        drawList->AddLine(ImVec2(cx + 2.5f, cy - 5.0f), ImVec2(cx - 3.0f, cy), iconCol, 2.0f);
        drawList->AddLine(ImVec2(cx - 3.0f, cy), ImVec2(cx + 2.5f, cy + 5.0f), iconCol, 2.0f);
    }
    // Center neutral cell
    {
        drawCell(center.x, center.y, false, true);
        drawList->AddCircleFilled(center, 4.5f, IM_COL32(115, 130, 150, 160), 16);
    }
    // Right cell + chevron >
    {
        const float cx = center.x + kStep;
        const float cy = center.y;
        drawCell(cx, cy, right, false);
        const ImU32 iconCol = right ? IM_COL32(255, 255, 255, 255) : IM_COL32(185, 198, 215, 230);
        drawList->AddLine(ImVec2(cx - 2.5f, cy - 5.0f), ImVec2(cx + 3.0f, cy), iconCol, 2.0f);
        drawList->AddLine(ImVec2(cx + 3.0f, cy), ImVec2(cx - 2.5f, cy + 5.0f), iconCol, 2.0f);
    }
    // Down cell + chevron v
    {
        const float cx = center.x;
        const float cy = center.y + kStep;
        drawCell(cx, cy, down, false);
        const ImU32 iconCol = down ? IM_COL32(255, 255, 255, 255) : IM_COL32(185, 198, 215, 230);
        drawList->AddLine(ImVec2(cx - 5.0f, cy - 2.5f), ImVec2(cx, cy + 3.0f), iconCol, 2.0f);
        drawList->AddLine(ImVec2(cx, cy + 3.0f), ImVec2(cx + 5.0f, cy - 2.5f), iconCol, 2.0f);
    }
}

enum class PlayStationShape { kTriangle, kSquare, kCircle, kCross };

void DrawActionButtonsDiamondWidget(uint16_t buttons, ControllerLayoutType layout) {
    const float colStartX = ImGui::GetCursorPosX();
    const float colWidth = ImGui::GetContentRegionAvail().x;
    DrawTextCenteredInColumn(colStartX, colWidth, kTextColorWhite, "Action Buttons");
    ImGui::Spacing();

    constexpr float kBtnRadius = 19.0f;
    constexpr float kStep = 33.0f;
    constexpr float kCanvasSize = (kStep + kBtnRadius) * 2.0f + 8.0f;

    ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - kCanvasSize) * 0.5f));
    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(kCanvasSize, kCanvasSize));

    const ImVec2 center(canvasMin.x + kCanvasSize * 0.5f, canvasMin.y + kCanvasSize * 0.5f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    auto drawFaceButton = [&](float bx, float by, const char* letter, PlayStationShape shape, bool active) {
        const ImVec2 bCenter(bx, by);
        const ImU32 fillCol = active ? IM_COL32(45, 130, 225, 255) : IM_COL32(38, 44, 56, 255);
        const ImU32 borderCol = active ? IM_COL32(120, 220, 255, 255) : IM_COL32(130, 145, 168, 210);
        drawList->AddCircleFilled(bCenter, kBtnRadius, fillCol, 32);
        drawList->AddCircle(bCenter, kBtnRadius, borderCol, 32, active ? 2.2f : 1.4f);

        const ImVec2 lSz = ImGui::CalcTextSize(letter);
        drawList->AddText(ImVec2(bx - lSz.x * 0.5f, by - lSz.y + 1.0f), IM_COL32(255, 255, 255, 255), letter);

        const float sy = by + 7.5f;
        const ImU32 shapeCol = active ? IM_COL32(240, 250, 255, 255) : IM_COL32(150, 168, 190, 220);
        switch (shape) {
            case PlayStationShape::kTriangle:
                drawList->AddTriangle(ImVec2(bx, sy - 3.8f), ImVec2(bx - 4.0f, sy + 3.2f), ImVec2(bx + 4.0f, sy + 3.2f),
                                      shapeCol, 1.3f);
                break;
            case PlayStationShape::kSquare:
                drawList->AddRect(ImVec2(bx - 3.5f, sy - 3.5f), ImVec2(bx + 3.5f, sy + 3.5f), shapeCol, 0.0f, 0, 1.3f);
                break;
            case PlayStationShape::kCircle:
                drawList->AddCircle(ImVec2(bx, sy), 3.7f, shapeCol, 20, 1.3f);
                break;
            case PlayStationShape::kCross:
                drawList->AddLine(ImVec2(bx - 3.3f, sy - 3.3f), ImVec2(bx + 3.3f, sy + 3.3f), shapeCol, 1.4f);
                drawList->AddLine(ImVec2(bx + 3.3f, sy - 3.3f), ImVec2(bx - 3.3f, sy + 3.3f), shapeCol, 1.4f);
                break;
        }
    };

    const bool isNintendo = (layout == CONTROLLER_LAYOUT_REVERSE);
    // North (Y on Xbox / X on Switch, Triangle on PS)
    drawFaceButton(center.x, center.y - kStep, isNintendo ? "X" : "Y", PlayStationShape::kTriangle,
                   (buttons & BUTTON_Y) != 0);
    // West (X on Xbox / Y on Switch, Square on PS)
    drawFaceButton(center.x - kStep, center.y, isNintendo ? "Y" : "X", PlayStationShape::kSquare,
                   (buttons & BUTTON_X) != 0);
    // East (B on Xbox / A on Switch, Circle on PS)
    drawFaceButton(center.x + kStep, center.y, isNintendo ? "A" : "B", PlayStationShape::kCircle,
                   (buttons & BUTTON_B) != 0);
    // South (A on Xbox / B on Switch, Cross on PS)
    drawFaceButton(center.x, center.y + kStep, isNintendo ? "B" : "A", PlayStationShape::kCross,
                   (buttons & BUTTON_A) != 0);
}

}  // namespace

DemoScene::DemoScene()
    : mSnapshots{},
      mPrevConnected{false, false, false, false},
      mMostRecentConnectedSlot(-1),
      mCurrentControllerSlot(0),
      mActiveControllerPanelTab(0),
      mFontScale(1.0f),
      mRadialDeadzone(0.10f),
      mDontTrimDeadzone(false),
      mPreferencesActive(false),
      mLastDetectedInput{},
      mPrevButtons{},
      mPrevDpad{},
      mPrevMiscButtons{},
      mPrevL2Active{},
      mPrevR2Active{},
      mPrevLeftStickActive{},
      mPrevRightStickActive{},
      mRumbleDelayMs{},
      mRumbleDurationMs{},
      mRumbleWeakIntensity{},
      mRumbleStrongIntensity{},
      mGyroHistory{},
      mAccelHistory{},
      mGyroAngleDeg{},
      mImuHistoryOffset{},
      mLastImuTimestampUs{},
      mImuPlotPaused(false),
      mPlayerLedIndex{},
      mPlayerLedBits{},
      mRgbColor{},
      mRgbLiveUpdate{},
      mBrightnessPercent{} {
    for (int i = 0; i < kMaxControllers; ++i) {
        std::snprintf(mLastDetectedInput[i], sizeof(mLastDetectedInput[i]), "None");

        mRumbleDelayMs[i] = 0.0f;
        mRumbleDurationMs[i] = 500.0f;
        mRumbleWeakIntensity[i] = 0.50f;
        mRumbleStrongIntensity[i] = 0.50f;

        mPlayerLedIndex[i] = i + 1;
        for (int b = 0; b < 4; ++b) {
            mPlayerLedBits[i][b] = (b == i);
        }

        // Default RGB lightbar color: PlayStation Blue
        mRgbColor[i][0] = 0.0f;
        mRgbColor[i][1] = 0.25f;
        mRgbColor[i][2] = 1.0f;
        mRgbLiveUpdate[i] = false;

        mBrightnessPercent[i] = 100;
    }
}

DemoScene::~DemoScene() = default;

void DemoScene::UpdateImuHistory(int slot, const ControllerSnapshot& snap) {
    if (slot < 0 || slot >= kMaxControllers) {
        return;
    }
    if (!snap.connected) {
        if (mPrevConnected[slot]) {
            ClearImuHistory(slot);
        }
        return;
    }
    if (mImuPlotPaused) {
        return;
    }
    // Only record a new sample when `last_report_timestamp_us` advances so a controller
    // reporting at e.g. 30 Hz is not duplicated across consecutive 60 Hz UI frames.
    if (snap.last_report_timestamp_us == 0 || snap.last_report_timestamp_us == mLastImuTimestampUs[slot]) {
        return;
    }

    float dt_sec = 0.0f;
    if (mLastImuTimestampUs[slot] > 0 && snap.last_report_timestamp_us > mLastImuTimestampUs[slot]) {
        dt_sec = static_cast<float>(snap.last_report_timestamp_us - mLastImuTimestampUs[slot]) * 1e-6f;
    } else if (snap.report_delta_ms > 0 && snap.report_delta_ms <= 500) {
        dt_sec = static_cast<float>(snap.report_delta_ms) * 1e-3f;
    }
    mLastImuTimestampUs[slot] = snap.last_report_timestamp_us;

    const float gyroScale = GyroCountsPerDegPerSec(snap.vendor_id);
    const size_t idx = mImuHistoryOffset[slot];
    for (size_t axis = 0; axis < kMotionAxisCount; ++axis) {
        const float rawGyro = static_cast<float>(snap.controller.gamepad.gyro[axis]);
        const float rawAccel = static_cast<float>(snap.controller.gamepad.accel[axis]);
        mGyroHistory[slot][axis][idx] = rawGyro;
        mAccelHistory[slot][axis][idx] = rawAccel;

        if (dt_sec > 0.0f && dt_sec <= 0.5f) {
            const float degPerSec = rawGyro / gyroScale;
            // Integrate angular rate into [-180, +180] degree dial angle
            mGyroAngleDeg[slot][axis] = std::remainder(mGyroAngleDeg[slot][axis] + degPerSec * dt_sec, 360.0f);
        }
    }
    mImuHistoryOffset[slot] = (idx + 1) % kImuHistoryLen;
}

void DemoScene::ClearImuHistory(int slot) {
    if (slot < 0 || slot >= kMaxControllers) {
        return;
    }
    std::memset(mGyroHistory[slot], 0, sizeof(mGyroHistory[slot]));
    std::memset(mAccelHistory[slot], 0, sizeof(mAccelHistory[slot]));
    std::memset(mGyroAngleDeg[slot], 0, sizeof(mGyroAngleDeg[slot]));
    mImuHistoryOffset[slot] = 0;
    mLastImuTimestampUs[slot] = 0;
}

void DemoScene::UpdateLastDetectedInput(int slot, const ControllerSnapshot& snap) {
    if (slot < 0 || slot >= kMaxControllers) {
        return;
    }
    if (!snap.connected) {
        std::snprintf(mLastDetectedInput[slot], sizeof(mLastDetectedInput[slot]), "None");
        mPrevButtons[slot] = 0;
        mPrevDpad[slot] = 0;
        mPrevMiscButtons[slot] = 0;
        mPrevL2Active[slot] = false;
        mPrevR2Active[slot] = false;
        mPrevLeftStickActive[slot] = false;
        mPrevRightStickActive[slot] = false;
        return;
    }

    const uni_gamepad_t& gp = snap.controller.gamepad;
    const uint16_t newBtns = static_cast<uint16_t>(gp.buttons & ~mPrevButtons[slot]);
    const uint8_t newDpad = static_cast<uint8_t>(gp.dpad & ~mPrevDpad[slot]);
    const uint8_t newMisc = static_cast<uint8_t>(gp.misc_buttons & ~mPrevMiscButtons[slot]);

    auto setDetected = [&](const char* label) {
        std::snprintf(mLastDetectedInput[slot], sizeof(mLastDetectedInput[slot]), "%s", label);
    };

    if (newDpad & DPAD_UP) {
        setDetected("D-Pad Up");
    } else if (newDpad & DPAD_DOWN) {
        setDetected("D-Pad Down");
    } else if (newDpad & DPAD_LEFT) {
        setDetected("D-Pad Left");
    } else if (newDpad & DPAD_RIGHT) {
        setDetected("D-Pad Right");
    }

    const bool isNintendo = (snap.layout == CONTROLLER_LAYOUT_REVERSE);
    if (newBtns & BUTTON_A) {
        setDetected(isNintendo ? "Button B (South)" : "Button A / Cross");
    } else if (newBtns & BUTTON_B) {
        setDetected(isNintendo ? "Button A (East)" : "Button B / Circle");
    } else if (newBtns & BUTTON_X) {
        setDetected(isNintendo ? "Button Y (West)" : "Button X / Square");
    } else if (newBtns & BUTTON_Y) {
        setDetected(isNintendo ? "Button X (North)" : "Button Y / Triangle");
    } else if (newBtns & BUTTON_SHOULDER_L) {
        setDetected("LB / L1");
    } else if (newBtns & BUTTON_SHOULDER_R) {
        setDetected("RB / R1");
    } else if (newBtns & BUTTON_THUMB_L) {
        setDetected("Left Stick (L3)");
    } else if (newBtns & BUTTON_THUMB_R) {
        setDetected("Right Stick (R3)");
    } else if (newBtns & BUTTON_TRIGGER_L) {
        setDetected("LT / L2");
    } else if (newBtns & BUTTON_TRIGGER_R) {
        setDetected("RT / R2");
    }

    if (newMisc & MISC_BUTTON_SELECT) {
        setDetected("Select / Share");
    } else if (newMisc & MISC_BUTTON_SYSTEM) {
        setDetected("Mode / Guide / PS");
    } else if (newMisc & MISC_BUTTON_START) {
        setDetected("Start / Options");
    } else if (newMisc & MISC_BUTTON_CAPTURE) {
        setDetected("Capture / Mute");
    }

    const float l2Val = NormalizeTriggerAxis(gp.brake, (gp.buttons & BUTTON_TRIGGER_L) != 0);
    const float r2Val = NormalizeTriggerAxis(gp.throttle, (gp.buttons & BUTTON_TRIGGER_R) != 0);
    const bool l2Active = (l2Val > 0.15f);
    const bool r2Active = (r2Val > 0.15f);
    if (l2Active && !mPrevL2Active[slot]) {
        setDetected("LT / L2");
    }
    if (r2Active && !mPrevR2Active[slot]) {
        setDetected("RT / R2");
    }

    const float effectiveDeadzone = mDontTrimDeadzone ? 0.0f : std::max(0.15f, mRadialDeadzone);
    float lx = 0.0f, ly = 0.0f, rx = 0.0f, ry = 0.0f;
    ApplyRadialDeadzone(gp.axis_x, gp.axis_y, effectiveDeadzone, &lx, &ly);
    ApplyRadialDeadzone(gp.axis_rx, gp.axis_ry, effectiveDeadzone, &rx, &ry);
    const bool leftActive = (std::hypot(lx, ly) > effectiveDeadzone);
    const bool rightActive = (std::hypot(rx, ry) > effectiveDeadzone);
    if (leftActive && !mPrevLeftStickActive[slot]) {
        setDetected("Left Stick");
    }
    if (rightActive && !mPrevRightStickActive[slot]) {
        setDetected("Right Stick");
    }

    mPrevButtons[slot] = gp.buttons;
    mPrevDpad[slot] = gp.dpad;
    mPrevMiscButtons[slot] = gp.misc_buttons;
    mPrevL2Active[slot] = l2Active;
    mPrevR2Active[slot] = r2Active;
    mPrevLeftStickActive[slot] = leftActive;
    mPrevRightStickActive[slot] = rightActive;
}

void DemoScene::DoFrame() {
    int newly_connected = -1;
    posix_imgui_get_snapshots(mSnapshots, &newly_connected);
    if (newly_connected >= 0 && newly_connected < kMaxControllers) {
        mMostRecentConnectedSlot = newly_connected;
    }

    for (int i = 0; i < kMaxControllers; ++i) {
        if (!mPrevConnected[i] && mSnapshots[i].connected) {
            mMostRecentConnectedSlot = i;
        }
        UpdateImuHistory(i, mSnapshots[i]);
        UpdateLastDetectedInput(i, mSnapshots[i]);
        mPrevConnected[i] = mSnapshots[i].connected;
    }

    SetupUIWindow();

    if (!RenderPreferences()) {
        ImGui::SameLine(0.0f, 24.0f);
        RenderStatusBar();
        ImGui::Separator();
        RenderControllerTabs();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void DemoScene::SetupUIWindow() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowPosition(0.0f, 0.0f);
    ImVec2 minWindowSize(io.DisplaySize.x, io.DisplaySize.y);
    ImVec2 maxWindowSize = io.DisplaySize;
    ImGui::SetNextWindowPos(windowPosition);
    ImGui::SetNextWindowSizeConstraints(minWindowSize, maxWindowSize, nullptr, nullptr);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 20.0f);
    ImGui::Begin("Bluepad32 POSIX Controller Tester (Dear ImGui)", nullptr, windowFlags);
}

void DemoScene::RenderStatusBar() {
    int connected_count = 0;
    for (int i = 0; i < kMaxControllers; ++i) {
        if (mSnapshots[i].connected) {
            connected_count++;
        }
    }

    if (connected_count > 0) {
        ImGui::TextColored(kTextColorGreen, "Connected Controllers: %d / %d", connected_count, kMaxControllers);
    } else {
        ImGui::TextColored(kTextColorYellow, "Connected Controllers: 0 / %d (Scanning via Bluetooth...)",
                           kMaxControllers);
    }

    ImGui::SameLine(0.0f, 24.0f);
    ImGui::TextColored(kTextColorGrey, "Radial Deadzone: %.0f%% | %.1f FPS",
                       static_cast<double>((mDontTrimDeadzone ? 0.0f : mRadialDeadzone) * 100.0f),
                       static_cast<double>(ImGui::GetIO().Framerate));
}

bool DemoScene::RenderPreferences() {
    if (!mPreferencesActive) {
        if (ImGui::Button("Preferences...")) {
            mPreferencesActive = true;
        }
        return false;
    }

    // Dear ImGui 1.92+ / 1.93.0 WIP uses `style.FontScaleMain` instead of legacy `io.FontGlobalScale`.
    ImGuiStyle& style = ImGui::GetStyle();
    mFontScale = style.FontScaleMain;

    ImGui::TextColored(kTextColorCyan, "Display & Input Preferences");
    ImGui::Separator();

    ImGui::Spacing();
    ImGui::Text("Font scale:");
    ImGui::SameLine(180.0f);
    if (ImGui::Button(" - ##font")) {
        mFontScale = std::max(kFontScaleMin, mFontScale - kFontScaleStep);
        style.FontScaleMain = mFontScale;
    }
    ImGui::SameLine();
    ImGui::Text("%2.2fx", static_cast<double>(mFontScale));
    ImGui::SameLine();
    if (ImGui::Button(" + ##font")) {
        mFontScale = std::min(kFontScaleMax, mFontScale + kFontScaleStep);
        style.FontScaleMain = mFontScale;
    }
    ImGui::SameLine(0.0f, 16.0f);
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::SliderFloat("##font_slider", &mFontScale, kFontScaleMin, kFontScaleMax, "%.2fx")) {
        style.FontScaleMain = mFontScale;
    }

    ImGui::Spacing();
    ImGui::Checkbox("Raw deadzone (do not trim stick center deadzone to 0.0)", &mDontTrimDeadzone);

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("    OK    ")) {
        mPreferencesActive = false;
    }
    return true;
}

void DemoScene::RenderControllerTabs() {
    if (ImGui::BeginTabBar("ControllerTabBar", ImGuiTabBarFlags_NoTooltip)) {
        for (int slot = 0; slot < kMaxControllers; ++slot) {
            ImGuiTabItemFlags tabItemFlags = ImGuiTabItemFlags_None;
            // Auto-focus a newly connected controller tab for a single frame, then clear
            // `mMostRecentConnectedSlot` so the user can freely switch tabs afterward.
            if (mMostRecentConnectedSlot == slot) {
                tabItemFlags |= ImGuiTabItemFlags_SetSelected;
                mMostRecentConnectedSlot = -1;
            }

            const bool isConnected = mSnapshots[slot].connected;
            const ImVec4 tabTextColor = isConnected ? kTextColorWhite : kTextColorGrey;

            ImGui::PushStyleColor(ImGuiCol_Text, tabTextColor);
            if (ImGui::BeginTabItem(kControllerTabNames[slot], nullptr, tabItemFlags)) {
                mCurrentControllerSlot = slot;
                ImGui::PopStyleColor(1);

                if (isConnected) {
                    RenderPanel(slot, mSnapshots[slot]);
                } else {
                    ImGui::Spacing();
                    ImGui::TextColored(kTextColorGrey, "Slot #%d (Seat %c): Not connected", slot + 1, 'A' + slot);
                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        "Place a Bluetooth controller (DualSense, DualShock 4, DualShock 3, "
                        "Xbox Wireless Controller, Nintendo Switch Pro Controller, Joy-Con, "
                        "Wiimote, 8BitDo, etc.) into pairing mode to connect automatically.");
                }
                ImGui::EndTabItem();
            } else {
                ImGui::PopStyleColor(1);
            }
        }
        ImGui::EndTabBar();
    }
}

void DemoScene::RenderPanel(int slot, const ControllerSnapshot& snap) {
    const char* displayName = (snap.name[0] != '\0') ? snap.name : snap.model_name;
    ImGui::TextColored(kTextColorGreen, "[Seat #%d] %s", slot + 1, displayName);
    ImGui::SameLine(0.0f, 16.0f);
    ImGui::TextColored(kTextColorGrey, "(Model: %s | VID: 0x%04X PID: 0x%04X | MAC: %s)", snap.model_name,
                       snap.vendor_id, snap.product_id, bd_addr_to_str(snap.btaddr));

    if (ImGui::BeginTabBar("CategoryTabBar", ImGuiTabBarFlags_NoTooltip)) {
        struct CategoryTab {
            int index;
            const char* title;
            void (DemoScene::*renderFn)(int, const ControllerSnapshot&);
        };

        const CategoryTab kTabs[] = {
            {0, " Controls ", &DemoScene::RenderPanel_ControlsTab},
            {1, " Rumble ", &DemoScene::RenderPanel_VibrationTab},
            {2, " IMU ", &DemoScene::RenderPanel_MotionTab},
            {3, " Lights ", &DemoScene::RenderPanel_LightsTab},
            {4, " Info ", &DemoScene::RenderPanel_InfoTab},
        };

        for (const CategoryTab& tab : kTabs) {
            const bool isActive = (mActiveControllerPanelTab == tab.index);
            const ImVec4 tabColor = isActive ? kTextColorWhite : kTextColorGrey;
            ImGui::PushStyleColor(ImGuiCol_Text, tabColor);
            if (ImGui::BeginTabItem(tab.title, nullptr, ImGuiTabItemFlags_None)) {
                mActiveControllerPanelTab = tab.index;
                ImGui::PopStyleColor(1);
                (this->*tab.renderFn)(slot, snap);
                ImGui::EndTabItem();
            } else {
                ImGui::PopStyleColor(1);
            }
        }

        ImGui::EndTabBar();
    }
}

void DemoScene::RenderPanel_ControlsTab(int slot, const ControllerSnapshot& snap) {
    const uni_gamepad_t& gp = snap.controller.gamepad;
    const float effectiveDeadzone = mDontTrimDeadzone ? 0.0f : mRadialDeadzone;

    float leftX = 0.0f;
    float leftY = 0.0f;
    float rightX = 0.0f;
    float rightY = 0.0f;
    ApplyRadialDeadzone(gp.axis_x, gp.axis_y, effectiveDeadzone, &leftX, &leftY);
    ApplyRadialDeadzone(gp.axis_rx, gp.axis_ry, effectiveDeadzone, &rightX, &rightY);

    const float l2Val = NormalizeTriggerAxis(gp.brake, (gp.buttons & BUTTON_TRIGGER_L) != 0);
    const float r2Val = NormalizeTriggerAxis(gp.throttle, (gp.buttons & BUTTON_TRIGGER_R) != 0);
    const bool l1Active = (gp.buttons & BUTTON_SHOULDER_L) != 0;
    const bool r1Active = (gp.buttons & BUTTON_SHOULDER_R) != 0;
    const bool l3Active = (gp.buttons & BUTTON_THUMB_L) != 0;
    const bool r3Active = (gp.buttons & BUTTON_THUMB_R) != 0;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.13f, 0.17f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.26f, 0.30f, 0.38f, 0.75f));

    ImGui::Spacing();

    // ========================================================================
    // 1. Top Row: LT / L2 Trigger Card  |  LB/L1 & RB/R1  |  RT / R2 Trigger Card
    // ========================================================================
    if (ImGui::BeginTable("##controls_top_triggers", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("##lt_col", ImGuiTableColumnFlags_WidthStretch, 0.41f);
        ImGui::TableSetupColumn("##bumpers_col", ImGuiTableColumnFlags_WidthStretch, 0.18f);
        ImGui::TableSetupColumn("##rt_col", ImGuiTableColumnFlags_WidthStretch, 0.41f);
        ImGui::TableNextRow();

        auto renderTriggerCard = [&](const char* childId, const char* label, float normVal, int32_t rawVal) {
            if (ImGui::BeginChild(childId, ImVec2(0.0f, 74.0f), ImGuiChildFlags_Borders,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                ImGui::TextColored(normVal > 0.01f ? kTextColorCyan : kTextColorWhite, "%s", label);

                const float barWidth = std::max(40.0f, ImGui::GetContentRegionAvail().x);
                constexpr float kBarHeight = 13.0f;
                const ImVec2 barMin = ImGui::GetCursorScreenPos();
                const ImVec2 barMax(barMin.x + barWidth, barMin.y + kBarHeight);
                ImGui::Dummy(ImVec2(barWidth, kBarHeight));

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(barMin, barMax, IM_COL32(20, 24, 32, 255), 6.5f);
                if (normVal > 0.002f) {
                    const float fillW = std::max(kBarHeight, barWidth * std::clamp(normVal, 0.0f, 1.0f));
                    drawList->AddRectFilled(barMin, ImVec2(barMin.x + fillW, barMax.y), IM_COL32(55, 150, 245, 255),
                                            6.5f);
                }
                drawList->AddRect(barMin, barMax, IM_COL32(120, 135, 158, 200), 6.5f, 0, 1.2f);

                const float cStartX = ImGui::GetCursorPosX();
                const float cWidth = ImGui::GetContentRegionAvail().x;
                DrawTextCenteredInColumn(cStartX, cWidth, kTextColorGrey, "%.2f (%.0f%%)  [raw: %d]",
                                         static_cast<double>(normVal), static_cast<double>(normVal * 100.0f), rawVal);
            }
            ImGui::EndChild();
        };

        // Left Trigger (LT / L2)
        ImGui::TableNextColumn();
        renderTriggerCard("##lt_card", "LT / L2", l2Val, gp.brake);

        // Center Shoulder Bumpers (LB / L1 and RB / R1)
        ImGui::TableNextColumn();
        {
            const float colStartX = ImGui::GetCursorPosX();
            const float colWidth = ImGui::GetContentRegionAvail().x;
            constexpr float kPillW = 66.0f;
            constexpr float kPillH = 34.0f;
            constexpr float kPillGap = 10.0f;
            constexpr float kTotalW = kPillW * 2.0f + kPillGap;

            ImGui::Dummy(ImVec2(0.0f, 16.0f));
            ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - kTotalW) * 0.5f));
            const ImVec2 rowMin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(kTotalW, kPillH));

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            DrawVectorButtonBadge(drawList, rowMin, ImVec2(kPillW, kPillH), "LB / L1", nullptr, l1Active);
            DrawVectorButtonBadge(drawList, ImVec2(rowMin.x + kPillW + kPillGap, rowMin.y), ImVec2(kPillW, kPillH),
                                  "RB / R1", nullptr, r1Active);
        }

        // Right Trigger (RT / R2)
        ImGui::TableNextColumn();
        renderTriggerCard("##rt_card", "RT / R2", r2Val, gp.throttle);

        ImGui::EndTable();
    }

    ImGui::Spacing();

    // ========================================================================
    // 2. Main 3-Column Grid (Left Stick + D-Pad | Nav + Extra + Last Input + Deadzone | Action + Right Stick)
    // ========================================================================
    if (ImGui::BeginTable("##controls_main_grid", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("##left_col", ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableSetupColumn("##center_col", ImGuiTableColumnFlags_WidthStretch, 0.40f);
        ImGui::TableSetupColumn("##right_col", ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableNextRow();

        // --------------------------------------------------------------------
        // Column 1: Left Stick (L3) Card + D-Pad Card
        // --------------------------------------------------------------------
        ImGui::TableNextColumn();
        {
            if (ImGui::BeginChild("##left_stick_card", ImVec2(0.0f, 172.0f), ImGuiChildFlags_Borders,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                DrawStickWellWidget("Left Stick (L3)", gp.axis_x, gp.axis_y, leftX, leftY, effectiveDeadzone, l3Active);
            }
            ImGui::EndChild();

            ImGui::Spacing();

            if (ImGui::BeginChild("##dpad_card", ImVec2(0.0f, 162.0f), ImGuiChildFlags_Borders,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                DrawDPadCrossWidget(gp.dpad);
            }
            ImGui::EndChild();
        }

        // --------------------------------------------------------------------
        // Column 2: Navigation Buttons + Extra Buttons + Last Input + Radial Deadzone
        // --------------------------------------------------------------------
        ImGui::TableNextColumn();
        {
            // Card 2A: Navigation Buttons (Select/Share, Mode/Guide/PS, Start/Options)
            if (ImGui::BeginChild("##nav_buttons_card", ImVec2(0.0f, 80.0f), ImGuiChildFlags_Borders,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                const float colStartX = ImGui::GetCursorPosX();
                const float colWidth = ImGui::GetContentRegionAvail().x;
                DrawTextCenteredInColumn(colStartX, colWidth, kTextColorWhite, "Navigation Buttons");
                ImGui::Spacing();

                constexpr float kBtnW = 74.0f;
                constexpr float kBtnH = 38.0f;
                constexpr float kGap = 10.0f;
                constexpr float kRowW = kBtnW * 3.0f + kGap * 2.0f;

                ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - kRowW) * 0.5f));
                const ImVec2 rowMin = ImGui::GetCursorScreenPos();
                ImGui::Dummy(ImVec2(kRowW, kBtnH));

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                DrawVectorButtonBadge(drawList, rowMin, ImVec2(kBtnW, kBtnH), "Select", "Share",
                                      (gp.misc_buttons & MISC_BUTTON_SELECT) != 0);
                DrawVectorButtonBadge(drawList, ImVec2(rowMin.x + kBtnW + kGap, rowMin.y), ImVec2(kBtnW, kBtnH), "Mode",
                                      "Guide / PS", (gp.misc_buttons & MISC_BUTTON_SYSTEM) != 0);
                DrawVectorButtonBadge(drawList, ImVec2(rowMin.x + (kBtnW + kGap) * 2.0f, rowMin.y),
                                      ImVec2(kBtnW, kBtnH), "Start", "Options",
                                      (gp.misc_buttons & MISC_BUTTON_START) != 0);
            }
            ImGui::EndChild();

            ImGui::Spacing();

            // Card 2B: Extra Buttons (Capture, L3, R3, plus live hex bitmasks)
            if (ImGui::BeginChild("##extra_buttons_card", ImVec2(0.0f, 114.0f), ImGuiChildFlags_Borders,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                const float colStartX = ImGui::GetCursorPosX();
                const float colWidth = ImGui::GetContentRegionAvail().x;
                DrawTextCenteredInColumn(colStartX, colWidth, kTextColorWhite, "Extra Buttons");
                DrawTextCenteredInColumn(colStartX, colWidth, kTextColorGrey, "Auxiliary, thumb-click & bitmask keys");
                ImGui::Spacing();

                constexpr float kBtnW = 74.0f;
                constexpr float kBtnH = 36.0f;
                constexpr float kGap = 10.0f;
                constexpr float kRowW = kBtnW * 3.0f + kGap * 2.0f;

                ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - kRowW) * 0.5f));
                const ImVec2 rowMin = ImGui::GetCursorScreenPos();
                ImGui::Dummy(ImVec2(kRowW, kBtnH));

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                DrawVectorButtonBadge(drawList, rowMin, ImVec2(kBtnW, kBtnH), "Capture", "Mute / Share",
                                      (gp.misc_buttons & MISC_BUTTON_CAPTURE) != 0);
                DrawVectorButtonBadge(drawList, ImVec2(rowMin.x + kBtnW + kGap, rowMin.y), ImVec2(kBtnW, kBtnH), "L3",
                                      "Left Stick", l3Active);
                DrawVectorButtonBadge(drawList, ImVec2(rowMin.x + (kBtnW + kGap) * 2.0f, rowMin.y),
                                      ImVec2(kBtnW, kBtnH), "R3", "Right Stick", r3Active);

                ImGui::Spacing();
                DrawTextCenteredInColumn(colStartX, colWidth, kTextColorGrey,
                                         "Buttons: 0x%04X   DPad: 0x%02X   Misc: 0x%02X", gp.buttons, gp.dpad,
                                         gp.misc_buttons);
            }
            ImGui::EndChild();

            ImGui::Spacing();

            // Card 2C: LAST DETECTED INPUT
            if (ImGui::BeginChild("##last_detected_card", ImVec2(0.0f, 62.0f), ImGuiChildFlags_Borders,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                ImGui::TextColored(kTextColorGrey, "LAST DETECTED INPUT");
                ImGui::Indent(8.0f);
                ImGui::TextColored(kTextColorCyan, "%s", mLastDetectedInput[slot]);
                ImGui::Unindent(8.0f);
            }
            ImGui::EndChild();

            ImGui::Spacing();

            // Card 2D: Radial Deadzone Slider (0% .. 35%)
            if (ImGui::BeginChild("##radial_deadzone_card", ImVec2(0.0f, 66.0f), ImGuiChildFlags_Borders,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                ImGui::TextColored(kTextColorWhite, "Radial Deadzone");
                ImGui::SameLine();
                char pctBuf[32];
                std::snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", static_cast<double>(effectiveDeadzone * 100.0f));
                const float pctW = ImGui::CalcTextSize(pctBuf).x;
                const float availW = ImGui::GetContentRegionAvail().x;
                if (availW > pctW) {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availW - pctW);
                }
                ImGui::TextColored(kTextColorCyan, "%s", pctBuf);

                float deadzonePct = effectiveDeadzone * 100.0f;
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::SliderFloat("##radial_deadzone_slider", &deadzonePct, 0.0f, 35.0f, "")) {
                    mRadialDeadzone = std::clamp(deadzonePct / 100.0f, 0.0f, 0.35f);
                    mDontTrimDeadzone = (mRadialDeadzone <= 0.001f);
                }
            }
            ImGui::EndChild();
        }

        // --------------------------------------------------------------------
        // Column 3: Action Buttons Card + Right Stick (R3) Card
        // --------------------------------------------------------------------
        ImGui::TableNextColumn();
        {
            if (ImGui::BeginChild("##action_buttons_card", ImVec2(0.0f, 162.0f), ImGuiChildFlags_Borders,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                DrawActionButtonsDiamondWidget(gp.buttons, snap.layout);
            }
            ImGui::EndChild();

            ImGui::Spacing();

            if (ImGui::BeginChild("##right_stick_card", ImVec2(0.0f, 172.0f), ImGuiChildFlags_Borders,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                DrawStickWellWidget("Right Stick (R3)", gp.axis_rx, gp.axis_ry, rightX, rightY, effectiveDeadzone,
                                    r3Active);
            }
            ImGui::EndChild();
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(1);
}

void DemoScene::RenderPanel_InfoTab(int slot, const ControllerSnapshot& snap) {
    ImGui::Spacing();
    ImGui::TextColored(kTextColorCyan, "Hardware & Bluetooth Link Diagnostics");
    ImGui::Separator();

    if (ImGui::BeginTable("##infotable", 2,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 240.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        auto addRow = [](const char* label, const char* fmt, ...) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(kTextColorGrey, "%s", label);
            ImGui::TableNextColumn();
            va_list args;
            va_start(args, fmt);
            ImGui::TextV(fmt, args);
            va_end(args);
        };

        addRow("Slot Index / Seat:", "Slot #%d (GAMEPAD_SEAT_%c = 0x%02X)", slot + 1, 'A' + slot, 1 << slot);
        addRow("Bluetooth Device Name:", "%s", snap.name[0] != '\0' ? snap.name : "(Unnamed Device)");
        addRow("Bluepad32 Model Name:", "%s (type=%d)", snap.model_name, static_cast<int>(snap.controller_type));
        addRow("Controller Subtype:", "%s (%d)", SubtypeToString(snap.controller_subtype),
               static_cast<int>(snap.controller_subtype));
        addRow("Vendor ID / Product ID:", "VID: 0x%04X  |  PID: 0x%04X", snap.vendor_id, snap.product_id);
        addRow("Bluetooth MAC Address:", "%s", bd_addr_to_str(snap.btaddr));
        addRow("Face Button Layout:", "%s", LayoutTypeToString(snap.layout));
        addRow("RSSI (Link Quality):", "%u (signed: %d dBm)", snap.rssi,
               static_cast<int>(static_cast<int8_t>(snap.rssi)));

        if (snap.report_delta_ms > 0) {
            const float hz = 1000.0f / static_cast<float>(snap.report_delta_ms);
            addRow("Input Report Interval:", "%u ms (~%.1f Hz)", snap.report_delta_ms, static_cast<double>(hz));
        } else {
            addRow("Input Report Interval:", "%u ms (awaiting consecutive reports)", snap.report_delta_ms);
        }

        // Battery status row with progress bar
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(kTextColorGrey, "Battery Level:");
        ImGui::TableNextColumn();
        const uint8_t battery = snap.controller.battery;
        if (battery == UNI_CONTROLLER_BATTERY_NOT_AVAILABLE || battery == 255) {
            ImGui::TextColored(kTextColorGrey, "Not available (raw=%u)", battery);
        } else {
            const float frac = std::clamp(static_cast<float>(battery) / 254.0f, 0.0f, 1.0f);
            char overlay[64];
            std::snprintf(overlay, sizeof(overlay), "%.0f%% (%u / 254)", static_cast<double>(frac * 100.0f), battery);
            ImGui::SetNextItemWidth(220.0f);
            ImGui::ProgressBar(frac, ImVec2(220.0f, 0.0f), overlay);
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextColored(kTextColorCyan, "Supported Controller Capabilities");
    ImGui::Separator();

    auto renderBadge = [](const char* label, bool supported) {
        ImGui::TextColored(supported ? kTextColorGreen : kTextColorGrey, "[%s] %s", supported ? "YES" : "NO ", label);
    };

    renderBadge("Dual-Motor Rumble (play_dual_rumble)", snap.has_rumble);
    renderBadge("Player Indicator LEDs (set_player_leds)", snap.has_player_leds);
    renderBadge("RGB Lightbar (set_lightbar_color)", snap.has_rgb_led);
    renderBadge("Switch Pro / Joy-Con Brightness LED", snap.has_brightness_led);
    renderBadge("6-Axis IMU (Accelerometer + Gyroscope)", snap.has_imu);
}

void DemoScene::RenderPanel_VibrationTab(int slot, const ControllerSnapshot& snap) {
    ImGui::Spacing();
    ImGui::TextColored(kTextColorCyan, "Dual-Motor Force Feedback (Rumble)");
    ImGui::Separator();

    if (!snap.has_rumble) {
        ImGui::Spacing();
        ImGui::TextColored(kTextColorYellow, "No vibration / rumble support reported for %s.", snap.model_name);
        return;
    }

    ImGui::TextColored(kTextColorGreen, "Dual-motor rumble supported (`play_dual_rumble`)");

    VibrationParameters("Start Delay (ms):", "delay", kVibrationDelayMin, kVibrationDelayMax, kVibrationDelayStep,
                        &mRumbleDelayMs[slot]);
    VibrationParameters("Duration (ms):", "dur", kVibrationDurationMin, kVibrationDurationMax, kVibrationDurationStep,
                        &mRumbleDurationMs[slot]);
    VibrationParameters("Weak Motor Intensity:", "weak", kVibrationIntensityMin, kVibrationIntensityMax,
                        kVibrationIntensityStep, &mRumbleWeakIntensity[slot]);
    VibrationParameters("Strong Motor Intensity:", "strong", kVibrationIntensityMin, kVibrationIntensityMax,
                        kVibrationIntensityStep, &mRumbleStrongIntensity[slot]);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const uint16_t delayMs = static_cast<uint16_t>(std::clamp(mRumbleDelayMs[slot], 0.0f, 1000.0f));
    const uint16_t durationMs = static_cast<uint16_t>(std::clamp(mRumbleDurationMs[slot], 0.0f, 2000.0f));
    const uint8_t weakU8 =
        static_cast<uint8_t>(std::clamp(std::round(mRumbleWeakIntensity[slot] * 255.0f), 0.0f, 255.0f));
    const uint8_t strongU8 =
        static_cast<uint8_t>(std::clamp(std::round(mRumbleStrongIntensity[slot] * 255.0f), 0.0f, 255.0f));

    if (ImGui::Button("  Vibrate  ")) {
        posix_imgui_request_rumble(slot, delayMs, durationMs, weakU8, strongU8);
    }
    ImGui::SameLine(0.0f, 16.0f);
    if (ImGui::Button("  Stop Rumble (0, 0)  ")) {
        posix_imgui_request_rumble(slot, 0, 0, 0, 0);
    }

    ImGui::Spacing();
    ImGui::TextColored(kTextColorGrey, "Quick Presets:");
    ImGui::SameLine();
    if (ImGui::Button("Light Tap (150ms)")) {
        mRumbleDelayMs[slot] = 0.0f;
        mRumbleDurationMs[slot] = 150.0f;
        mRumbleWeakIntensity[slot] = 0.35f;
        mRumbleStrongIntensity[slot] = 0.0f;
        posix_imgui_request_rumble(slot, 0, 150, 90, 0);
    }
    ImGui::SameLine();
    if (ImGui::Button("Heavy Pulse (600ms)")) {
        mRumbleDelayMs[slot] = 0.0f;
        mRumbleDurationMs[slot] = 600.0f;
        mRumbleWeakIntensity[slot] = 0.80f;
        mRumbleStrongIntensity[slot] = 1.00f;
        posix_imgui_request_rumble(slot, 0, 600, 204, 255);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delayed Pulse (+250ms, 300ms)")) {
        mRumbleDelayMs[slot] = 250.0f;
        mRumbleDurationMs[slot] = 300.0f;
        mRumbleWeakIntensity[slot] = 0.70f;
        mRumbleStrongIntensity[slot] = 0.70f;
        posix_imgui_request_rumble(slot, 250, 300, 178, 178);
    }
}

void DemoScene::RenderPanel_MotionTab(int slot, const ControllerSnapshot& snap) {
    ImGui::Spacing();
    ImGui::TextColored(kTextColorCyan, "6-Axis Inertial Measurement Unit (Accelerometer & Gyroscope)");
    ImGui::Separator();

    if (!snap.has_imu) {
        ImGui::TextColored(kTextColorYellow, "Note: %s does not advertise native IMU support (showing raw values).",
                           snap.model_name);
    } else {
        ImGui::TextColored(kTextColorGreen, "IMU Active — Report Delta: %u ms", snap.report_delta_ms);
    }

    const uni_gamepad_t& gp = snap.controller.gamepad;
    const float accelScale = AccelCountsPerG(snap.vendor_id);
    const float gyroScale = GyroCountsPerDegPerSec(snap.vendor_id);

    float accelG[3] = {};
    float accelMps2[3] = {};
    float gyroDegS[3] = {};
    float gyroRadS[3] = {};
    for (int axis = 0; axis < 3; ++axis) {
        accelG[axis] = static_cast<float>(gp.accel[axis]) / accelScale;
        accelMps2[axis] = accelG[axis] * kGravityMps2;
        gyroDegS[axis] = static_cast<float>(gp.gyro[axis]) / gyroScale;
        gyroRadS[axis] = gyroDegS[axis] * kDegToRad;
    }

    ImGui::Spacing();
    if (ImGui::BeginTable(
            "##imu_circular_cards", 2,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("##accel_card", ImGuiTableColumnFlags_WidthStretch, 0.44f);
        ImGui::TableSetupColumn("##gyro_card", ImGuiTableColumnFlags_WidthStretch, 0.56f);
        ImGui::TableNextRow();

        // ====================================================================
        // Left Column: Accelerometer 2D Circular Bullseye Radar Widget
        // ====================================================================
        ImGui::TableNextColumn();
        {
            ImGui::TextColored(kTextColorWhite, "Accelerometer");
            ImGui::TextColored(kTextColorGrey, "Tilt and linear movement (m/s\xC2\xB2)");
            ImGui::Spacing();

            const float colStartX = ImGui::GetCursorPosX();
            const float colWidth = ImGui::GetContentRegionAvail().x;
            constexpr float kBullseyeRadius = 56.0f;
            constexpr float kBullseyeDiameter = kBullseyeRadius * 2.0f;

            ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - kBullseyeDiameter) * 0.5f));
            const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(kBullseyeDiameter, kBullseyeDiameter));

            const ImVec2 center(canvasMin.x + kBullseyeRadius, canvasMin.y + kBullseyeRadius);
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // Subtle dark circular backdrop
            drawList->AddCircleFilled(center, kBullseyeRadius, IM_COL32(22, 26, 34, 220), 64);

            // Three concentric target rings (r/3, 2r/3, r)
            drawList->AddCircle(center, kBullseyeRadius * (1.0f / 3.0f), IM_COL32(95, 110, 130, 130), 48, 1.2f);
            drawList->AddCircle(center, kBullseyeRadius * (2.0f / 3.0f), IM_COL32(95, 110, 130, 150), 64, 1.2f);
            drawList->AddCircle(center, kBullseyeRadius, IM_COL32(150, 170, 195, 220), 64, 1.8f);

            // Crosshair lines through center
            drawList->AddLine(ImVec2(center.x - kBullseyeRadius, center.y),
                              ImVec2(center.x + kBullseyeRadius, center.y), IM_COL32(95, 110, 130, 150), 1.0f);
            drawList->AddLine(ImVec2(center.x, center.y - kBullseyeRadius),
                              ImVec2(center.x, center.y + kBullseyeRadius), IM_COL32(95, 110, 130, 150), 1.0f);

            // Map in-plane tilt axes to 2D radar dot:
            // Sony DS4/DualSense (0x054c) uses Y as vertical gravity (+1g at rest) and X/Z as horizontal plane;
            // Nintendo Switch / other controllers use Z as vertical gravity and X/Y as horizontal plane.
            float nx = accelG[0];
            float ny = (snap.vendor_id == 0x054c) ? accelG[2] : accelG[1];
            const float mag = std::hypot(nx, ny);
            if (mag > 1.0f) {
                nx /= mag;
                ny /= mag;
            }

            const float maxDotOffset = kBullseyeRadius - 7.0f;
            const ImVec2 dotPos(center.x + nx * maxDotOffset, center.y - ny * maxDotOffset);

            drawList->AddLine(center, dotPos, IM_COL32(80, 200, 255, 110), 1.5f);
            drawList->AddCircleFilled(dotPos, 8.0f, IM_COL32(80, 215, 255, 70), 24);
            drawList->AddCircleFilled(dotPos, 5.5f, IM_COL32(90, 220, 255, 255), 24);
            drawList->AddCircle(dotPos, 5.5f, IM_COL32(230, 250, 255, 220), 24, 1.2f);

            ImGui::Spacing();
            if (ImGui::BeginTable("##accel_xyz_readouts", 3, ImGuiTableFlags_SizingStretchSame)) {
                const char* kAxisLabels[3] = {"X", "Y", "Z"};
                ImGui::TableNextRow();
                for (int axis = 0; axis < 3; ++axis) {
                    ImGui::TableNextColumn();
                    const float subStartX = ImGui::GetCursorPosX();
                    const float subWidth = ImGui::GetContentRegionAvail().x;
                    DrawTextCenteredInColumn(subStartX, subWidth, kTextColorGrey, "%s", kAxisLabels[axis]);
                    DrawTextCenteredInColumn(subStartX, subWidth, kTextColorWhite, "%+0.2f",
                                             static_cast<double>(accelMps2[axis]));
                    DrawTextCenteredInColumn(subStartX, subWidth, kTextColorGrey, "(%d)", gp.accel[axis]);
                }
                ImGui::EndTable();
            }
        }

        // ====================================================================
        // Right Column: Gyroscope 3x Circular Needle Dials (X, Y, Z)
        // ====================================================================
        ImGui::TableNextColumn();
        {
            ImGui::TextColored(kTextColorWhite, "Gyroscope");
            ImGui::SameLine();
            const float resetBtnWidth = 68.0f;
            const float availRight = ImGui::GetContentRegionAvail().x;
            if (availRight > resetBtnWidth) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availRight - resetBtnWidth);
            }
            if (ImGui::SmallButton(" Reset ")) {
                std::memset(mGyroAngleDeg[slot], 0, sizeof(mGyroAngleDeg[slot]));
            }
            ImGui::TextColored(kTextColorGrey, "Rotation angle & speed (rad/s)");
            ImGui::Spacing();

            if (ImGui::BeginTable("##gyro_dials_table", 3, ImGuiTableFlags_SizingStretchSame)) {
                const char* kAxisLabels[3] = {"X", "Y", "Z"};
                ImGui::TableNextRow();
                for (int axis = 0; axis < 3; ++axis) {
                    ImGui::TableNextColumn();
                    const float subStartX = ImGui::GetCursorPosX();
                    const float subWidth = ImGui::GetContentRegionAvail().x;
                    constexpr float kDialRadius = 46.0f;
                    constexpr float kDialDiameter = kDialRadius * 2.0f;

                    ImGui::SetCursorPosX(subStartX + std::max(0.0f, (subWidth - kDialDiameter) * 0.5f));
                    const ImVec2 dialMin = ImGui::GetCursorScreenPos();
                    ImGui::Dummy(ImVec2(kDialDiameter, kDialDiameter));

                    const ImVec2 dialCenter(dialMin.x + kDialRadius, dialMin.y + kDialRadius);
                    ImDrawList* drawList = ImGui::GetWindowDrawList();

                    // Dial background and outer ring
                    drawList->AddCircleFilled(dialCenter, kDialRadius, IM_COL32(22, 26, 34, 220), 64);
                    drawList->AddCircle(dialCenter, kDialRadius, IM_COL32(150, 170, 195, 220), 64, 1.8f);

                    // Minor 3/6/9-o'clock reference ticks
                    drawList->AddLine(ImVec2(dialCenter.x + kDialRadius - 5.0f, dialCenter.y),
                                      ImVec2(dialCenter.x + kDialRadius, dialCenter.y), IM_COL32(95, 110, 130, 160),
                                      1.2f);
                    drawList->AddLine(ImVec2(dialCenter.x - kDialRadius, dialCenter.y),
                                      ImVec2(dialCenter.x - kDialRadius + 5.0f, dialCenter.y),
                                      IM_COL32(95, 110, 130, 160), 1.2f);
                    drawList->AddLine(ImVec2(dialCenter.x, dialCenter.y + kDialRadius - 5.0f),
                                      ImVec2(dialCenter.x, dialCenter.y + kDialRadius), IM_COL32(95, 110, 130, 160),
                                      1.2f);

                    // Prominent 12-o'clock zero-degree reference tick
                    drawList->AddLine(ImVec2(dialCenter.x, dialCenter.y - kDialRadius),
                                      ImVec2(dialCenter.x, dialCenter.y - kDialRadius + 8.0f),
                                      IM_COL32(220, 230, 245, 240), 2.0f);

                    // Rotating needle showing integrated rotation angle in degrees
                    const float angleDeg = mGyroAngleDeg[slot][axis];
                    const float angleRad = angleDeg * kDegToRad;
                    const float needleLen = kDialRadius - 8.0f;
                    const ImVec2 needleTip(dialCenter.x + std::sin(angleRad) * needleLen,
                                           dialCenter.y - std::cos(angleRad) * needleLen);

                    drawList->AddLine(dialCenter, needleTip, IM_COL32(90, 220, 255, 255), 2.4f);
                    drawList->AddCircleFilled(dialCenter, 4.0f, IM_COL32(220, 230, 245, 255), 16);

                    ImGui::Spacing();
                    DrawTextCenteredInColumn(subStartX, subWidth, kTextColorWhite, "%.0f\xC2\xB0",
                                             static_cast<double>(angleDeg));
                    DrawTextCenteredInColumn(subStartX, subWidth, kTextColorWhite, "%s  %+0.2f", kAxisLabels[axis],
                                             static_cast<double>(gyroRadS[axis]));
                    DrawTextCenteredInColumn(subStartX, subWidth, kTextColorGrey, "(%d)", gp.gyro[axis]);
                }
                ImGui::EndTable();
            }
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::Button(mImuPlotPaused ? " Resume Plot " : " Pause Plot ")) {
        mImuPlotPaused = !mImuPlotPaused;
    }
    ImGui::SameLine();
    if (ImGui::Button(" Clear History ")) {
        ClearImuHistory(slot);
    }

    const int histCount = static_cast<int>(kImuHistoryLen);
    const int histOffset = static_cast<int>(mImuHistoryOffset[slot]);

    ImGui::Spacing();
    ImGui::TextColored(kTextColorCyan, "Live History Plots (240 samples)");
    if (ImGui::BeginTable("##imuplots", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Accelerometer (X / Y / Z)");
        ImGui::PlotLines("Accel X", mAccelHistory[slot][0], histCount, histOffset, nullptr, -8192.0f, 8192.0f,
                         ImVec2(0.0f, 55.0f));
        ImGui::PlotLines("Accel Y", mAccelHistory[slot][1], histCount, histOffset, nullptr, -8192.0f, 8192.0f,
                         ImVec2(0.0f, 55.0f));
        ImGui::PlotLines("Accel Z", mAccelHistory[slot][2], histCount, histOffset, nullptr, -8192.0f, 8192.0f,
                         ImVec2(0.0f, 55.0f));

        ImGui::TableNextColumn();
        ImGui::Text("Gyroscope (X / Y / Z)");
        ImGui::PlotLines("Gyro X", mGyroHistory[slot][0], histCount, histOffset, nullptr, -2048.0f, 2048.0f,
                         ImVec2(0.0f, 55.0f));
        ImGui::PlotLines("Gyro Y", mGyroHistory[slot][1], histCount, histOffset, nullptr, -2048.0f, 2048.0f,
                         ImVec2(0.0f, 55.0f));
        ImGui::PlotLines("Gyro Z", mGyroHistory[slot][2], histCount, histOffset, nullptr, -2048.0f, 2048.0f,
                         ImVec2(0.0f, 55.0f));
        ImGui::EndTable();
    }
}

void DemoScene::RenderPanel_LightsTab(int slot, const ControllerSnapshot& snap) {
    ImGui::Spacing();

    // ------------------------------------------------------------------------
    // 1. Player Indicator LEDs (set_player_leds)
    // ------------------------------------------------------------------------
    ImGui::TextColored(kTextColorCyan, "1. Player Indicator Lights (`set_player_leds`)");
    ImGui::Separator();
    if (snap.has_player_leds) {
        ImGui::SetNextItemWidth(180.0f);
        ImGui::SliderInt("Player Index (1..4)", &mPlayerLedIndex[slot], 1, 4);
        ImGui::SameLine();
        if (ImGui::Button("Set Player Index Light")) {
            const uint8_t mask = static_cast<uint8_t>(1u << (mPlayerLedIndex[slot] - 1));
            for (int b = 0; b < 4; ++b) {
                mPlayerLedBits[slot][b] = ((mask & (1u << b)) != 0);
            }
            posix_imgui_request_player_leds(slot, mask);
        }

        ImGui::Spacing();
        ImGui::Text("Raw 4-Bit LED Mask:");
        ImGui::SameLine();
        uint8_t rawMask = 0;
        for (int b = 0; b < 4; ++b) {
            char cbLabel[32];
            std::snprintf(cbLabel, sizeof(cbLabel), "LED %d (Bit %d)", b + 1, b);
            ImGui::Checkbox(cbLabel, &mPlayerLedBits[slot][b]);
            if (mPlayerLedBits[slot][b]) {
                rawMask |= static_cast<uint8_t>(1u << b);
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("Set Raw LED Bitmask")) {
            posix_imgui_request_player_leds(slot, rawMask);
        }

        ImGui::TextColored(kTextColorGrey, "Quick Presets:");
        ImGui::SameLine();
        for (int p = 1; p <= 4; ++p) {
            char btnLabel[24];
            std::snprintf(btnLabel, sizeof(btnLabel), "Seat #%d", p);
            if (ImGui::Button(btnLabel)) {
                mPlayerLedIndex[slot] = p;
                const uint8_t mask = static_cast<uint8_t>(1u << (p - 1));
                for (int b = 0; b < 4; ++b) {
                    mPlayerLedBits[slot][b] = (b == (p - 1));
                }
                posix_imgui_request_player_leds(slot, mask);
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("All Off (0x0)")) {
            for (int b = 0; b < 4; ++b) {
                mPlayerLedBits[slot][b] = false;
            }
            posix_imgui_request_player_leds(slot, 0x00);
        }
    } else {
        ImGui::TextColored(kTextColorGrey, "No player index light present on %s.", snap.model_name);
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // ------------------------------------------------------------------------
    // 2. RGB Lightbar (set_lightbar_color)
    // ------------------------------------------------------------------------
    ImGui::TextColored(kTextColorCyan, "2. RGB Lightbar (`set_lightbar_color`)");
    ImGui::Separator();
    if (snap.has_rgb_led) {
        ImGui::SetNextItemWidth(260.0f);
        const bool colorChanged = ImGui::ColorEdit3("LightColor", mRgbColor[slot]);
        ImGui::SameLine();
        if (ImGui::Button("Set Light Color") || (colorChanged && mRgbLiveUpdate[slot])) {
            const uint8_t r = static_cast<uint8_t>(std::clamp(std::round(mRgbColor[slot][0] * 255.0f), 0.0f, 255.0f));
            const uint8_t g = static_cast<uint8_t>(std::clamp(std::round(mRgbColor[slot][1] * 255.0f), 0.0f, 255.0f));
            const uint8_t b = static_cast<uint8_t>(std::clamp(std::round(mRgbColor[slot][2] * 255.0f), 0.0f, 255.0f));
            posix_imgui_request_lightbar_color(slot, r, g, b);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Live update on drag", &mRgbLiveUpdate[slot]);

        struct ColorSwatch {
            const char* name;
            float r;
            float g;
            float b;
        };
        const ColorSwatch kSwatches[] = {
            {"PS Blue", 0.00f, 0.25f, 1.00f}, {"Red", 1.00f, 0.00f, 0.00f},   {"Green", 0.00f, 1.00f, 0.00f},
            {"Amber", 1.00f, 0.60f, 0.00f},   {"White", 1.00f, 1.00f, 1.00f}, {"Off", 0.00f, 0.00f, 0.00f},
        };
        ImGui::TextColored(kTextColorGrey, "Swatches:");
        ImGui::SameLine();
        for (size_t i = 0; i < sizeof(kSwatches) / sizeof(kSwatches[0]); ++i) {
            if (i > 0) {
                ImGui::SameLine();
            }
            if (ImGui::Button(kSwatches[i].name)) {
                mRgbColor[slot][0] = kSwatches[i].r;
                mRgbColor[slot][1] = kSwatches[i].g;
                mRgbColor[slot][2] = kSwatches[i].b;
                posix_imgui_request_lightbar_color(slot, static_cast<uint8_t>(std::round(kSwatches[i].r * 255.0f)),
                                                   static_cast<uint8_t>(std::round(kSwatches[i].g * 255.0f)),
                                                   static_cast<uint8_t>(std::round(kSwatches[i].b * 255.0f)));
            }
        }
    } else {
        ImGui::TextColored(kTextColorGrey, "No RGB light present on %s.", snap.model_name);
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // ------------------------------------------------------------------------
    // 3. Brightness Light (Switch Pro Controller / Joy-Con Right UI Placeholder)
    // ------------------------------------------------------------------------
    ImGui::TextColored(kTextColorCyan, "3. Brightness Light (Nintendo Switch Pro Controller / Joy-Con Right)");
    ImGui::Separator();
    if (!snap.has_brightness_led) {
        ImGui::TextColored(kTextColorGrey, "No brightness light present on %s.", snap.model_name);
    }
    ImGui::BeginDisabled(true);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::SliderInt("LED Brightness (%)", &mBrightnessPercent[slot], 0, 100, "%d%%");
    ImGui::SameLine();
    ImGui::Button("Set Brightness Light");
    ImGui::EndDisabled();
    ImGui::TextColored(kTextColorYellow,
                       "Note: UI placeholder — uni_hid_device_t does not yet expose a brightness LED setter in "
                       "libbluepad32.");
}
