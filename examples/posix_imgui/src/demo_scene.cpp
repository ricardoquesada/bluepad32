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
 *   3. DPI- & Font-Scale-Aware Card Layout (`RenderPanel_ControlsTab`, `RenderPanel_VibrationTab`,
 *      `RenderPanel_MotionTab`):
 *      All child cards use `ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY` with
 *      `ImVec2(0.0f, 0.0f)` and scale custom `ImDrawList` vector geometry proportionally via
 *      `UiScale() = ImGui::GetFontSize() / 13.0f` so widgets never clip across `0.50x..4.00x`.
 *   4. Packet-Driven IMU Ring Buffer (`UpdateImuHistory`):
 *      Advances the 240-sample circular buffers (`accel_history`, `gyro_history`) only
 *      when `snap.last_report_timestamp_us` changes, ensuring plots reflect genuine
 *      Bluetooth HID input reports rather than 60 Hz frame duplicates.
 */

#include "demo_scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <span>

namespace {

/// Display labels for the 4 top-level controller seat tabs (`#1..#4`).
constexpr std::array<const char*, kMaxControllers> kControllerTabNames = {
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

/**
 * @brief Returns the active UI scale multiplier relative to Dear ImGui's 13px baseline font.
 *
 * Why `ImGui::GetFontSize() / 13.0f`:
 *   Dear ImGui's default ProggyClean font measures 13px at `1.0x` scale. Dividing the
 *   current effective font size (which incorporates both monitor DPI `style.FontScaleDpi`
 *   and user preference `style.FontScaleMain`) by `13.0f` yields a single proportional
 *   multiplier so all custom `ImDrawList` vector geometry and fixed column widths scale
 *   cohesively across `0.50x..4.00x`.
 */
[[nodiscard]] float UiScale() noexcept {
    return ImGui::GetFontSize() / 13.0f;
}

/**
 * @brief Renders `printf`-formatted colored text right-aligned within the current content region.
 *
 * Note: `IM_FMTARGS(2)` is placed on the forward declaration because Clang/GCC require
 * `__attribute__((format(printf, ...)))` on declarations rather than function definitions.
 */
void DrawRightAlignedText(const ImVec4& color, const char* fmt, ...) IM_FMTARGS(2);
void DrawRightAlignedText(const ImVec4& color, const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    const float textW = ImGui::CalcTextSize(buf).x;
    const float availW = ImGui::GetContentRegionAvail().x;
    if (availW > textW) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availW - textW);
    }
    ImGui::TextColored(color, "%s", buf);
}

/**
 * @brief Renders a custom `ImDrawList` pill toggle switch scaled by `UiScale()`.
 *
 * @param strId     Unique Dear ImGui widget ID string.
 * @param[in,out] v Pointer to the boolean state toggled when clicked.
 * @return True on the frame the switch is clicked; false otherwise.
 */
bool DrawToggleSwitch(const char* strId, bool* v) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float s = UiScale();
    const float height = 26.0f * s;
    const float width = 48.0f * s;
    const float radius = height * 0.5f;

    ImGui::InvisibleButton(strId, ImVec2(width, height));
    const bool clicked = ImGui::IsItemClicked();
    if (clicked) {
        *v = !*v;
    }
    const bool hovered = ImGui::IsItemHovered();

    const ImU32 bgCol = *v ? (hovered ? IM_COL32(55, 165, 255, 255) : IM_COL32(35, 145, 240, 255))
                           : (hovered ? IM_COL32(75, 82, 96, 255) : IM_COL32(52, 58, 70, 255));
    const ImU32 borderCol = *v ? IM_COL32(110, 205, 255, 255) : IM_COL32(110, 118, 135, 255);
    const ImU32 knobCol = *v ? IM_COL32(255, 255, 255, 255) : IM_COL32(190, 196, 210, 255);

    const ImVec2 maxPos = p + ImVec2(width, height);
    drawList->AddRectFilled(p, maxPos, bgCol, radius);
    drawList->AddRect(p, maxPos, borderCol, radius, 0, 1.5f * s);

    const float knobX = *v ? (p.x + width - radius) : (p.x + radius);
    drawList->AddCircleFilled(ImVec2(knobX, p.y + radius), std::max(1.0f, radius - 3.5f * s), knobCol, 24);
    return clicked;
}

/// Returns a human-readable description of a Bluepad32 controller subtype.
[[nodiscard]] const char* SubtypeToString(uni_controller_subtype_t subtype) noexcept {
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

// Conversion constants between Bluepad32's canonical SI IMU telemetry (`m/s^2`, `rad/s`)
// and secondary UI display units (`g`, `deg/s`, and integrated dial angles in degrees).
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
constexpr float kGravityMps2 = UNI_STANDARD_GRAVITY;

/// Renders `printf`-formatted colored text horizontally centered within `[colStartX, colStartX + colWidth]`.
void DrawTextCenteredInColumn(float colStartX, float colWidth, const ImVec4& color, const char* fmt, ...) IM_FMTARGS(4);
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

/**
 * @brief Normalizes raw Bluepad32 stick axes (`[-512, +511]`) into `[-1.0, +1.0]` and applies
 *        a radial center deadzone.
 *
 * @param rawX     Raw horizontal axis value in `[-512, +511]`.
 * @param rawY     Raw vertical axis value in `[-512, +511]`.
 * @param deadzone Radial deadzone threshold in `[0.0, 0.35]`.
 * @return Normalized `(x, y)` deflection vector, or `(0.0f, 0.0f)` if inside `deadzone`.
 */
[[nodiscard]] ImVec2 ApplyRadialDeadzone(int32_t rawX, int32_t rawY, float deadzone) noexcept {
    const float nx = std::clamp(static_cast<float>(rawX) / (rawX < 0 ? 512.0f : 511.0f), -1.0f, 1.0f);
    const float ny = std::clamp(static_cast<float>(rawY) / (rawY < 0 ? 512.0f : 511.0f), -1.0f, 1.0f);
    const float mag = std::hypot(nx, ny);
    if (deadzone > 0.0f && mag < deadzone) {
        return ImVec2(0.0f, 0.0f);
    }
    return ImVec2(nx, ny);
}

/**
 * @brief Normalizes a raw Bluepad32 trigger axis (`[0, 1023]`) to `[0.0, 1.0]`, falling back
 *        to digital trigger button state (`1.0f` when pressed) if the analog axis is zero.
 */
[[nodiscard]] float NormalizeTriggerAxis(int32_t raw_trigger, bool digital_fallback) noexcept {
    if (raw_trigger > 0) {
        return std::clamp(static_cast<float>(raw_trigger) / 1023.0f, 0.0f, 1.0f);
    }
    return digital_fallback ? 1.0f : 0.0f;
}

/// Draws a rounded vector pill badge for shoulder bumpers, navigation buttons, and extra buttons.
void DrawVectorButtonBadge(ImDrawList* drawList,
                           ImVec2 minPos,
                           ImVec2 size,
                           const char* primaryText,
                           const char* subText,
                           bool active) {
    const float s = UiScale();
    const ImVec2 maxPos = minPos + size;
    const ImU32 fillCol = active ? IM_COL32(45, 130, 225, 255) : IM_COL32(36, 42, 54, 255);
    const ImU32 borderCol = active ? IM_COL32(120, 220, 255, 255) : IM_COL32(120, 135, 158, 200);
    const float borderThick = (active ? 2.0f : 1.3f) * s;
    const float rounding = 6.0f * s;

    drawList->AddRectFilled(minPos, maxPos, fillCol, rounding);
    drawList->AddRect(minPos, maxPos, borderCol, rounding, 0, borderThick);

    if (subText == nullptr) {
        const ImVec2 pSz = ImGui::CalcTextSize(primaryText);
        const ImVec2 pPos = minPos + (size - pSz) * 0.5f;
        drawList->AddText(pPos, IM_COL32(255, 255, 255, 255), primaryText);
    } else {
        const ImVec2 pSz = ImGui::CalcTextSize(primaryText);
        const ImVec2 sSz = ImGui::CalcTextSize(subText);
        const float lineGap = 1.0f * s;
        const float totalH = pSz.y + sSz.y + lineGap;
        const float startY = minPos.y + (size.y - totalH) * 0.5f;
        drawList->AddText(ImVec2(minPos.x + (size.x - pSz.x) * 0.5f, startY), IM_COL32(255, 255, 255, 255),
                          primaryText);
        drawList->AddText(ImVec2(minPos.x + (size.x - sSz.x) * 0.5f, startY + pSz.y + lineGap),
                          active ? IM_COL32(235, 248, 255, 255) : IM_COL32(150, 165, 185, 230), subText);
    }
}

/// Draws a circular analog thumbstick gate, deadzone ring, crosshairs, and deflection puck.
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

    const float s = UiScale();
    const float outerRadius = 46.0f * s;
    const float innerRadius = 40.0f * s;
    const float diameter = outerRadius * 2.0f;

    ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - diameter) * 0.5f));
    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(diameter, diameter));

    const ImVec2 center = canvasMin + ImVec2(outerRadius, outerRadius);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Circular well backdrop and dual concentric boundary rings
    drawList->AddCircleFilled(center, outerRadius, IM_COL32(20, 24, 32, 230), 64);
    drawList->AddCircle(center, outerRadius, thumbPressed ? IM_COL32(90, 225, 255, 255) : IM_COL32(140, 155, 180, 200),
                        64, (thumbPressed ? 2.4f : 1.5f) * s);
    drawList->AddCircle(center, innerRadius, IM_COL32(105, 120, 145, 170), 64, 1.2f * s);

    if (deadzone > 0.005f) {
        drawList->AddCircle(center, innerRadius * deadzone, IM_COL32(90, 105, 125, 100), 48, 1.0f * s);
    }

    // Crosshairs
    drawList->AddLine(center - ImVec2(innerRadius, 0.0f), center + ImVec2(innerRadius, 0.0f),
                      IM_COL32(90, 105, 125, 140), 1.0f * s);
    drawList->AddLine(center - ImVec2(0.0f, innerRadius), center + ImVec2(0.0f, innerRadius),
                      IM_COL32(90, 105, 125, 140), 1.0f * s);

    // Clamp visual puck position to the circular gate
    float clampedX = normX;
    float clampedY = normY;
    const float mag = std::hypot(clampedX, clampedY);
    if (mag > 1.0f) {
        clampedX /= mag;
        clampedY /= mag;
    }

    const float maxTravel = innerRadius - 7.0f * s;
    const ImVec2 puckPos = center + ImVec2(clampedX, clampedY) * maxTravel;

    drawList->AddLine(center, puckPos, IM_COL32(70, 160, 240, 110), 1.5f * s);
    drawList->AddCircleFilled(puckPos, 7.5f * s,
                              thumbPressed ? IM_COL32(70, 210, 255, 255) : IM_COL32(55, 110, 195, 255), 24);
    drawList->AddCircle(puckPos, 7.5f * s, IM_COL32(190, 230, 255, 230), 24, 1.2f * s);
    drawList->AddCircleFilled(puckPos, 2.2f * s, IM_COL32(255, 255, 255, 255), 12);

    ImGui::Spacing();
    DrawTextCenteredInColumn(colStartX, colWidth, kTextColorGrey, "X: %+0.2f   Y: %+0.2f", static_cast<double>(normX),
                             static_cast<double>(normY));
    DrawTextCenteredInColumn(colStartX, colWidth, kTextColorGrey, "(%d, %d)", rawX, rawY);
}

