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

    mLastImuTimestampUs[slot] = snap.last_report_timestamp_us;
    const size_t idx = mImuHistoryOffset[slot];
    for (size_t axis = 0; axis < kMotionAxisCount; ++axis) {
        mGyroHistory[slot][axis][idx] = static_cast<float>(snap.controller.gamepad.gyro[axis]);
        mAccelHistory[slot][axis][idx] = static_cast<float>(snap.controller.gamepad.accel[axis]);
    }
    mImuHistoryOffset[slot] = (idx + 1) % kImuHistoryLen;
}

void DemoScene::ClearImuHistory(int slot) {
    if (slot < 0 || slot >= kMaxControllers) {
        return;
    }
    std::memset(mGyroHistory[slot], 0, sizeof(mGyroHistory[slot]));
    std::memset(mAccelHistory[slot], 0, sizeof(mAccelHistory[slot]));
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

    // 2-Column Motion Table matching AGDK RenderMotionTableData
    if (ImGui::BeginTable("##motiontable", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Axis / Metric", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Accelerometer (raw / G)", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("Gyroscope (raw / deg/s)", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(kTextColorGrey, "Interval (ms)");
        ImGui::TableNextColumn();
        ImGui::Text("%4u ms", snap.report_delta_ms);
        ImGui::TableNextColumn();
        ImGui::Text("%4u ms", snap.report_delta_ms);

        const char* kAxisNames[3] = {"X Axis", "Y Axis", "Z Axis"};
        for (int axis = 0; axis < 3; ++axis) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(kTextColorGrey, "%s", kAxisNames[axis]);

            ImGui::TableNextColumn();
            ImGui::Text("%7d", gp.accel[axis]);
            ImGui::SameLine(90.0f);
            const float accelNorm = std::clamp((static_cast<float>(gp.accel[axis]) + 8192.0f) / 16384.0f, 0.0f, 1.0f);
            ImGui::ProgressBar(accelNorm, ImVec2(150.0f, 0.0f), "");

            ImGui::TableNextColumn();
            ImGui::Text("%7d", gp.gyro[axis]);
            ImGui::SameLine(90.0f);
            const float gyroNorm = std::clamp((static_cast<float>(gp.gyro[axis]) + 2048.0f) / 4096.0f, 0.0f, 1.0f);
            ImGui::ProgressBar(gyroNorm, ImVec2(150.0f, 0.0f), "");
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
