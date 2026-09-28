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

constexpr float kUiScaleMin = 0.50f;
constexpr float kUiScaleMax = 4.00f;
constexpr float kUiScaleStep = 0.25f;

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

}  // namespace

DemoScene::DemoScene()
    : mSnapshots{},
      mPrevConnected{false, false, false, false},
      mMostRecentConnectedSlot(-1),
      mCurrentControllerSlot(0),
      mActiveControllerPanelTab(0),
      mControllerPanelBaseX(0.0f),
      mControllerPanelBaseY(0.0f),
      mControllerPanelScale(1.25f),
      mFontScale(1.0f),
      mDontTrimDeadzone(false),
      mPreferencesActive(false),
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

void DemoScene::OnCreate() {
    ControllerUIData::LoadControllerUIData();
}

void DemoScene::OnDestroy() {
    ControllerUIData::UnloadControllerUIData();
}

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
    ImGui::TextColored(kTextColorGrey, "UI Scale: %.2fx | Deadzone: %s | %.1f FPS",
                       static_cast<double>(mControllerPanelScale), mDontTrimDeadzone ? "Raw (Untrimmed)" : "Trimmed",
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
    ImGui::Text("Controller UI scale:");
    ImGui::SameLine(180.0f);
    if (ImGui::Button(" - ##ui")) {
        mControllerPanelScale = std::max(kUiScaleMin, mControllerPanelScale - kUiScaleStep);
    }
    ImGui::SameLine();
    ImGui::Text("%2.2fx", static_cast<double>(mControllerPanelScale));
    ImGui::SameLine();
    if (ImGui::Button(" + ##ui")) {
        mControllerPanelScale = std::min(kUiScaleMax, mControllerPanelScale + kUiScaleStep);
    }
    ImGui::SameLine(0.0f, 16.0f);
    ImGui::SetNextItemWidth(180.0f);
    ImGui::SliderFloat("##ui_slider", &mControllerPanelScale, kUiScaleMin, kUiScaleMax, "%.2fx");

    ImGui::Spacing();
    ImGui::Checkbox("Raw deadzone (do not trim stick center deadzone to 0.0)", &mDontTrimDeadzone);
    ImGui::TextColored(kTextColorGrey, "  Bluepad32 AXIS_THRESHOLD = %d (out of [-512, 511] full scale)",
                       AXIS_THRESHOLD);

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

void DemoScene::RenderPanel_ControlsTab(int /*slot*/, const ControllerSnapshot& snap) {
    const uni_gamepad_t& gp = snap.controller.gamepad;

    ControllerUIData::ConfigureButtonLayout(snap.layout);

    mControllerPanelBaseX = ImGui::GetCursorPosX();
    mControllerPanelBaseY = ImGui::GetCursorPosY();

    ControllerUIPanelParams panelParams{mControllerPanelBaseX, mControllerPanelBaseY, mControllerPanelScale};

    const uint32_t buttonsMask = MapGamepadToUIButtonMask(gp);

    // 1. Render all enabled button & D-Pad sprites
    for (uint32_t index = UIBUTTON_A; index < UIBUTTON_COUNT; ++index) {
        const ControllerUIButtons buttonId = static_cast<ControllerUIButtons>(index);
        ControllerButtonInfo buttonInfo = ControllerUIData::getControllerButtonInfo(buttonId);
        buttonInfo.buttonState = (buttonsMask & buttonInfo.buttonMask) ? UIBUTTON_STATE_ACTIVE : UIBUTTON_STATE_IDLE;
        if (buttonInfo.enabled) {
            ControllerUIUtil::Button(panelParams, buttonId, buttonInfo);
        }
    }

    // 2. Render Left & Right Analog Thumbsticks
    const float leftX = NormalizeStickAxis(gp.axis_x, mDontTrimDeadzone);
    const float leftY = NormalizeStickAxis(gp.axis_y, mDontTrimDeadzone);
    const float rightX = NormalizeStickAxis(gp.axis_rx, mDontTrimDeadzone);
    const float rightY = NormalizeStickAxis(gp.axis_ry, mDontTrimDeadzone);

    ControllerUIStickStates leftStickState = UISTICK_STATE_IDLE;
    if ((buttonsMask & UI_BUTTON_MASK_L3) != 0) {
        leftStickState = UISTICK_STATE_DEPRESSED;
    } else if (std::abs(gp.axis_x) >= AXIS_THRESHOLD || std::abs(gp.axis_y) >= AXIS_THRESHOLD) {
        leftStickState = UISTICK_STATE_ACTIVE;
    }

    ControllerUIStickStates rightStickState = UISTICK_STATE_IDLE;
    if ((buttonsMask & UI_BUTTON_MASK_R3) != 0) {
        rightStickState = UISTICK_STATE_DEPRESSED;
    } else if (std::abs(gp.axis_rx) >= AXIS_THRESHOLD || std::abs(gp.axis_ry) >= AXIS_THRESHOLD) {
        rightStickState = UISTICK_STATE_ACTIVE;
    }

    const float stickScale = ControllerUIData::getStickScale();
    const ImVec2 leftStickValues(leftX * stickScale, leftY * stickScale);
    const ImVec2 rightStickValues(rightX * stickScale, rightY * stickScale);

    ControllerUIUtil::Thumbstick(panelParams, ControllerUIData::getStickPosition(true), leftStickValues,
                                 leftStickState);
    ControllerUIUtil::Thumbstick(panelParams, ControllerUIData::getStickPosition(false), rightStickValues,
                                 rightStickState);

    // 3. Render L1/L2/R1/R2 Trigger Fill Bars
    const float l1Val = (buttonsMask & UI_BUTTON_MASK_L1) ? 1.0f : 0.0f;
    const float l2Val = NormalizeTriggerAxis(gp.brake, (gp.buttons & BUTTON_TRIGGER_L) != 0);
    const float r1Val = (buttonsMask & UI_BUTTON_MASK_R1) ? 1.0f : 0.0f;
    const float r2Val = NormalizeTriggerAxis(gp.throttle, (gp.buttons & BUTTON_TRIGGER_R) != 0);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ControllerUIUtil::TriggerBar(panelParams, draw_list, UIBUTTON_L1, l1Val, 0.0f);
    ControllerUIUtil::TriggerBar(panelParams, draw_list, UIBUTTON_L2, l2Val, 0.0f);
    ControllerUIUtil::TriggerBar(panelParams, draw_list, UIBUTTON_R1, r1Val, 0.0f);
    ControllerUIUtil::TriggerBar(panelParams, draw_list, UIBUTTON_R2, r2Val, 0.0f);

    // 4. Why this explicit `SetCursorPos` is mandatory:
    // `ControllerUIUtil::Thumbstick()` positions the right stick sprite via
    // `ImGui::SetCursorPos(basePos + rightStickValues)`. Without re-anchoring the cursor
    // at a fixed Y offset below the 250-unit-tall canvas (`mControllerPanelBaseY + 250 * scale`),
    // vertical deflection of the right thumbstick would cause all live telemetry rows below
    // to bob up and down dynamically.
    ImGui::SetCursorPos(ImVec2(mControllerPanelBaseX + 16.0f, mControllerPanelBaseY + 250.0f * mControllerPanelScale));
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::Separator();

    // 5. Post-canvas live numeric telemetry bar
    ImGui::Text("Left Stick:  X=%5d (%+6.2f)  Y=%5d (%+6.2f)  L3=%d", gp.axis_x, static_cast<double>(leftX), gp.axis_y,
                static_cast<double>(leftY), (buttonsMask & UI_BUTTON_MASK_L3) ? 1 : 0);
    ImGui::SameLine(0.0f, 32.0f);
    ImGui::Text("Right Stick: RX=%5d (%+6.2f)  RY=%5d (%+6.2f)  R3=%d", gp.axis_rx, static_cast<double>(rightX),
                gp.axis_ry, static_cast<double>(rightY), (buttonsMask & UI_BUTTON_MASK_R3) ? 1 : 0);

    ImGui::Text("Triggers:    Brake (L2)=%4d (%5.1f%%)  Throttle (R2)=%4d (%5.1f%%)", gp.brake,
                static_cast<double>(l2Val * 100.0f), gp.throttle, static_cast<double>(r2Val * 100.0f));
    ImGui::SameLine(0.0f, 32.0f);
    ImGui::Text("Bitmasks: DPad=0x%02X  Buttons=0x%04X  Misc=0x%02X", gp.dpad, gp.buttons, gp.misc_buttons);

    const bool capturePressed = (buttonsMask & UI_BUTTON_MASK_CAPTURE) != 0;
    ImGui::SameLine(0.0f, 24.0f);
    ImGui::TextColored(capturePressed ? kTextColorGreen : kTextColorGrey, "[Capture/Mute: %s]",
                       capturePressed ? "ACTIVE" : "Idle");
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