/// Draws a 5-cell directional pad cross widget with directional chevron icons.
void DrawDPadCrossWidget(uint8_t dpad) {
    const float colStartX = ImGui::GetCursorPosX();
    const float colWidth = ImGui::GetContentRegionAvail().x;
    DrawTextCenteredInColumn(colStartX, colWidth, kTextColorWhite, "D-Pad");
    ImGui::Spacing();

    const float s = UiScale();
    const float cellSize = 30.0f * s;
    const float step = 34.0f * s;
    const float canvasSize = step * 2.0f + cellSize + 4.0f * s;

    ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - canvasSize) * 0.5f));
    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(canvasSize, canvasSize));

    const ImVec2 center = canvasMin + ImVec2(canvasSize * 0.5f, canvasSize * 0.5f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    auto drawCell = [&](float cx, float cy, bool active, bool isCenter) {
        const ImVec2 halfCell(cellSize * 0.5f, cellSize * 0.5f);
        const ImVec2 cMin = ImVec2(cx, cy) - halfCell;
        const ImVec2 cMax = ImVec2(cx, cy) + halfCell;
        const ImU32 fillCol =
            isCenter ? IM_COL32(30, 35, 45, 255) : (active ? IM_COL32(45, 130, 225, 255) : IM_COL32(38, 44, 56, 255));
        const ImU32 borderCol = active ? IM_COL32(120, 220, 255, 255) : IM_COL32(120, 135, 158, 190);
        drawList->AddRectFilled(cMin, cMax, fillCol, 5.0f * s);
        drawList->AddRect(cMin, cMax, borderCol, 5.0f * s, 0, (active ? 2.0f : 1.2f) * s);
    };

    const bool up = (dpad & DPAD_UP) != 0;
    const bool down = (dpad & DPAD_DOWN) != 0;
    const bool left = (dpad & DPAD_LEFT) != 0;
    const bool right = (dpad & DPAD_RIGHT) != 0;

    // Up cell + chevron ^
    {
        const float cx = center.x;
        const float cy = center.y - step;
        drawCell(cx, cy, up, false);
        const ImU32 iconCol = up ? IM_COL32(255, 255, 255, 255) : IM_COL32(185, 198, 215, 230);
        drawList->AddLine(ImVec2(cx - 5.0f * s, cy + 2.5f * s), ImVec2(cx, cy - 3.0f * s), iconCol, 2.0f * s);
        drawList->AddLine(ImVec2(cx, cy - 3.0f * s), ImVec2(cx + 5.0f * s, cy + 2.5f * s), iconCol, 2.0f * s);
    }
    // Left cell + chevron <
    {
        const float cx = center.x - step;
        const float cy = center.y;
        drawCell(cx, cy, left, false);
        const ImU32 iconCol = left ? IM_COL32(255, 255, 255, 255) : IM_COL32(185, 198, 215, 230);
        drawList->AddLine(ImVec2(cx + 2.5f * s, cy - 5.0f * s), ImVec2(cx - 3.0f * s, cy), iconCol, 2.0f * s);
        drawList->AddLine(ImVec2(cx - 3.0f * s, cy), ImVec2(cx + 2.5f * s, cy + 5.0f * s), iconCol, 2.0f * s);
    }
    // Center neutral cell
    {
        drawCell(center.x, center.y, false, true);
        drawList->AddCircleFilled(center, 4.5f * s, IM_COL32(115, 130, 150, 160), 16);
    }
    // Right cell + chevron >
    {
        const float cx = center.x + step;
        const float cy = center.y;
        drawCell(cx, cy, right, false);
        const ImU32 iconCol = right ? IM_COL32(255, 255, 255, 255) : IM_COL32(185, 198, 215, 230);
        drawList->AddLine(ImVec2(cx - 2.5f * s, cy - 5.0f * s), ImVec2(cx + 3.0f * s, cy), iconCol, 2.0f * s);
        drawList->AddLine(ImVec2(cx + 3.0f * s, cy), ImVec2(cx - 2.5f * s, cy + 5.0f * s), iconCol, 2.0f * s);
    }
    // Down cell + chevron v
    {
        const float cx = center.x;
        const float cy = center.y + step;
        drawCell(cx, cy, down, false);
        const ImU32 iconCol = down ? IM_COL32(255, 255, 255, 255) : IM_COL32(185, 198, 215, 230);
        drawList->AddLine(ImVec2(cx - 5.0f * s, cy - 2.5f * s), ImVec2(cx, cy + 3.0f * s), iconCol, 2.0f * s);
        drawList->AddLine(ImVec2(cx, cy + 3.0f * s), ImVec2(cx + 5.0f * s, cy - 2.5f * s), iconCol, 2.0f * s);
    }
}

/// Geometric PlayStation face-button sub-icons rendered beneath the primary letter label.
enum class PlayStationShape { kTriangle, kSquare, kCircle, kCross };

/// Draws the 4-button North/South/East/West action diamond, adapting letter labels to `layout`.
void DrawActionButtonsDiamondWidget(uint16_t buttons, ControllerLayoutType layout) {
    const float colStartX = ImGui::GetCursorPosX();
    const float colWidth = ImGui::GetContentRegionAvail().x;
    DrawTextCenteredInColumn(colStartX, colWidth, kTextColorWhite, "Action Buttons");
    ImGui::Spacing();

    const float s = UiScale();
    const float btnRadius = 19.0f * s;
    const float step = 33.0f * s;
    const float canvasSize = (step + btnRadius) * 2.0f + 8.0f * s;

    ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - canvasSize) * 0.5f));
    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(canvasSize, canvasSize));

    const ImVec2 center = canvasMin + ImVec2(canvasSize * 0.5f, canvasSize * 0.5f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    auto drawFaceButton = [&](float bx, float by, const char* letter, PlayStationShape shape, bool active) {
        const ImVec2 bCenter(bx, by);
        const ImU32 fillCol = active ? IM_COL32(45, 130, 225, 255) : IM_COL32(38, 44, 56, 255);
        const ImU32 borderCol = active ? IM_COL32(120, 220, 255, 255) : IM_COL32(130, 145, 168, 210);
        drawList->AddCircleFilled(bCenter, btnRadius, fillCol, 32);
        drawList->AddCircle(bCenter, btnRadius, borderCol, 32, (active ? 2.2f : 1.4f) * s);

        const ImVec2 lSz = ImGui::CalcTextSize(letter);
        drawList->AddText(ImVec2(bx - lSz.x * 0.5f, by - lSz.y + 1.0f * s), IM_COL32(255, 255, 255, 255), letter);

        const float sy = by + 7.5f * s;
        const ImU32 shapeCol = active ? IM_COL32(240, 250, 255, 255) : IM_COL32(150, 168, 190, 220);
        switch (shape) {
            case PlayStationShape::kTriangle:
                drawList->AddTriangle(ImVec2(bx, sy - 3.8f * s), ImVec2(bx - 4.0f * s, sy + 3.2f * s),
                                      ImVec2(bx + 4.0f * s, sy + 3.2f * s), shapeCol, 1.3f * s);
                break;
            case PlayStationShape::kSquare:
                drawList->AddRect(ImVec2(bx - 3.5f * s, sy - 3.5f * s), ImVec2(bx + 3.5f * s, sy + 3.5f * s), shapeCol,
                                  0.0f, 0, 1.3f * s);
                break;
            case PlayStationShape::kCircle:
                drawList->AddCircle(ImVec2(bx, sy), 3.7f * s, shapeCol, 20, 1.3f * s);
                break;
            case PlayStationShape::kCross:
                drawList->AddLine(ImVec2(bx - 3.3f * s, sy - 3.3f * s), ImVec2(bx + 3.3f * s, sy + 3.3f * s), shapeCol,
                                  1.4f * s);
                drawList->AddLine(ImVec2(bx + 3.3f * s, sy - 3.3f * s), ImVec2(bx - 3.3f * s, sy + 3.3f * s), shapeCol,
                                  1.4f * s);
                break;
        }
    };

    const bool isNintendo = (layout == CONTROLLER_LAYOUT_REVERSE);
    // North (Y on Xbox / X on Switch, Triangle on PS)
    drawFaceButton(center.x, center.y - step, isNintendo ? "X" : "Y", PlayStationShape::kTriangle,
                   (buttons & BUTTON_Y) != 0);
    // West (X on Xbox / Y on Switch, Square on PS)
    drawFaceButton(center.x - step, center.y, isNintendo ? "Y" : "X", PlayStationShape::kSquare,
                   (buttons & BUTTON_X) != 0);
    // East (B on Xbox / A on Switch, Circle on PS)
    drawFaceButton(center.x + step, center.y, isNintendo ? "A" : "B", PlayStationShape::kCircle,
                   (buttons & BUTTON_B) != 0);
    // South (A on Xbox / B on Switch, Cross on PS)
    drawFaceButton(center.x, center.y + step, isNintendo ? "B" : "A", PlayStationShape::kCross,
                   (buttons & BUTTON_A) != 0);
}

}  // namespace

void DemoScene::ControllerSlotUiState::ResetForSlot(int slot) noexcept {
    // Why `*this = ControllerSlotUiState{}`:
    //   Re-applies all C++23 Non-Static Data Member Initializers (NSDMI) in a single
    //   aggregate assignment, then customizes the default Player Indicator LED bitmask
    //   for `slot` (`#1..#4`) so no stale rumble, LED, or IMU state from a previously
    //   disconnected controller bleeds into a newly connected controller.
    *this = ControllerSlotUiState{};
    if (slot >= 0 && slot < kMaxControllers) {
        player_led_index = slot + 1;
        for (int b = 0; b < 4; ++b) {
            player_led_bits[static_cast<size_t>(b)] = (b == slot);
        }
    }
    std::snprintf(last_detected_input.data(), last_detected_input.size(), "None");
}

DemoScene::DemoScene()
    : mVirtualDevicesEnabled(posix_imgui_is_virtual_devices_enabled()),
      mAutoAcceptGamepads((posix_imgui_get_allowed_device_types() & POSIX_IMGUI_DEVICE_TYPE_GAMEPAD) != 0),
      mAutoAcceptMice((posix_imgui_get_allowed_device_types() & POSIX_IMGUI_DEVICE_TYPE_MOUSE) != 0),
      mAutoAcceptKeyboards((posix_imgui_get_allowed_device_types() & POSIX_IMGUI_DEVICE_TYPE_KEYBOARD) != 0),
      mBleServiceEnabled(posix_imgui_is_ble_service_enabled()) {
    posix_imgui_get_ble_service_name(mBleServiceName.data(), mBleServiceName.size());
    posix_imgui_get_ble_service_password(mBleServicePassword.data(), mBleServicePassword.size());
    for (int i = 0; i < kMaxControllers; ++i) {
        mSlots[static_cast<size_t>(i)].ResetForSlot(i);
    }
}

DemoScene::~DemoScene() = default;

void DemoScene::SelectCategoryTabForTest(int tab_index) noexcept {
    mRequestedCategoryTabForTest = tab_index;
}

void DemoScene::SetPreferencesActiveForTest(bool active) noexcept {
    mPreferencesActive = active;
    if (active) {
        mVirtualDevicesEnabled = posix_imgui_is_virtual_devices_enabled();
        const uint32_t allowedMask = posix_imgui_get_allowed_device_types();
        mAutoAcceptGamepads = (allowedMask & POSIX_IMGUI_DEVICE_TYPE_GAMEPAD) != 0;
        mAutoAcceptMice = (allowedMask & POSIX_IMGUI_DEVICE_TYPE_MOUSE) != 0;
        mAutoAcceptKeyboards = (allowedMask & POSIX_IMGUI_DEVICE_TYPE_KEYBOARD) != 0;
        mBleServiceEnabled = posix_imgui_is_ble_service_enabled();
        posix_imgui_get_ble_service_name(mBleServiceName.data(), mBleServiceName.size());
        posix_imgui_get_ble_service_password(mBleServicePassword.data(), mBleServicePassword.size());
    }
}

const DemoScene::ControllerSlotUiState& DemoScene::GetSlotStateForTest(int slot) const noexcept {
    const int clamped = (slot >= 0 && slot < kMaxControllers) ? slot : 0;
    return mSlots[static_cast<size_t>(clamped)];
}

DemoScene::ControllerSlotUiState& DemoScene::MutateSlotStateForTest(int slot) noexcept {
    const int clamped = (slot >= 0 && slot < kMaxControllers) ? slot : 0;
    return mSlots[static_cast<size_t>(clamped)];
}

void DemoScene::UpdateImuHistory(int slot, const ControllerSnapshot& snap) {
    if (slot < 0 || slot >= kMaxControllers) {
        return;
    }
    ControllerSlotUiState& slot_state = mSlots[static_cast<size_t>(slot)];
    if (!snap.connected) {
        if (slot_state.prev_connected) {
            ClearImuHistory(slot);
        }
        return;
    }
    if (mImuPlotPaused) {
        return;
    }
    // Only record a new sample when `last_report_timestamp_us` advances so a controller
    // reporting at e.g. 30 Hz is not duplicated across consecutive 60 Hz UI frames.
    if (snap.last_report_timestamp_us == 0 || snap.last_report_timestamp_us == slot_state.last_imu_timestamp_us) {
        return;
    }

    float dt_sec = 0.0f;
    if (slot_state.last_imu_timestamp_us > 0 && snap.last_report_timestamp_us > slot_state.last_imu_timestamp_us) {
        dt_sec = static_cast<float>(snap.last_report_timestamp_us - slot_state.last_imu_timestamp_us) * 1e-6f;
    } else if (snap.report_delta_ms > 0 && snap.report_delta_ms <= 500) {
        dt_sec = static_cast<float>(snap.report_delta_ms) * 1e-3f;
    }
    slot_state.last_imu_timestamp_us = snap.last_report_timestamp_us;

    const size_t idx = slot_state.imu_history_offset;
    for (size_t axis = 0; axis < kMotionAxisCount; ++axis) {
        const float gyroRadS = snap.controller.gamepad.gyro[axis];
        const float accelMps2 = snap.controller.gamepad.accel[axis];
        slot_state.gyro_history[axis][idx] = gyroRadS;
        slot_state.accel_history[axis][idx] = accelMps2;

        if (dt_sec > 0.0f && dt_sec <= 0.5f) {
            const float degPerSec = gyroRadS * kRadToDeg;
            // Integrate angular rate into [-180, +180] degree dial angle
            slot_state.gyro_angle_deg[axis] =
                std::remainder(slot_state.gyro_angle_deg[axis] + degPerSec * dt_sec, 360.0f);
        }
    }
    slot_state.imu_history_offset = (idx + 1) % kImuHistoryLen;
}

void DemoScene::ClearImuHistory(int slot) {
    if (slot < 0 || slot >= kMaxControllers) {
        return;
    }
    ControllerSlotUiState& slot_state = mSlots[static_cast<size_t>(slot)];
    slot_state.gyro_history = {};
    slot_state.accel_history = {};
    slot_state.gyro_angle_deg = {};
    slot_state.imu_history_offset = 0;
    slot_state.last_imu_timestamp_us = 0;
}

void DemoScene::UpdateLastDetectedInput(int slot, const ControllerSnapshot& snap) {
    if (slot < 0 || slot >= kMaxControllers) {
        return;
    }
    ControllerSlotUiState& slot_state = mSlots[static_cast<size_t>(slot)];
    if (!snap.connected) {
        std::snprintf(slot_state.last_detected_input.data(), slot_state.last_detected_input.size(), "None");
        slot_state.prev_buttons = 0;
        slot_state.prev_dpad = 0;
        slot_state.prev_misc_buttons = 0;
        slot_state.prev_l2_active = false;
        slot_state.prev_r2_active = false;
        slot_state.prev_left_stick_active = false;
        slot_state.prev_right_stick_active = false;
        return;
    }

    const uni_gamepad_t& gp = snap.controller.gamepad;
    const uint16_t newBtns = static_cast<uint16_t>(gp.buttons & ~slot_state.prev_buttons);
    const uint8_t newDpad = static_cast<uint8_t>(gp.dpad & ~slot_state.prev_dpad);
    const uint8_t newMisc = static_cast<uint8_t>(gp.misc_buttons & ~slot_state.prev_misc_buttons);

    auto setDetected = [&](const char* label) {
        std::snprintf(slot_state.last_detected_input.data(), slot_state.last_detected_input.size(), "%s", label);
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
    if (l2Active && !slot_state.prev_l2_active) {
        setDetected("LT / L2");
    }
    if (r2Active && !slot_state.prev_r2_active) {
        setDetected("RT / R2");
    }

    const float effectiveDeadzone = mDontTrimDeadzone ? 0.0f : std::max(0.15f, mRadialDeadzone);
    const ImVec2 leftStick = ApplyRadialDeadzone(gp.axis_x, gp.axis_y, effectiveDeadzone);
    const ImVec2 rightStick = ApplyRadialDeadzone(gp.axis_rx, gp.axis_ry, effectiveDeadzone);
    const bool leftActive = (std::hypot(leftStick.x, leftStick.y) > effectiveDeadzone);
    const bool rightActive = (std::hypot(rightStick.x, rightStick.y) > effectiveDeadzone);
    if (leftActive && !slot_state.prev_left_stick_active) {
        setDetected("Left Stick");
    }
    if (rightActive && !slot_state.prev_right_stick_active) {
        setDetected("Right Stick");
    }

    slot_state.prev_buttons = gp.buttons;
    slot_state.prev_dpad = gp.dpad;
    slot_state.prev_misc_buttons = gp.misc_buttons;
    slot_state.prev_l2_active = l2Active;
    slot_state.prev_r2_active = r2Active;
    slot_state.prev_left_stick_active = leftActive;
    slot_state.prev_right_stick_active = rightActive;
}

void DemoScene::UpdateTriggerRumble(int slot, const ControllerSnapshot& snap) {
    if (slot < 0 || slot >= kMaxControllers) {
        return;
    }
    ControllerSlotUiState& slot_state = mSlots[static_cast<size_t>(slot)];
    if (!snap.connected || !snap.has_rumble || !slot_state.trigger_rumble_enabled) {
        if (slot_state.trigger_rumble_active) {
            posix_imgui_request_rumble(slot, 0, 0, 0, 0);
            slot_state.trigger_rumble_active = false;
            slot_state.last_trigger_strong_u8 = 0;
            slot_state.last_trigger_weak_u8 = 0;
        }
        return;
    }

    const uni_gamepad_t& gp = snap.controller.gamepad;
    const float l2Norm = NormalizeTriggerAxis(gp.brake, (gp.buttons & BUTTON_TRIGGER_L) != 0);
    const float r2Norm = NormalizeTriggerAxis(gp.throttle, (gp.buttons & BUTTON_TRIGGER_R) != 0);

    // Threshold minor trigger noise below 2%
    const float strongNorm = (l2Norm >= 0.02f) ? l2Norm : 0.0f;
    const float weakNorm = (r2Norm >= 0.02f) ? r2Norm : 0.0f;
    const uint8_t strongU8 = static_cast<uint8_t>(std::clamp(std::round(strongNorm * 255.0f), 0.0f, 255.0f));
    const uint8_t weakU8 = static_cast<uint8_t>(std::clamp(std::round(weakNorm * 255.0f), 0.0f, 255.0f));

    if (strongU8 == 0 && weakU8 == 0) {
        if (slot_state.trigger_rumble_active) {
            posix_imgui_request_rumble(slot, 0, 0, 0, 0);
            slot_state.trigger_rumble_active = false;
            slot_state.last_trigger_strong_u8 = 0;
            slot_state.last_trigger_weak_u8 = 0;
        }
        return;
    }

    // Reflect live trigger pressure on the Rumble tab sliders while Trigger Rumble Mode is active
    slot_state.rumble_strong_intensity = strongNorm;
    slot_state.rumble_weak_intensity = weakNorm;

    const double nowSec = ImGui::GetTime();
    const double elapsedSec = nowSec - slot_state.last_trigger_rumble_time_sec;
    const int deltaStrong = std::abs(static_cast<int>(strongU8) - static_cast<int>(slot_state.last_trigger_strong_u8));
    const int deltaWeak = std::abs(static_cast<int>(weakU8) - static_cast<int>(slot_state.last_trigger_weak_u8));

    // Rate-limit Bluetooth HID output reports:
    //   - Dispatch immediately when transitioning from idle -> active
    //   - Dispatch at up to 20 Hz (50ms) when trigger pressure changes by >= 8 counts
    //   - Sustain at 8 Hz (125ms) with a 200ms pulse window while held steady
    if (!slot_state.trigger_rumble_active || (elapsedSec >= 0.05 && (deltaStrong >= 8 || deltaWeak >= 8)) ||
        elapsedSec >= 0.125) {
        posix_imgui_request_rumble(slot, 0, 200, weakU8, strongU8);
        slot_state.trigger_rumble_active = true;
        slot_state.last_trigger_rumble_time_sec = nowSec;
        slot_state.last_trigger_strong_u8 = strongU8;
        slot_state.last_trigger_weak_u8 = weakU8;
    }
}

void DemoScene::DoFrame() {
    int newly_connected = -1;
    posix_imgui_get_snapshots(std::span{mSnapshots}, &newly_connected);
    if (newly_connected >= 0 && newly_connected < kMaxControllers) {
        mMostRecentConnectedSlot = newly_connected;
    }

    for (int i = 0; i < kMaxControllers; ++i) {
        const auto slot_idx = static_cast<size_t>(i);
        // Why `ResetForSlot(i)` runs BEFORE `UpdateImuHistory` / `UpdateLastDetectedInput`:
        //   When a slot transitions from disconnected to connected (`!prev_connected && connected`),
        //   resetting the slot's UI state first ensures any initial input report already present
        //   in `mSnapshots[slot_idx]` on the connection frame is recorded into fresh buffers.
        if (!mSlots[slot_idx].prev_connected && mSnapshots[slot_idx].connected) {
            mSlots[slot_idx].ResetForSlot(i);
            if (!mSnapshots[slot_idx].is_virtual_device) {
                mMostRecentConnectedSlot = i;
            }
        }
        UpdateImuHistory(i, mSnapshots[slot_idx]);
        UpdateLastDetectedInput(i, mSnapshots[slot_idx]);
        UpdateTriggerRumble(i, mSnapshots[slot_idx]);
        mSlots[slot_idx].prev_connected = mSnapshots[slot_idx].connected;
    }

    // Balance `PushStyleVar`/`PopStyleVar` and `Begin`/`End` within `DoFrame()` (and always
    // call `ImGui::End()` regardless of `ImGui::Begin()`'s return value, per Dear ImGui contract).
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 20.0f);
    if (ImGui::Begin("Bluepad32 POSIX Controller Tester (Dear ImGui)", nullptr, kWindowFlags)) {
        if (!RenderPreferences()) {
            ImGui::SameLine(0.0f, 24.0f);
            RenderStatusBar();
            ImGui::Separator();
            RenderControllerTabs();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void DemoScene::RenderStatusBar() {
    int connected_count = 0;
    for (const ControllerSnapshot& snap : mSnapshots) {
        if (snap.connected) {
            connected_count++;
        }
    }

    ImGui::AlignTextToFramePadding();
    if (connected_count > 0) {
        ImGui::TextColored(kTextColorGreen, "Connected Controllers: %d / %d", connected_count, kMaxControllers);
    } else {
        ImGui::TextColored(kTextColorYellow, "Connected Controllers: 0 / %d (Scanning via Bluetooth...)",
                           kMaxControllers);
    }

    ImGui::SameLine(0.0f, 20.0f);
    if (posix_imgui_is_ble_service_enabled()) {
        char svc_name[UNI_BT_SERVICE_NAME_MAX_LEN + 1]{};
        char svc_pass[UNI_BT_SERVICE_PASSWORD_MAX_LEN + 1]{};
        posix_imgui_get_ble_service_name(svc_name, sizeof(svc_name));
        posix_imgui_get_ble_service_password(svc_pass, sizeof(svc_pass));
        const bool locked = (svc_pass[0] != '\0');
        ImGui::TextColored(kTextColorCyan, "BLE Service: \"%s\" [%s]", svc_name, locked ? "Password" : "Open");
    } else {
        ImGui::TextColored(kTextColorGrey, "BLE Service: Off");
    }

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::TextColored(kTextColorGrey, "Radial Deadzone: %.0f%% | %.1f FPS",
                       static_cast<double>((mDontTrimDeadzone ? 0.0f : mRadialDeadzone) * 100.0f),
                       static_cast<double>(ImGui::GetIO().Framerate));
}

bool DemoScene::RenderPreferences() {
    if (!mPreferencesActive) {
        if (ImGui::Button("Preferences...")) {
            mPreferencesActive = true;
            // Synchronize UI checkboxes and BLE service fields with the authoritative platform state.
            mVirtualDevicesEnabled = posix_imgui_is_virtual_devices_enabled();
            const uint32_t allowedMask = posix_imgui_get_allowed_device_types();
            mAutoAcceptGamepads = (allowedMask & POSIX_IMGUI_DEVICE_TYPE_GAMEPAD) != 0;
            mAutoAcceptMice = (allowedMask & POSIX_IMGUI_DEVICE_TYPE_MOUSE) != 0;
            mAutoAcceptKeyboards = (allowedMask & POSIX_IMGUI_DEVICE_TYPE_KEYBOARD) != 0;
            mBleServiceEnabled = posix_imgui_is_ble_service_enabled();
            posix_imgui_get_ble_service_name(mBleServiceName.data(), mBleServiceName.size());
            posix_imgui_get_ble_service_password(mBleServicePassword.data(), mBleServicePassword.size());
        }
        return false;
    }

    mBleServiceEnabled = posix_imgui_is_ble_service_enabled();

    // Dear ImGui 1.92+ / 1.93.0 WIP uses `style.FontScaleMain` instead of legacy `io.FontGlobalScale`.
    ImGuiStyle& style = ImGui::GetStyle();
    mFontScale = style.FontScaleMain;

    ImGui::TextColored(kTextColorCyan, "Display & Input Preferences");
    ImGui::Separator();

    ImGui::Spacing();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Font scale:");
    ImGui::SameLine(180.0f * UiScale());
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
    ImGui::SetNextItemWidth(180.0f * UiScale());
    if (ImGui::SliderFloat("##font_slider", &mFontScale, kFontScaleMin, kFontScaleMax, "%.2fx")) {
        style.FontScaleMain = mFontScale;
    }

    ImGui::Spacing();
    ImGui::Checkbox("Raw deadzone (do not trim stick center deadzone to 0.0)", &mDontTrimDeadzone);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextColored(kTextColorCyan, "Bluetooth Auto-Connect Device Filter");
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(
        kTextColorGrey,
        "Select which physical Bluetooth HID device categories are automatically accepted when discovered:");

    bool filterChanged = false;
    filterChanged |= ImGui::Checkbox("Gamepads & Joysticks (Default: ON)", &mAutoAcceptGamepads);
    filterChanged |= ImGui::Checkbox("Mice (Default: OFF)", &mAutoAcceptMice);
    filterChanged |= ImGui::Checkbox("Keyboards (Default: OFF)", &mAutoAcceptKeyboards);
    if (filterChanged) {
        uint32_t allowedMask = POSIX_IMGUI_DEVICE_TYPE_NONE;
        if (mAutoAcceptGamepads) {
            allowedMask |= POSIX_IMGUI_DEVICE_TYPE_GAMEPAD;
        }
        if (mAutoAcceptMice) {
            allowedMask |= POSIX_IMGUI_DEVICE_TYPE_MOUSE;
        }
        if (mAutoAcceptKeyboards) {
            allowedMask |= POSIX_IMGUI_DEVICE_TYPE_KEYBOARD;
        }
        posix_imgui_request_set_allowed_device_types(allowedMask);
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextColored(kTextColorCyan, "Virtual Devices");
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox("Enable Virtual Devices (DualSense / DualShock 4 Touchpad Mouse)", &mVirtualDevicesEnabled)) {
        posix_imgui_request_set_virtual_devices_enabled(mVirtualDevicesEnabled);
    }
    ImGui::TextColored(kTextColorGrey,
                       "When disabled (default), DualSense and DualShock 4 controllers do not spawn a secondary "
                       "virtual mouse slot.");

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextColored(kTextColorCyan, "BLE Configuration");
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox("Enable BLE Service", &mBleServiceEnabled)) {
        posix_imgui_request_set_ble_service_enabled(mBleServiceEnabled);
    }

    char active_ble_name[UNI_BT_SERVICE_NAME_MAX_LEN + 1]{};
    char active_ble_pass[UNI_BT_SERVICE_PASSWORD_MAX_LEN + 1]{};
    posix_imgui_get_ble_service_name(active_ble_name, sizeof(active_ble_name));
    posix_imgui_get_ble_service_password(active_ble_pass, sizeof(active_ble_pass));
    const bool is_password_protected = (active_ble_pass[0] != '\0');

    ImGui::SameLine(0.0f, 16.0f * UiScale());
    if (!mBleServiceEnabled) {
        ImGui::TextColored(kTextColorGrey, "[Service Disabled]");
    } else if (is_password_protected) {
        ImGui::TextColored(kTextColorYellow, "[Password Protected — Advertising as \"%s\"]", active_ble_name);
    } else {
        ImGui::TextColored(kTextColorGreen, "[Open Access (No Password) — Advertising as \"%s\"]", active_ble_name);
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!mBleServiceEnabled);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("BLE Service Name:");
    ImGui::SameLine(180.0f * UiScale());
    ImGui::SetNextItemWidth(260.0f * UiScale());
    const bool name_enter = ImGui::InputText("##ble_service_name", mBleServiceName.data(), mBleServiceName.size(),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button("Apply##ble_name") || name_enter) {
        posix_imgui_request_set_ble_service_name(mBleServiceName.data());
        posix_imgui_get_ble_service_name(mBleServiceName.data(), mBleServiceName.size());
    }

    ImGui::Spacing();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("BLE Password:");
    ImGui::SameLine(180.0f * UiScale());
    ImGui::SetNextItemWidth(260.0f * UiScale());
    const ImGuiInputTextFlags pw_flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                         (mBleShowPassword ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_Password);
    const bool pw_enter =
        ImGui::InputText("##ble_service_password", mBleServicePassword.data(), mBleServicePassword.size(), pw_flags);
    ImGui::SameLine();
    if (ImGui::Button("Apply##ble_pw") || pw_enter) {
        posix_imgui_request_set_ble_service_password(mBleServicePassword.data());
        posix_imgui_get_ble_service_password(mBleServicePassword.data(), mBleServicePassword.size());
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear##ble_pw")) {
        mBleServicePassword.fill('\0');
        posix_imgui_request_set_ble_service_password("");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Show Password", &mBleShowPassword);

    ImGui::EndDisabled();
    ImGui::TextColored(kTextColorGrey,
                       "Identifies this Bluepad32 instance on the BLE companion app (max 29 UTF-8 bytes) and "
                       "optionally requires a password (max 31 bytes) before reading or modifying settings.");

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
            const auto slot_idx = static_cast<size_t>(slot);
            ImGuiTabItemFlags tabItemFlags = ImGuiTabItemFlags_None;
            // Auto-focus a newly connected controller tab for a single frame, then clear
            // `mMostRecentConnectedSlot` so the user can freely switch tabs afterward.
            if (mMostRecentConnectedSlot == slot) {
                tabItemFlags |= ImGuiTabItemFlags_SetSelected;
                mMostRecentConnectedSlot = -1;
            }

            const bool isConnected = mSnapshots[slot_idx].connected;
            const ImVec4 tabTextColor = isConnected ? kTextColorWhite : kTextColorGrey;

            ImGui::PushStyleColor(ImGuiCol_Text, tabTextColor);
            if (ImGui::BeginTabItem(kControllerTabNames[slot_idx], nullptr, tabItemFlags)) {
                mCurrentControllerSlot = slot;
                ImGui::PopStyleColor(1);

                if (isConnected) {
                    RenderPanel(slot, mSnapshots[slot_idx]);
                } else {
                    ImGui::Spacing();
                    ImGui::TextColored(kTextColorGrey, "Slot #%d (Seat %c): Not connected", slot + 1, 'A' + slot);
                    ImGui::Spacing();
                    ImGui::TextWrapped("Place a Bluetooth gamepad into pairing mode to connect automatically.");
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
    // Push `slot` onto the Dear ImGui ID stack so child windows, sliders, and buttons
    // never collide across controller slots (`#1..#4`).
    ImGui::PushID(slot);

    const char* displayName = (snap.name[0] != '\0') ? snap.name : snap.model_name;
    ImGui::TextColored(kTextColorGreen, "[Seat #%d] %s%s", slot + 1, displayName,
                       snap.is_virtual_device ? " [Virtual Device]" : "");
    ImGui::SameLine(0.0f, 16.0f);
    ImGui::TextColored(kTextColorGrey, "(Model: %s | VID: 0x%04X PID: 0x%04X | MAC: %s)", snap.model_name,
                       snap.vendor_id, snap.product_id, bd_addr_to_str(snap.btaddr));

    if (ImGui::BeginTabBar("CategoryTabBar", ImGuiTabBarFlags_NoTooltip)) {
        struct CategoryTab {
            int index;
            const char* title;
            void (DemoScene::*renderFn)(int, const ControllerSnapshot&);
        };

        constexpr std::array<CategoryTab, 5> kTabs = {{
            {0, " Controls ", &DemoScene::RenderPanel_ControlsTab},
            {1, " Rumble ", &DemoScene::RenderPanel_VibrationTab},
            {2, " IMU ", &DemoScene::RenderPanel_MotionTab},
            {3, " Lights ", &DemoScene::RenderPanel_LightsTab},
            {4, " Info ", &DemoScene::RenderPanel_InfoTab},
        }};

        for (const CategoryTab& tab : kTabs) {
            ImGuiTabItemFlags tabFlags = ImGuiTabItemFlags_None;
            if (mRequestedCategoryTabForTest == tab.index) {
                tabFlags |= ImGuiTabItemFlags_SetSelected;
                mRequestedCategoryTabForTest = -1;
            }
            const bool isActive = (mActiveControllerPanelTab == tab.index);
            const ImVec4 tabColor = isActive ? kTextColorWhite : kTextColorGrey;
            ImGui::PushStyleColor(ImGuiCol_Text, tabColor);
            if (ImGui::BeginTabItem(tab.title, nullptr, tabFlags)) {
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

    ImGui::PopID();
}

void DemoScene::RenderPanel_ControlsTab(int slot, const ControllerSnapshot& snap) {
    const ControllerSlotUiState& slot_state = mSlots[static_cast<size_t>(slot)];
    const uni_gamepad_t& gp = snap.controller.gamepad;
    const float effectiveDeadzone = mDontTrimDeadzone ? 0.0f : mRadialDeadzone;
    const float s = UiScale();

    const ImVec2 leftStick = ApplyRadialDeadzone(gp.axis_x, gp.axis_y, effectiveDeadzone);
    const ImVec2 rightStick = ApplyRadialDeadzone(gp.axis_rx, gp.axis_ry, effectiveDeadzone);

    const float l2Val = NormalizeTriggerAxis(gp.brake, (gp.buttons & BUTTON_TRIGGER_L) != 0);
    const float r2Val = NormalizeTriggerAxis(gp.throttle, (gp.buttons & BUTTON_TRIGGER_R) != 0);
    const bool l1Active = (gp.buttons & BUTTON_SHOULDER_L) != 0;
    const bool r1Active = (gp.buttons & BUTTON_SHOULDER_R) != 0;
    const bool l3Active = (gp.buttons & BUTTON_THUMB_L) != 0;
    const bool r3Active = (gp.buttons & BUTTON_THUMB_R) != 0;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f * s);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.13f, 0.17f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.26f, 0.30f, 0.38f, 0.75f));

    ImGui::Spacing();

    constexpr ImGuiChildFlags kCardChildFlags = ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY;
    constexpr ImGuiWindowFlags kCardWindowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    // ========================================================================
    // 1. Top Row: LT / L2 Trigger Card  |  LB/L1 & RB/R1  |  RT / R2 Trigger Card
    // ========================================================================
    if (ImGui::BeginTable("##controls_top_triggers", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("##lt_col", ImGuiTableColumnFlags_WidthStretch, 0.41f);
        ImGui::TableSetupColumn("##bumpers_col", ImGuiTableColumnFlags_WidthStretch, 0.18f);
        ImGui::TableSetupColumn("##rt_col", ImGuiTableColumnFlags_WidthStretch, 0.41f);
        ImGui::TableNextRow();

        auto renderTriggerCard = [&](const char* childId, const char* label, float normVal, int32_t rawVal) {
            if (ImGui::BeginChild(childId, ImVec2(0.0f, 0.0f), kCardChildFlags, kCardWindowFlags)) {
                ImGui::TextColored(normVal > 0.01f ? kTextColorCyan : kTextColorWhite, "%s", label);

                const float barWidth = std::max(40.0f * s, ImGui::GetContentRegionAvail().x);
                const float barHeight = 13.0f * s;
                const float barRounding = 6.5f * s;
                const ImVec2 barMin = ImGui::GetCursorScreenPos();
                const ImVec2 barMax = barMin + ImVec2(barWidth, barHeight);
                ImGui::Dummy(ImVec2(barWidth, barHeight));

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(barMin, barMax, IM_COL32(20, 24, 32, 255), barRounding);
                if (normVal > 0.002f) {
                    const float fillW = std::max(barHeight, barWidth * std::clamp(normVal, 0.0f, 1.0f));
                    drawList->AddRectFilled(barMin, ImVec2(barMin.x + fillW, barMax.y), IM_COL32(55, 150, 245, 255),
                                            barRounding);
                }
                drawList->AddRect(barMin, barMax, IM_COL32(120, 135, 158, 200), barRounding, 0, 1.2f * s);

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
            const float pillW = 66.0f * s;
            const float pillH = 34.0f * s;
            const float pillGap = 10.0f * s;
            const float totalW = pillW * 2.0f + pillGap;

            ImGui::Dummy(ImVec2(0.0f, 16.0f * s));
            ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - totalW) * 0.5f));
            const ImVec2 rowMin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(totalW, pillH));

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            DrawVectorButtonBadge(drawList, rowMin, ImVec2(pillW, pillH), "LB / L1", nullptr, l1Active);
            DrawVectorButtonBadge(drawList, rowMin + ImVec2(pillW + pillGap, 0.0f), ImVec2(pillW, pillH), "RB / R1",
                                  nullptr, r1Active);
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
            if (ImGui::BeginChild("##left_stick_card", ImVec2(0.0f, 0.0f), kCardChildFlags, kCardWindowFlags)) {
                DrawStickWellWidget("Left Stick (L3)", gp.axis_x, gp.axis_y, leftStick.x, leftStick.y,
                                    effectiveDeadzone, l3Active);
            }
            ImGui::EndChild();

            ImGui::Spacing();

            if (ImGui::BeginChild("##dpad_card", ImVec2(0.0f, 0.0f), kCardChildFlags, kCardWindowFlags)) {
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
            if (ImGui::BeginChild("##nav_buttons_card", ImVec2(0.0f, 0.0f), kCardChildFlags, kCardWindowFlags)) {
                const float colStartX = ImGui::GetCursorPosX();
                const float colWidth = ImGui::GetContentRegionAvail().x;
                DrawTextCenteredInColumn(colStartX, colWidth, kTextColorWhite, "Navigation Buttons");
                ImGui::Spacing();

                const float btnW = 74.0f * s;
                const float btnH = 38.0f * s;
                const float gap = 10.0f * s;
                const float rowW = btnW * 3.0f + gap * 2.0f;

                ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - rowW) * 0.5f));
                const ImVec2 rowMin = ImGui::GetCursorScreenPos();
                ImGui::Dummy(ImVec2(rowW, btnH));

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                DrawVectorButtonBadge(drawList, rowMin, ImVec2(btnW, btnH), "Select", "Share",
                                      (gp.misc_buttons & MISC_BUTTON_SELECT) != 0);
                DrawVectorButtonBadge(drawList, rowMin + ImVec2(btnW + gap, 0.0f), ImVec2(btnW, btnH), "Mode",
                                      "Guide / PS", (gp.misc_buttons & MISC_BUTTON_SYSTEM) != 0);
                DrawVectorButtonBadge(drawList, rowMin + ImVec2((btnW + gap) * 2.0f, 0.0f), ImVec2(btnW, btnH), "Start",
                                      "Options", (gp.misc_buttons & MISC_BUTTON_START) != 0);
            }
            ImGui::EndChild();

            ImGui::Spacing();

            // Card 2B: Extra Buttons (Capture, L3, R3, plus live hex bitmasks)
            if (ImGui::BeginChild("##extra_buttons_card", ImVec2(0.0f, 0.0f), kCardChildFlags, kCardWindowFlags)) {
                const float colStartX = ImGui::GetCursorPosX();
                const float colWidth = ImGui::GetContentRegionAvail().x;
                DrawTextCenteredInColumn(colStartX, colWidth, kTextColorWhite, "Extra Buttons");
                DrawTextCenteredInColumn(colStartX, colWidth, kTextColorGrey, "Auxiliary, thumb-click & bitmask keys");
                ImGui::Spacing();

                const float btnW = 74.0f * s;
                const float btnH = 36.0f * s;
                const float gap = 10.0f * s;
                const float rowW = btnW * 3.0f + gap * 2.0f;

                ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - rowW) * 0.5f));
                const ImVec2 rowMin = ImGui::GetCursorScreenPos();
                ImGui::Dummy(ImVec2(rowW, btnH));

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                DrawVectorButtonBadge(drawList, rowMin, ImVec2(btnW, btnH), "Capture", "Mute / Share",
                                      (gp.misc_buttons & MISC_BUTTON_CAPTURE) != 0);
                DrawVectorButtonBadge(drawList, rowMin + ImVec2(btnW + gap, 0.0f), ImVec2(btnW, btnH), "L3",
                                      "Left Stick", l3Active);
                DrawVectorButtonBadge(drawList, rowMin + ImVec2((btnW + gap) * 2.0f, 0.0f), ImVec2(btnW, btnH), "R3",
                                      "Right Stick", r3Active);

                ImGui::Spacing();
                DrawTextCenteredInColumn(colStartX, colWidth, kTextColorGrey,
                                         "Buttons: 0x%04X   DPad: 0x%02X   Misc: 0x%02X", gp.buttons, gp.dpad,
                                         gp.misc_buttons);
            }
            ImGui::EndChild();

            ImGui::Spacing();

            // Card 2C: LAST DETECTED INPUT
            if (ImGui::BeginChild("##last_detected_card", ImVec2(0.0f, 0.0f), kCardChildFlags, kCardWindowFlags)) {
                ImGui::TextColored(kTextColorGrey, "LAST DETECTED INPUT");
                ImGui::Indent(8.0f * s);
                ImGui::TextColored(kTextColorCyan, "%s", slot_state.last_detected_input.data());
                ImGui::Unindent(8.0f * s);
            }
            ImGui::EndChild();

            ImGui::Spacing();

            // Card 2D: Radial Deadzone Slider (0% .. 35%)
            if (ImGui::BeginChild("##radial_deadzone_card", ImVec2(0.0f, 0.0f), kCardChildFlags, kCardWindowFlags)) {
                ImGui::TextColored(kTextColorWhite, "Radial Deadzone");
                ImGui::SameLine();
                DrawRightAlignedText(kTextColorCyan, "%.0f%%", static_cast<double>(effectiveDeadzone * 100.0f));

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
            if (ImGui::BeginChild("##action_buttons_card", ImVec2(0.0f, 0.0f), kCardChildFlags, kCardWindowFlags)) {
                DrawActionButtonsDiamondWidget(gp.buttons, snap.layout);
            }
            ImGui::EndChild();

            ImGui::Spacing();

            if (ImGui::BeginChild("##right_stick_card", ImVec2(0.0f, 0.0f), kCardChildFlags, kCardWindowFlags)) {
                DrawStickWellWidget("Right Stick (R3)", gp.axis_rx, gp.axis_ry, rightStick.x, rightStick.y,
                                    effectiveDeadzone, r3Active);
            }
            ImGui::EndChild();
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(1);
}

void DemoScene::RenderPanel_InfoTab(int slot, const ControllerSnapshot& snap) {
    const float s = UiScale();
    ImGui::Spacing();
    ImGui::TextColored(kTextColorCyan, "Hardware & Bluetooth Link Diagnostics");
    ImGui::Separator();

    if (ImGui::BeginTable("##infotable", 2,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 240.0f * s);
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
        addRow("Device Type:", "%s",
               snap.is_virtual_device ? "Virtual Child Device (Touchpad Mouse)" : "Physical Bluetooth HID Device");
        addRow("Controller Subtype:", "%s (%d)", SubtypeToString(snap.controller_subtype),
               static_cast<int>(snap.controller_subtype));
        addRow("Device Extra Info:", "%s",
               snap.device_extra_info[0] != '\0' ? snap.device_extra_info : "Not available");
        addRow("Vendor ID / Product ID:", "VID: 0x%04X  |  PID: 0x%04X", snap.vendor_id, snap.product_id);
        addRow("Bluetooth MAC Address:", "%s", bd_addr_to_str(snap.btaddr));
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
            const unsigned int pct = static_cast<unsigned int>(std::round(frac * 100.0f));
            char overlay[64];
            std::snprintf(overlay, sizeof(overlay), "%u%% (%u / 254)", pct, static_cast<unsigned int>(battery));
            ImGui::SetNextItemWidth(220.0f * s);
            ImGui::ProgressBar(frac, ImVec2(220.0f * s, 0.0f), overlay);
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
    ControllerSlotUiState& slot_state = mSlots[static_cast<size_t>(slot)];
    const float s = UiScale();

    ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f * s);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.11f, 0.14f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.25f, 0.32f, 1.0f));

    constexpr ImGuiChildFlags kCardChildFlags = ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY;
    constexpr ImGuiWindowFlags kCardWindowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    // ========================================================================
    // Card 1: Dual-Motor Force Amplitude
    // ========================================================================
    if (ImGui::BeginChild("##rumble_amplitude_card", ImVec2(0.0f, 0.0f), kCardChildFlags, kCardWindowFlags)) {
        ImGui::TextColored(kTextColorWhite, "Dual-Motor Force Amplitude");
        ImGui::SameLine();

        const char* statusLabel = !snap.has_rumble                   ? "No Vibrator"
                                  : slot_state.trigger_rumble_active ? "Trigger Haptics Active"
                                                                     : "Vibrator Ready";
        const ImVec4 statusColor = !snap.has_rumble                   ? kTextColorYellow
                                   : slot_state.trigger_rumble_active ? kTextColorCyan
                                                                      : kTextColorGreen;
        const ImU32 dotColor = !snap.has_rumble                   ? IM_COL32(255, 215, 50, 255)
                               : slot_state.trigger_rumble_active ? IM_COL32(75, 215, 255, 255)
                                                                  : IM_COL32(65, 225, 110, 255);

        const float statusTextW = ImGui::CalcTextSize(statusLabel).x;
        const float availHeaderW = ImGui::GetContentRegionAvail().x;
        if (availHeaderW > statusTextW + 16.0f * s) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availHeaderW - statusTextW);
        }
        const ImVec2 statusScreenPos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(statusScreenPos.x - 10.0f * s, statusScreenPos.y + ImGui::GetTextLineHeight() * 0.5f), 4.0f * s,
            dotColor, 16);
        ImGui::TextColored(statusColor, "%s", statusLabel);

        if (!snap.has_rumble) {
            ImGui::TextColored(kTextColorYellow,
                               "Note: %s does not advertise rumble support (`play_dual_rumble` unavailable).",
                               snap.model_name);
        }

        ImGui::Spacing();

        // Slider 1: Left Motor (Heavy / Low Freq) -> strong_magnitude (0..255)
        const uint8_t strongU8 =
            static_cast<uint8_t>(std::clamp(std::round(slot_state.rumble_strong_intensity * 255.0f), 0.0f, 255.0f));
        ImGui::TextColored(kTextColorWhite, "Left Motor (Heavy / Low Freq)");
        ImGui::SameLine();
        DrawRightAlignedText(kTextColorCyan, "%.0f%% (%u/255)",
                             static_cast<double>(slot_state.rumble_strong_intensity * 100.0f), strongU8);

        float strongPct = slot_state.rumble_strong_intensity * 100.0f;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderFloat("##rumble_left_motor", &strongPct, 0.0f, 100.0f, "")) {
            slot_state.rumble_strong_intensity = std::clamp(strongPct / 100.0f, 0.0f, 1.0f);
        }

        ImGui::Spacing();

        // Slider 2: Right Motor (Light / High Freq) -> weak_magnitude (0..255)
        const uint8_t weakU8 =
            static_cast<uint8_t>(std::clamp(std::round(slot_state.rumble_weak_intensity * 255.0f), 0.0f, 255.0f));
        ImGui::TextColored(kTextColorWhite, "Right Motor (Light / High Freq)");
        ImGui::SameLine();
        DrawRightAlignedText(kTextColorCyan, "%.0f%% (%u/255)",
                             static_cast<double>(slot_state.rumble_weak_intensity * 100.0f), weakU8);

        float weakPct = slot_state.rumble_weak_intensity * 100.0f;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderFloat("##rumble_right_motor", &weakPct, 0.0f, 100.0f, "")) {
            slot_state.rumble_weak_intensity = std::clamp(weakPct / 100.0f, 0.0f, 1.0f);
        }

        ImGui::Spacing();

        // Slider 3: Duration (50 ms .. 5000 ms)
        const uint16_t durationMs =
            static_cast<uint16_t>(std::clamp(std::round(slot_state.rumble_duration_ms), 50.0f, 5000.0f));
        ImGui::TextColored(kTextColorWhite, "Duration");
        ImGui::SameLine();
        DrawRightAlignedText(kTextColorCyan, "%u ms (%.1fs)", durationMs, static_cast<double>(durationMs) / 1000.0);

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderFloat("##rumble_duration", &slot_state.rumble_duration_ms, 50.0f, 5000.0f, "")) {
            slot_state.rumble_duration_ms =
                std::clamp(std::round(slot_state.rumble_duration_ms / 10.0f) * 10.0f, 50.0f, 5000.0f);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // Bottom Action Buttons: "Test Rumble" (left half) and "Stop Rumble" (right half)
        const float availW = ImGui::GetContentRegionAvail().x;
        const float btnGap = 14.0f * s;
        const float btnHeight = 36.0f * s;
        const float halfBtnW = std::max(120.0f * s, (availW - btnGap) * 0.5f);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 18.0f * s);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        // 1. Test Rumble Button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.36f, 0.62f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.45f, 0.75f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.28f, 0.50f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.32f, 0.62f, 0.95f, 1.0f));
        if (ImGui::Button("    Test Rumble", ImVec2(halfBtnW, btnHeight)) && snap.has_rumble) {
            const uint8_t curWeak =
                static_cast<uint8_t>(std::clamp(std::round(slot_state.rumble_weak_intensity * 255.0f), 0.0f, 255.0f));
            const uint8_t curStrong =
                static_cast<uint8_t>(std::clamp(std::round(slot_state.rumble_strong_intensity * 255.0f), 0.0f, 255.0f));
            const uint16_t curDur =
                static_cast<uint16_t>(std::clamp(std::round(slot_state.rumble_duration_ms), 50.0f, 5000.0f));
            posix_imgui_request_rumble(slot, 0, curDur, curWeak, curStrong);
        }
        {
            // Draw small vector vibrator icon to the left of "Test Rumble"
            const ImVec2 bMin = ImGui::GetItemRectMin();
            const ImVec2 bMax = ImGui::GetItemRectMax();
            const float textW = ImGui::CalcTextSize("    Test Rumble").x;
            const float iconCx = (bMin.x + bMax.x - textW) * 0.5f + 6.0f * s;
            const float iconCy = (bMin.y + bMax.y) * 0.5f;
            const ImU32 iconCol = IM_COL32(235, 245, 255, 255);
            drawList->AddRect(ImVec2(iconCx - 4.0f * s, iconCy - 6.0f * s),
                              ImVec2(iconCx + 4.0f * s, iconCy + 6.0f * s), iconCol, 1.5f * s, 0, 1.5f * s);
            drawList->AddLine(ImVec2(iconCx - 7.0f * s, iconCy - 4.0f * s),
                              ImVec2(iconCx - 7.0f * s, iconCy + 4.0f * s), iconCol, 1.5f * s);
            drawList->AddLine(ImVec2(iconCx + 7.0f * s, iconCy - 4.0f * s),
                              ImVec2(iconCx + 7.0f * s, iconCy + 4.0f * s), iconCol, 1.5f * s);
        }
        ImGui::PopStyleColor(4);

        ImGui::SameLine(0.0f, btnGap);

        // 2. Stop Rumble Button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.14f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.14f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.14f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.38f, 0.28f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.38f, 0.38f, 1.0f));
        if (ImGui::Button("    Stop Rumble", ImVec2(halfBtnW, btnHeight)) && snap.has_rumble) {
            slot_state.trigger_rumble_active = false;
            posix_imgui_request_rumble(slot, 0, 0, 0, 0);
        }
        {
            // Draw small vector stop square to the left of "Stop Rumble"
            const ImVec2 bMin = ImGui::GetItemRectMin();
            const ImVec2 bMax = ImGui::GetItemRectMax();
            const float textW = ImGui::CalcTextSize("    Stop Rumble").x;
            const float iconCx = (bMin.x + bMax.x - textW) * 0.5f + 6.0f * s;
            const float iconCy = (bMin.y + bMax.y) * 0.5f;
            drawList->AddRectFilled(ImVec2(iconCx - 4.5f * s, iconCy - 4.5f * s),
                                    ImVec2(iconCx + 4.5f * s, iconCy + 4.5f * s), IM_COL32(245, 85, 85, 255), 1.5f * s);
        }
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // ========================================================================
    // Card 2: Preset Waveforms
    // ========================================================================
    if (ImGui::BeginChild("##rumble_presets_card", ImVec2(0.0f, 0.0f), kCardChildFlags, kCardWindowFlags)) {
        ImGui::TextColored(kTextColorWhite, "Preset Waveforms");
        ImGui::TextColored(kTextColorGrey,
                           "Quickly trigger pre-calibrated vibration patterns to verify dual-motor separation.");
        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f * s);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f * s, 6.0f * s));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.21f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.25f, 0.34f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.36f, 0.62f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.32f, 0.42f, 1.0f));

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        auto drawPlayTriangleOnLastButton = [&]() {
            const ImVec2 bMin = ImGui::GetItemRectMin();
            const ImVec2 bMax = ImGui::GetItemRectMax();
            const float cx = bMin.x + 14.0f * s;
            const float cy = (bMin.y + bMax.y) * 0.5f;
            drawList->AddTriangleFilled(ImVec2(cx - 3.0f * s, cy - 4.5f * s), ImVec2(cx - 3.0f * s, cy + 4.5f * s),
                                        ImVec2(cx + 4.5f * s, cy), IM_COL32(195, 205, 225, 255));
        };

        auto triggerPreset = [&](float durationMs, float strongNorm, float weakNorm) {
            slot_state.rumble_duration_ms = durationMs;
            slot_state.rumble_strong_intensity = strongNorm;
            slot_state.rumble_weak_intensity = weakNorm;
            if (snap.has_rumble) {
                const uint8_t weakVal = static_cast<uint8_t>(std::clamp(std::round(weakNorm * 255.0f), 0.0f, 255.0f));
                const uint8_t strongVal =
                    static_cast<uint8_t>(std::clamp(std::round(strongNorm * 255.0f), 0.0f, 255.0f));
                posix_imgui_request_rumble(slot, 0, static_cast<uint16_t>(durationMs), weakVal, strongVal);
            }
        };

        if (ImGui::Button("   Pulse (300ms)")) {
            triggerPreset(300.0f, 0.80f, 0.80f);
        }
        drawPlayTriangleOnLastButton();

        ImGui::SameLine(0.0f, 10.0f * s);
        if (ImGui::Button("   Heavy Rumble (1.5s)")) {
            triggerPreset(1500.0f, 1.00f, 0.30f);
        }
        drawPlayTriangleOnLastButton();

        ImGui::SameLine(0.0f, 10.0f * s);
        if (ImGui::Button("   Light Buzz (800ms)")) {
            triggerPreset(800.0f, 0.0f, 0.65f);
        }
        drawPlayTriangleOnLastButton();

        ImGui::SameLine(0.0f, 10.0f * s);
        if (ImGui::Button("Left Motor Only")) {
            triggerPreset(1000.0f, 1.00f, 0.0f);
        }

        ImGui::SameLine(0.0f, 10.0f * s);
        if (ImGui::Button("Right Motor Only")) {
            triggerPreset(1000.0f, 0.0f, 1.00f);
        }

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(3);
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // ========================================================================
    // Card 3: Trigger Rumble Mode
    // ========================================================================
    if (ImGui::BeginChild("##rumble_trigger_mode_card", ImVec2(0.0f, 0.0f), kCardChildFlags, kCardWindowFlags)) {
        if (ImGui::BeginTable("##trigger_rumble_table", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("##trigger_rumble_desc", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##trigger_rumble_toggle_col", ImGuiTableColumnFlags_WidthFixed, 64.0f * s);
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextColored(kTextColorWhite, "Trigger Rumble Mode");
            ImGui::TextColored(kTextColorGrey,
                               "Dynamically pulse haptics proportional to analog trigger pressure (LT/RT).");
            if (slot_state.trigger_rumble_enabled) {
                const uni_gamepad_t& gp = snap.controller.gamepad;
                const float l2Norm = NormalizeTriggerAxis(gp.brake, (gp.buttons & BUTTON_TRIGGER_L) != 0);
                const float r2Norm = NormalizeTriggerAxis(gp.throttle, (gp.buttons & BUTTON_TRIGGER_R) != 0);
                const uint8_t l2U8 = static_cast<uint8_t>(std::clamp(std::round(l2Norm * 255.0f), 0.0f, 255.0f));
                const uint8_t r2U8 = static_cast<uint8_t>(std::clamp(std::round(r2Norm * 255.0f), 0.0f, 255.0f));
                ImGui::TextColored(
                    kTextColorCyan,
                    "Live LT -> Left Motor: %3.0f%% (%3u/255)   |   RT -> Right Motor: %3.0f%% (%3u/255)",
                    static_cast<double>(l2Norm * 100.0f), l2U8, static_cast<double>(r2Norm * 100.0f), r2U8);
            }

            ImGui::TableNextColumn();
            ImGui::Dummy(ImVec2(0.0f, 8.0f * s));
            if (DrawToggleSwitch("##trigger_rumble_switch", &slot_state.trigger_rumble_enabled)) {
                if (!slot_state.trigger_rumble_enabled && slot_state.trigger_rumble_active) {
                    posix_imgui_request_rumble(slot, 0, 0, 0, 0);
                    slot_state.trigger_rumble_active = false;
                    slot_state.last_trigger_strong_u8 = 0;
                    slot_state.last_trigger_weak_u8 = 0;
                }
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(1);
}

void DemoScene::RenderPanel_MotionTab(int slot, const ControllerSnapshot& snap) {
    ControllerSlotUiState& slot_state = mSlots[static_cast<size_t>(slot)];
    const float s = UiScale();

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

    std::array<float, kMotionAxisCount> accelMps2{};
    std::array<float, kMotionAxisCount> accelG{};
    std::array<float, kMotionAxisCount> gyroRadS{};
    std::array<float, kMotionAxisCount> gyroDegS{};
    for (size_t axis = 0; axis < kMotionAxisCount; ++axis) {
        accelMps2[axis] = gp.accel[axis];
        accelG[axis] = accelMps2[axis] / kGravityMps2;
        gyroRadS[axis] = gp.gyro[axis];
        gyroDegS[axis] = gyroRadS[axis] * kRadToDeg;
    }

    constexpr std::array<const char*, kMotionAxisCount> kAxisLabels = {"X", "Y", "Z"};

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
            const float bullseyeRadius = 56.0f * s;
            const float bullseyeDiameter = bullseyeRadius * 2.0f;

            ImGui::SetCursorPosX(colStartX + std::max(0.0f, (colWidth - bullseyeDiameter) * 0.5f));
            const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(bullseyeDiameter, bullseyeDiameter));

            const ImVec2 center = canvasMin + ImVec2(bullseyeRadius, bullseyeRadius);
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // Subtle dark circular backdrop
            drawList->AddCircleFilled(center, bullseyeRadius, IM_COL32(22, 26, 34, 220), 64);

            // Three concentric target rings (r/3, 2r/3, r)
            drawList->AddCircle(center, bullseyeRadius * (1.0f / 3.0f), IM_COL32(95, 110, 130, 130), 48, 1.2f * s);
            drawList->AddCircle(center, bullseyeRadius * (2.0f / 3.0f), IM_COL32(95, 110, 130, 150), 64, 1.2f * s);
            drawList->AddCircle(center, bullseyeRadius, IM_COL32(150, 170, 195, 220), 64, 1.8f * s);

            // Crosshair lines through center
            drawList->AddLine(center - ImVec2(bullseyeRadius, 0.0f), center + ImVec2(bullseyeRadius, 0.0f),
                              IM_COL32(95, 110, 130, 150), 1.0f * s);
            drawList->AddLine(center - ImVec2(0.0f, bullseyeRadius), center + ImVec2(0.0f, bullseyeRadius),
                              IM_COL32(95, 110, 130, 150), 1.0f * s);

            // Map canonical Y-up horizontal tilt plane (X = right, Z = back) to 2D radar dot:
            float nx = accelG[0];
            float ny = accelG[2];
            const float mag = std::hypot(nx, ny);
            if (mag > 1.0f) {
                nx /= mag;
                ny /= mag;
            }

            const float maxDotOffset = bullseyeRadius - 7.0f * s;
            const ImVec2 dotPos = center + ImVec2(nx * maxDotOffset, -ny * maxDotOffset);

            drawList->AddLine(center, dotPos, IM_COL32(80, 200, 255, 110), 1.5f * s);
            drawList->AddCircleFilled(dotPos, 8.0f * s, IM_COL32(80, 215, 255, 70), 24);
            drawList->AddCircleFilled(dotPos, 5.5f * s, IM_COL32(90, 220, 255, 255), 24);
            drawList->AddCircle(dotPos, 5.5f * s, IM_COL32(230, 250, 255, 220), 24, 1.2f * s);

            ImGui::Spacing();
            if (ImGui::BeginTable("##accel_xyz_readouts", 3, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                for (size_t axis = 0; axis < kMotionAxisCount; ++axis) {
                    ImGui::TableNextColumn();
                    const float subStartX = ImGui::GetCursorPosX();
                    const float subWidth = ImGui::GetContentRegionAvail().x;
                    DrawTextCenteredInColumn(subStartX, subWidth, kTextColorGrey, "%s", kAxisLabels[axis]);
                    DrawTextCenteredInColumn(subStartX, subWidth, kTextColorWhite, "%+0.2f",
                                             static_cast<double>(accelMps2[axis]));
                    DrawTextCenteredInColumn(subStartX, subWidth, kTextColorGrey, "(%+0.2f g)",
                                             static_cast<double>(accelG[axis]));
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
            const float resetBtnWidth = 68.0f * s;
            const float availRight = ImGui::GetContentRegionAvail().x;
            if (availRight > resetBtnWidth) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availRight - resetBtnWidth);
            }
            if (ImGui::SmallButton(" Reset ")) {
                slot_state.gyro_angle_deg = {};
            }
            ImGui::TextColored(kTextColorGrey, "Rotation angle & speed (rad/s)");
            ImGui::Spacing();

            if (ImGui::BeginTable("##gyro_dials_table", 3, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextRow();
                for (size_t axis = 0; axis < kMotionAxisCount; ++axis) {
                    ImGui::TableNextColumn();
                    const float subStartX = ImGui::GetCursorPosX();
                    const float subWidth = ImGui::GetContentRegionAvail().x;
                    const float dialRadius = 46.0f * s;
                    const float dialDiameter = dialRadius * 2.0f;

                    ImGui::SetCursorPosX(subStartX + std::max(0.0f, (subWidth - dialDiameter) * 0.5f));
                    const ImVec2 dialMin = ImGui::GetCursorScreenPos();
                    ImGui::Dummy(ImVec2(dialDiameter, dialDiameter));

                    const ImVec2 dialCenter = dialMin + ImVec2(dialRadius, dialRadius);
                    ImDrawList* drawList = ImGui::GetWindowDrawList();

                    // Dial background and outer ring
                    drawList->AddCircleFilled(dialCenter, dialRadius, IM_COL32(22, 26, 34, 220), 64);
                    drawList->AddCircle(dialCenter, dialRadius, IM_COL32(150, 170, 195, 220), 64, 1.8f * s);

                    // Minor 3/6/9-o'clock reference ticks
                    drawList->AddLine(dialCenter + ImVec2(dialRadius - 5.0f * s, 0.0f),
                                      dialCenter + ImVec2(dialRadius, 0.0f), IM_COL32(95, 110, 130, 160), 1.2f * s);
                    drawList->AddLine(dialCenter - ImVec2(dialRadius, 0.0f),
                                      dialCenter - ImVec2(dialRadius - 5.0f * s, 0.0f), IM_COL32(95, 110, 130, 160),
                                      1.2f * s);
                    drawList->AddLine(dialCenter + ImVec2(0.0f, dialRadius - 5.0f * s),
                                      dialCenter + ImVec2(0.0f, dialRadius), IM_COL32(95, 110, 130, 160), 1.2f * s);

                    // Prominent 12-o'clock zero-degree reference tick
                    drawList->AddLine(dialCenter - ImVec2(0.0f, dialRadius),
                                      dialCenter - ImVec2(0.0f, dialRadius - 8.0f * s), IM_COL32(220, 230, 245, 240),
                                      2.0f * s);

                    // Rotating needle showing integrated rotation angle in degrees
                    const float angleDeg = slot_state.gyro_angle_deg[axis];
                    const float angleRad = angleDeg * kDegToRad;
                    const float needleLen = dialRadius - 8.0f * s;
                    const ImVec2 needleTip = dialCenter + ImVec2(std::sin(angleRad), -std::cos(angleRad)) * needleLen;

                    drawList->AddLine(dialCenter, needleTip, IM_COL32(90, 220, 255, 255), 2.4f * s);
                    drawList->AddCircleFilled(dialCenter, 4.0f * s, IM_COL32(220, 230, 245, 255), 16);

                    ImGui::Spacing();
                    DrawTextCenteredInColumn(subStartX, subWidth, kTextColorWhite, "%.0f\xC2\xB0",
                                             static_cast<double>(angleDeg));
                    DrawTextCenteredInColumn(subStartX, subWidth, kTextColorWhite, "%s  %+0.2f", kAxisLabels[axis],
                                             static_cast<double>(gyroRadS[axis]));
                    DrawTextCenteredInColumn(subStartX, subWidth, kTextColorGrey, "(%+0.1f\xC2\xB0/s)",
                                             static_cast<double>(gyroDegS[axis]));
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
    const int histOffset = static_cast<int>(slot_state.imu_history_offset);
    const float plotHeight = 55.0f * s;

    ImGui::Spacing();
    ImGui::TextColored(kTextColorCyan, "Live History Plots (240 samples)");
    if (ImGui::BeginTable("##imuplots", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Accelerometer (X / Y / Z, m/s\xC2\xB2)");
        ImGui::PlotLines("Accel X", slot_state.accel_history[0].data(), histCount, histOffset, nullptr, -20.0f, 20.0f,
                         ImVec2(0.0f, plotHeight));
        ImGui::PlotLines("Accel Y", slot_state.accel_history[1].data(), histCount, histOffset, nullptr, -20.0f, 20.0f,
                         ImVec2(0.0f, plotHeight));
        ImGui::PlotLines("Accel Z", slot_state.accel_history[2].data(), histCount, histOffset, nullptr, -20.0f, 20.0f,
                         ImVec2(0.0f, plotHeight));

        ImGui::TableNextColumn();
        ImGui::Text("Gyroscope (X / Y / Z, rad/s)");
        ImGui::PlotLines("Gyro X", slot_state.gyro_history[0].data(), histCount, histOffset, nullptr, -10.0f, 10.0f,
                         ImVec2(0.0f, plotHeight));
        ImGui::PlotLines("Gyro Y", slot_state.gyro_history[1].data(), histCount, histOffset, nullptr, -10.0f, 10.0f,
                         ImVec2(0.0f, plotHeight));
        ImGui::PlotLines("Gyro Z", slot_state.gyro_history[2].data(), histCount, histOffset, nullptr, -10.0f, 10.0f,
                         ImVec2(0.0f, plotHeight));
        ImGui::EndTable();
    }
}

void DemoScene::RenderPanel_LightsTab(int slot, const ControllerSnapshot& snap) {
    ControllerSlotUiState& slot_state = mSlots[static_cast<size_t>(slot)];
    const float s = UiScale();

    ImGui::Spacing();

    // ------------------------------------------------------------------------
    // 1. Player Indicator LEDs (set_player_leds)
    // ------------------------------------------------------------------------
    ImGui::TextColored(kTextColorCyan, "1. Player Indicator Lights (`set_player_leds`)");
    ImGui::Separator();
    if (snap.has_player_leds) {
        ImGui::SetNextItemWidth(180.0f * s);
        ImGui::SliderInt("Player Index (1..4)", &slot_state.player_led_index, 1, 4);
        ImGui::SameLine();
        if (ImGui::Button("Set Player Index Light")) {
            const uint8_t mask = static_cast<uint8_t>(1u << (slot_state.player_led_index - 1));
            for (size_t b = 0; b < slot_state.player_led_bits.size(); ++b) {
                slot_state.player_led_bits[b] = ((mask & (1u << b)) != 0);
            }
            posix_imgui_request_player_leds(slot, mask);
        }

        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Raw 4-Bit LED Mask:");
        ImGui::SameLine();
        static constexpr std::array<const char*, 4> kLedBitLabels = {
            "LED 1 (Bit 0)",
            "LED 2 (Bit 1)",
            "LED 3 (Bit 2)",
            "LED 4 (Bit 3)",
        };
        static_assert(kLedBitLabels.size() == std::tuple_size_v<decltype(slot_state.player_led_bits)>);

        uint8_t rawMask = 0;
        for (size_t b = 0; b < slot_state.player_led_bits.size(); ++b) {
            ImGui::Checkbox(kLedBitLabels[b], &slot_state.player_led_bits[b]);
            if (slot_state.player_led_bits[b]) {
                rawMask |= static_cast<uint8_t>(1u << b);
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("Set Raw LED Bitmask")) {
            posix_imgui_request_player_leds(slot, rawMask);
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(kTextColorGrey, "Quick Presets:");
        ImGui::SameLine();
        static constexpr std::array<const char*, 4> kSeatPresetLabels = {
            "Seat #1",
            "Seat #2",
            "Seat #3",
            "Seat #4",
        };
        for (int p = 1; p <= 4; ++p) {
            if (ImGui::Button(kSeatPresetLabels[static_cast<size_t>(p - 1)])) {
                slot_state.player_led_index = p;
                const uint8_t mask = static_cast<uint8_t>(1u << (p - 1));
                for (size_t b = 0; b < slot_state.player_led_bits.size(); ++b) {
                    slot_state.player_led_bits[b] = (static_cast<int>(b) == (p - 1));
                }
                posix_imgui_request_player_leds(slot, mask);
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("All Off (0x0)")) {
            slot_state.player_led_bits.fill(false);
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
        ImGui::SetNextItemWidth(260.0f * s);
        const bool colorChanged = ImGui::ColorEdit3("LightColor", slot_state.rgb_color.data());
        ImGui::SameLine();
        if (ImGui::Button("Set Light Color") || (colorChanged && slot_state.rgb_live_update)) {
            const uint8_t r =
                static_cast<uint8_t>(std::clamp(std::round(slot_state.rgb_color[0] * 255.0f), 0.0f, 255.0f));
            const uint8_t g =
                static_cast<uint8_t>(std::clamp(std::round(slot_state.rgb_color[1] * 255.0f), 0.0f, 255.0f));
            const uint8_t b =
                static_cast<uint8_t>(std::clamp(std::round(slot_state.rgb_color[2] * 255.0f), 0.0f, 255.0f));
            posix_imgui_request_lightbar_color(slot, r, g, b);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Live update on drag", &slot_state.rgb_live_update);

        struct ColorSwatch {
            const char* name;
            float r;
            float g;
            float b;
        };
        constexpr std::array<ColorSwatch, 6> kSwatches = {{
            {"PS Blue", 0.00f, 0.25f, 1.00f},
            {"Red", 1.00f, 0.00f, 0.00f},
            {"Green", 0.00f, 1.00f, 0.00f},
            {"Amber", 1.00f, 0.60f, 0.00f},
            {"White", 1.00f, 1.00f, 1.00f},
            {"Off", 0.00f, 0.00f, 0.00f},
        }};
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(kTextColorGrey, "Swatches:");
        ImGui::SameLine();
        for (size_t i = 0; i < kSwatches.size(); ++i) {
            if (i > 0) {
                ImGui::SameLine();
            }
            if (ImGui::Button(kSwatches[i].name)) {
                slot_state.rgb_color = {kSwatches[i].r, kSwatches[i].g, kSwatches[i].b};
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
    ImGui::SetNextItemWidth(220.0f * s);
    ImGui::SliderInt("LED Brightness (%)", &slot_state.brightness_percent, 0, 100, "%d%%");
    ImGui::SameLine();
    ImGui::Button("Set Brightness Light");
    ImGui::EndDisabled();
    ImGui::TextColored(kTextColorYellow,
                       "Note: UI placeholder — uni_hid_device_t does not yet expose a brightness LED setter in "
                       "libbluepad32.");
}
