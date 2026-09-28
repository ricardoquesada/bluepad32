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
 * @file controllerui_data.cpp
 * @brief Implementation of 2D gamepad sprite tables, layout switching, and axis/button mapping.
 *
 * Architectural Role & Threading Context:
 *   - Executed on Thread 1 (the GLFW/ImGui UI thread) during frame rendering and texture
 *     lifecycle management, and invoked directly in `test_posix_imgui` for headless
 *     verification of layout and axis normalization invariants.
 *   - Defines unscaled 2D canvas coordinates for all face buttons, D-Pad directions,
 *     shoulder/trigger buttons (`L1/L2/R1/R2`), system buttons (`Select/Mode/Start`), and
 *     dual analog thumbsticks.
 */

#include "controllerui_data.h"

#include <cmath>
#include <cstdlib>

namespace {

bool uiDataInitialized = false;

UITextureInfo buttonTextures[UIBUTTON_COUNT] = {};
UITextureInfo stickTextures = {};
UITextureInfo stickRegionTexture = {};

const ControllerUIButtonDefinition buttonDefinitions[UIBUTTON_COUNT] = {
    {"gamecontroller/Button_A_Active.png", "gamecontroller/Button_A_Idle.png"},
    {"gamecontroller/Button_B_Active.png", "gamecontroller/Button_B_Idle.png"},
    {"gamecontroller/Button_X_Active.png", "gamecontroller/Button_X_Idle.png"},
    {"gamecontroller/Button_Y_Active.png", "gamecontroller/Button_Y_Idle.png"},
    {"gamecontroller/Button_ShapeX_Active.png", "gamecontroller/Button_ShapeX_Idle.png"},
    {"gamecontroller/Button_Circle_Active.png", "gamecontroller/Button_Circle_Idle.png"},
    {"gamecontroller/Button_Square_Active.png", "gamecontroller/Button_Square_Idle.png"},
    {"gamecontroller/Button_Triangle_Active.png", "gamecontroller/Button_Triangle_Idle.png"},
    {"gamecontroller/Button_L1_Active.png", "gamecontroller/Button_L1_Idle.png"},
    {"gamecontroller/Button_L2_Active.png", "gamecontroller/Button_L2_Idle.png"},
    {"gamecontroller/Button_R1_Active.png", "gamecontroller/Button_R1_Idle.png"},
    {"gamecontroller/Button_R2_Active.png", "gamecontroller/Button_R2_Idle.png"},
    {"gamecontroller/Button_Mode_Active.png", "gamecontroller/Button_Mode_Idle.png"},
    {"gamecontroller/Button_Select_Active.png", "gamecontroller/Button_Select_Idle.png"},
    {"gamecontroller/Button_Start_Active.png", "gamecontroller/Button_Start_Idle.png"},
    {"gamecontroller/Button_DPad_Up_Active.png", "gamecontroller/Button_DPad_Up_Idle.png"},
    {"gamecontroller/Button_DPad_Left_Active.png", "gamecontroller/Button_DPad_Left_Idle.png"},
    {"gamecontroller/Button_DPad_Down_Active.png", "gamecontroller/Button_DPad_Down_Idle.png"},
    {"gamecontroller/Button_DPad_Right_Active.png", "gamecontroller/Button_DPad_Right_Idle.png"},
};

const ControllerStickUIDefinition stickDefinition = {
    "gamecontroller/Thumbstick_Active.png",
    "gamecontroller/Thumbstick_Depressed.png",
    "gamecontroller/Thumbstick_Idle.png",
};

const char* const stickRegionAssetName = "gamecontroller/Thumbstick_Region.png";

const char* const kAllSpriteAssets[kTotalControllerSpriteAssets] = {
    "gamecontroller/Button_A_Active.png",        "gamecontroller/Button_A_Idle.png",
    "gamecontroller/Button_Active.png",          "gamecontroller/Button_B_Active.png",
    "gamecontroller/Button_B_Idle.png",          "gamecontroller/Button_Circle_Active.png",
    "gamecontroller/Button_Circle_Idle.png",     "gamecontroller/Button_DPad_Down_Active.png",
    "gamecontroller/Button_DPad_Down_Idle.png",  "gamecontroller/Button_DPad_Left_Active.png",
    "gamecontroller/Button_DPad_Left_Idle.png",  "gamecontroller/Button_DPad_Right_Active.png",
    "gamecontroller/Button_DPad_Right_Idle.png", "gamecontroller/Button_DPad_Up_Active.png",
    "gamecontroller/Button_DPad_Up_Idle.png",    "gamecontroller/Button_Idle.png",
    "gamecontroller/Button_L1_Active.png",       "gamecontroller/Button_L1_Idle.png",
    "gamecontroller/Button_L2_Active.png",       "gamecontroller/Button_L2_Idle.png",
    "gamecontroller/Button_Mode_Active.png",     "gamecontroller/Button_Mode_Idle.png",
    "gamecontroller/Button_R1_Active.png",       "gamecontroller/Button_R1_Idle.png",
    "gamecontroller/Button_R2_Active.png",       "gamecontroller/Button_R2_Idle.png",
    "gamecontroller/Button_Select_Active.png",   "gamecontroller/Button_Select_Idle.png",
    "gamecontroller/Button_ShapeX_Active.png",   "gamecontroller/Button_ShapeX_Idle.png",
    "gamecontroller/Button_Square_Active.png",   "gamecontroller/Button_Square_Idle.png",
    "gamecontroller/Button_Start_Active.png",    "gamecontroller/Button_Start_Idle.png",
    "gamecontroller/Button_Triangle_Active.png", "gamecontroller/Button_Triangle_Idle.png",
    "gamecontroller/Button_X_Active.png",        "gamecontroller/Button_X_Idle.png",
    "gamecontroller/Button_Y_Active.png",        "gamecontroller/Button_Y_Idle.png",
    "gamecontroller/Thumbstick_Active.png",      "gamecontroller/Thumbstick_Depressed.png",
    "gamecontroller/Thumbstick_Idle.png",        "gamecontroller/Thumbstick_Region.png",
};

// UI Layout constants
constexpr float ROW_QUAD_AND_STICKS_BASE_Y = 160.0f;
constexpr float ROW_BUTTON_SYSTEM_BASE_Y = ROW_QUAD_AND_STICKS_BASE_Y - 128.0f;
constexpr float ROW_BUTTON_TRIGGER_BASE_Y = ROW_QUAD_AND_STICKS_BASE_Y - 112.0f;

constexpr float STICK_LEFT_X = 128.0f;
constexpr float STICK_BETWEEN_WIDTH = 224.0f;
constexpr float STICK_RIGHT_X = STICK_LEFT_X + STICK_BETWEEN_WIDTH;
constexpr float STICK_BASE_Y = ROW_QUAD_AND_STICKS_BASE_Y;
constexpr float STICK_SCALE = 64.0f;

constexpr float BUTTON_QUAD_BASE_X = STICK_LEFT_X + 384.0f;
constexpr float BUTTON_QUAD_BASE_Y = ROW_QUAD_AND_STICKS_BASE_Y;
constexpr float BUTTON_QUAD_X_ADJUST = 32.0f;
constexpr float BUTTON_QUAD_Y_ADJUST = 32.0f;

constexpr float BUTTON_TRIGGER_LEFT_X = 48.0f;
constexpr float BUTTON_TRIGGER_RIGHT_X = 720.0f;
constexpr float BUTTON_TRIGGER_ADJUST_Y = 32.0f;

constexpr float TRIGGER_BAR_ADJUST_X = 54.0f;
constexpr float TRIGGER_BAR_ADJUST_Y = -16.0f;
constexpr float TRIGGER_BAR_WIDTH = 128.0f;
constexpr float TRIGGER_BAR_HEIGHT = 32.0f;

constexpr float BUTTON_SYSTEM_BASE_X = 380.0f;
constexpr float BUTTON_SYSTEM_X_ADJUST = 96.0f;

constexpr float DPAD_BASE_X = BUTTON_QUAD_BASE_X + 144.0f;
constexpr float DPAD_BASE_Y = ROW_QUAD_AND_STICKS_BASE_Y;
constexpr float DPAD_CENTER_ADJUST = 32.0f;

// Button/dpad element constants
constexpr float BUTTON_QUAD_TOP_X = BUTTON_QUAD_BASE_X;
constexpr float BUTTON_QUAD_TOP_Y = BUTTON_QUAD_BASE_Y - BUTTON_QUAD_Y_ADJUST;
constexpr float BUTTON_QUAD_LEFT_X = BUTTON_QUAD_BASE_X - BUTTON_QUAD_X_ADJUST;
constexpr float BUTTON_QUAD_LEFT_Y = BUTTON_QUAD_BASE_Y;
constexpr float BUTTON_QUAD_BOTTOM_X = BUTTON_QUAD_BASE_X;
constexpr float BUTTON_QUAD_BOTTOM_Y = BUTTON_QUAD_BASE_Y + BUTTON_QUAD_Y_ADJUST;
constexpr float BUTTON_QUAD_RIGHT_X = BUTTON_QUAD_BASE_X + BUTTON_QUAD_X_ADJUST;
constexpr float BUTTON_QUAD_RIGHT_Y = BUTTON_QUAD_BASE_Y;

constexpr float BUTTON_L1_X = BUTTON_TRIGGER_LEFT_X;
constexpr float BUTTON_L1_Y = ROW_BUTTON_TRIGGER_BASE_Y;
constexpr float BUTTON_L2_X = BUTTON_TRIGGER_LEFT_X;
constexpr float BUTTON_L2_Y = ROW_BUTTON_TRIGGER_BASE_Y - BUTTON_TRIGGER_ADJUST_Y;

constexpr float BUTTON_R1_X = BUTTON_TRIGGER_RIGHT_X;
constexpr float BUTTON_R1_Y = ROW_BUTTON_TRIGGER_BASE_Y;
constexpr float BUTTON_R2_X = BUTTON_TRIGGER_RIGHT_X;
constexpr float BUTTON_R2_Y = ROW_BUTTON_TRIGGER_BASE_Y - BUTTON_TRIGGER_ADJUST_Y;

constexpr float BUTTON_SELECT_X = BUTTON_SYSTEM_BASE_X - BUTTON_SYSTEM_X_ADJUST;
constexpr float BUTTON_SELECT_Y = ROW_BUTTON_SYSTEM_BASE_Y;
constexpr float BUTTON_MODE_X = BUTTON_SYSTEM_BASE_X;
constexpr float BUTTON_MODE_Y = ROW_BUTTON_SYSTEM_BASE_Y;
constexpr float BUTTON_START_X = BUTTON_SYSTEM_BASE_X + BUTTON_SYSTEM_X_ADJUST;
constexpr float BUTTON_START_Y = ROW_BUTTON_SYSTEM_BASE_Y;

constexpr float DPAD_UP_X = DPAD_BASE_X;
constexpr float DPAD_UP_Y = DPAD_BASE_Y - DPAD_CENTER_ADJUST;
constexpr float DPAD_LEFT_X = DPAD_BASE_X - DPAD_CENTER_ADJUST;
constexpr float DPAD_LEFT_Y = DPAD_BASE_Y;
constexpr float DPAD_DOWN_X = DPAD_BASE_X;
constexpr float DPAD_DOWN_Y = DPAD_BASE_Y + DPAD_CENTER_ADJUST;
constexpr float DPAD_RIGHT_X = DPAD_BASE_X + DPAD_CENTER_ADJUST;
constexpr float DPAD_RIGHT_Y = DPAD_BASE_Y;

// Arcade stick variants
constexpr float AS_DPAD_BASE_X = STICK_LEFT_X + 144.0f;
constexpr float AS_DPAD_BASE_Y = ROW_QUAD_AND_STICKS_BASE_Y;

constexpr float AS_DPAD_UP_X = AS_DPAD_BASE_X;
constexpr float AS_DPAD_UP_Y = AS_DPAD_BASE_Y - DPAD_CENTER_ADJUST;
constexpr float AS_DPAD_LEFT_X = AS_DPAD_BASE_X - DPAD_CENTER_ADJUST;
constexpr float AS_DPAD_LEFT_Y = AS_DPAD_BASE_Y;
constexpr float AS_DPAD_DOWN_X = AS_DPAD_BASE_X;
constexpr float AS_DPAD_DOWN_Y = AS_DPAD_BASE_Y + DPAD_CENTER_ADJUST;
constexpr float AS_DPAD_RIGHT_X = AS_DPAD_BASE_X + DPAD_CENTER_ADJUST;
constexpr float AS_DPAD_RIGHT_Y = AS_DPAD_BASE_Y;

constexpr float AS_BUTTONS_BASE_X = AS_DPAD_BASE_X + 96.0f;
constexpr float AS_BUTTONS_BASE_Y = ROW_QUAD_AND_STICKS_BASE_Y + 32.0f;

constexpr float AS_BUTTON_A_X = AS_BUTTONS_BASE_X;
constexpr float AS_BUTTON_A_Y = AS_BUTTONS_BASE_Y;
constexpr float AS_BUTTON_B_X = AS_BUTTON_A_X + 40.0f;
constexpr float AS_BUTTON_B_Y = AS_BUTTON_A_Y - 16.0f;
constexpr float AS_BUTTON_R2_X = AS_BUTTON_B_X + 56.0f;
constexpr float AS_BUTTON_R2_Y = AS_BUTTON_B_Y;
constexpr float AS_BUTTON_L2_X = AS_BUTTON_R2_X + 64.0f;
constexpr float AS_BUTTON_L2_Y = AS_BUTTON_R2_Y;

constexpr float AS_BUTTON_X_X = AS_BUTTONS_BASE_X + 16.0f;
constexpr float AS_BUTTON_X_Y = AS_BUTTONS_BASE_Y - 72.0f;
constexpr float AS_BUTTON_Y_X = AS_BUTTON_X_X + 40.0f;
constexpr float AS_BUTTON_Y_Y = AS_BUTTON_X_Y - 16.0f;
constexpr float AS_BUTTON_R1_X = AS_BUTTON_Y_X + 56.0f;
constexpr float AS_BUTTON_R1_Y = AS_BUTTON_Y_Y;
constexpr float AS_BUTTON_L1_X = AS_BUTTON_R1_X + 64.0f;
constexpr float AS_BUTTON_L1_Y = AS_BUTTON_R1_Y;

ControllerButtonInfo controllerButtonInfo[UIBUTTON_COUNT] = {
    {{BUTTON_QUAD_BOTTOM_X, BUTTON_QUAD_BOTTOM_Y}, UI_BUTTON_MASK_A, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_QUAD_RIGHT_X, BUTTON_QUAD_RIGHT_Y}, UI_BUTTON_MASK_B, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_QUAD_LEFT_X, BUTTON_QUAD_LEFT_Y}, UI_BUTTON_MASK_X, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_QUAD_TOP_X, BUTTON_QUAD_TOP_Y}, UI_BUTTON_MASK_Y, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_QUAD_BOTTOM_X, BUTTON_QUAD_BOTTOM_Y}, UI_BUTTON_MASK_A, UIBUTTON_STATE_IDLE, false},
    {{BUTTON_QUAD_RIGHT_X, BUTTON_QUAD_RIGHT_Y}, UI_BUTTON_MASK_B, UIBUTTON_STATE_IDLE, false},
    {{BUTTON_QUAD_LEFT_X, BUTTON_QUAD_LEFT_Y}, UI_BUTTON_MASK_X, UIBUTTON_STATE_IDLE, false},
    {{BUTTON_QUAD_TOP_X, BUTTON_QUAD_TOP_Y}, UI_BUTTON_MASK_Y, UIBUTTON_STATE_IDLE, false},
    {{BUTTON_L1_X, BUTTON_L1_Y}, UI_BUTTON_MASK_L1, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_L2_X, BUTTON_L2_Y}, UI_BUTTON_MASK_L2, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_R1_X, BUTTON_R1_Y}, UI_BUTTON_MASK_R1, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_R2_X, BUTTON_R2_Y}, UI_BUTTON_MASK_R2, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_MODE_X, BUTTON_MODE_Y}, UI_BUTTON_MASK_SYSTEM, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_SELECT_X, BUTTON_SELECT_Y}, UI_BUTTON_MASK_SELECT, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_START_X, BUTTON_START_Y}, UI_BUTTON_MASK_START, UIBUTTON_STATE_IDLE, true},
    {{DPAD_UP_X, DPAD_UP_Y}, UI_BUTTON_MASK_DPAD_UP, UIBUTTON_STATE_IDLE, true},
    {{DPAD_LEFT_X, DPAD_LEFT_Y}, UI_BUTTON_MASK_DPAD_LEFT, UIBUTTON_STATE_IDLE, true},
    {{DPAD_DOWN_X, DPAD_DOWN_Y}, UI_BUTTON_MASK_DPAD_DOWN, UIBUTTON_STATE_IDLE, true},
    {{DPAD_RIGHT_X, DPAD_RIGHT_Y}, UI_BUTTON_MASK_DPAD_RIGHT, UIBUTTON_STATE_IDLE, true},
};

ControllerButtonInfo controllerButtonInfo_ArcadeStick[UIBUTTON_COUNT] = {
    {{AS_BUTTON_A_X, AS_BUTTON_A_Y}, UI_BUTTON_MASK_A, UIBUTTON_STATE_IDLE, true},
    {{AS_BUTTON_B_X, AS_BUTTON_B_Y}, UI_BUTTON_MASK_B, UIBUTTON_STATE_IDLE, true},
    {{AS_BUTTON_X_X, AS_BUTTON_X_Y}, UI_BUTTON_MASK_X, UIBUTTON_STATE_IDLE, true},
    {{AS_BUTTON_Y_X, AS_BUTTON_Y_Y}, UI_BUTTON_MASK_Y, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_QUAD_BOTTOM_X, BUTTON_QUAD_BOTTOM_Y}, UI_BUTTON_MASK_A, UIBUTTON_STATE_IDLE, false},
    {{BUTTON_QUAD_RIGHT_X, BUTTON_QUAD_RIGHT_Y}, UI_BUTTON_MASK_B, UIBUTTON_STATE_IDLE, false},
    {{BUTTON_QUAD_LEFT_X, BUTTON_QUAD_LEFT_Y}, UI_BUTTON_MASK_X, UIBUTTON_STATE_IDLE, false},
    {{BUTTON_QUAD_TOP_X, BUTTON_QUAD_TOP_Y}, UI_BUTTON_MASK_Y, UIBUTTON_STATE_IDLE, false},
    {{AS_BUTTON_L1_X, AS_BUTTON_L1_Y}, UI_BUTTON_MASK_L1, UIBUTTON_STATE_IDLE, true},
    {{AS_BUTTON_L2_X, AS_BUTTON_L2_Y}, UI_BUTTON_MASK_L2, UIBUTTON_STATE_IDLE, true},
    {{AS_BUTTON_R1_X, AS_BUTTON_R1_Y}, UI_BUTTON_MASK_R1, UIBUTTON_STATE_IDLE, true},
    {{AS_BUTTON_R2_X, AS_BUTTON_R2_Y}, UI_BUTTON_MASK_R2, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_MODE_X, BUTTON_MODE_Y}, UI_BUTTON_MASK_SYSTEM, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_SELECT_X, BUTTON_SELECT_Y}, UI_BUTTON_MASK_SELECT, UIBUTTON_STATE_IDLE, true},
    {{BUTTON_START_X, BUTTON_START_Y}, UI_BUTTON_MASK_START, UIBUTTON_STATE_IDLE, true},
    {{AS_DPAD_UP_X, AS_DPAD_UP_Y}, UI_BUTTON_MASK_DPAD_UP, UIBUTTON_STATE_IDLE, true},
    {{AS_DPAD_LEFT_X, AS_DPAD_LEFT_Y}, UI_BUTTON_MASK_DPAD_LEFT, UIBUTTON_STATE_IDLE, true},
    {{AS_DPAD_DOWN_X, AS_DPAD_DOWN_Y}, UI_BUTTON_MASK_DPAD_DOWN, UIBUTTON_STATE_IDLE, true},
    {{AS_DPAD_RIGHT_X, AS_DPAD_RIGHT_Y}, UI_BUTTON_MASK_DPAD_RIGHT, UIBUTTON_STATE_IDLE, true},
};

}  // namespace

uint32_t MapGamepadToUIButtonMask(const uni_gamepad_t& gp) {
    uint32_t mask = 0;

    if (gp.dpad & DPAD_UP) {
        mask |= UI_BUTTON_MASK_DPAD_UP;
    }
    if (gp.dpad & DPAD_LEFT) {
        mask |= UI_BUTTON_MASK_DPAD_LEFT;
    }
    if (gp.dpad & DPAD_DOWN) {
        mask |= UI_BUTTON_MASK_DPAD_DOWN;
    }
    if (gp.dpad & DPAD_RIGHT) {
        mask |= UI_BUTTON_MASK_DPAD_RIGHT;
    }

    if (gp.buttons & BUTTON_A) {
        mask |= UI_BUTTON_MASK_A;
    }
    if (gp.buttons & BUTTON_B) {
        mask |= UI_BUTTON_MASK_B;
    }
    if (gp.buttons & BUTTON_X) {
        mask |= UI_BUTTON_MASK_X;
    }
    if (gp.buttons & BUTTON_Y) {
        mask |= UI_BUTTON_MASK_Y;
    }
    if (gp.buttons & BUTTON_SHOULDER_L) {
        mask |= UI_BUTTON_MASK_L1;
    }
    if ((gp.buttons & BUTTON_TRIGGER_L) || gp.brake > 0) {
        mask |= UI_BUTTON_MASK_L2;
    }
    if (gp.buttons & BUTTON_THUMB_L) {
        mask |= UI_BUTTON_MASK_L3;
    }
    if (gp.buttons & BUTTON_SHOULDER_R) {
        mask |= UI_BUTTON_MASK_R1;
    }
    if ((gp.buttons & BUTTON_TRIGGER_R) || gp.throttle > 0) {
        mask |= UI_BUTTON_MASK_R2;
    }
    if (gp.buttons & BUTTON_THUMB_R) {
        mask |= UI_BUTTON_MASK_R3;
    }

    if (gp.misc_buttons & MISC_BUTTON_SELECT) {
        mask |= UI_BUTTON_MASK_SELECT;
    }
    if (gp.misc_buttons & MISC_BUTTON_START) {
        mask |= UI_BUTTON_MASK_START;
    }
    if (gp.misc_buttons & MISC_BUTTON_SYSTEM) {
        mask |= UI_BUTTON_MASK_SYSTEM;
    }
    if (gp.misc_buttons & MISC_BUTTON_CAPTURE) {
        mask |= UI_BUTTON_MASK_CAPTURE;
    }

    return mask;
}

float NormalizeStickAxis(int32_t raw_axis, bool dont_trim_deadzone) {
    if (!dont_trim_deadzone && std::abs(raw_axis) < AXIS_THRESHOLD) {
        return 0.0f;
    }
    float normalized = static_cast<float>(raw_axis) / 512.0f;
    if (normalized < -1.0f) {
        normalized = -1.0f;
    } else if (normalized > 1.0f) {
        normalized = 1.0f;
    }
    return normalized;
}

float NormalizeTriggerAxis(int32_t raw_trigger, bool digital_pressed) {
    if (raw_trigger > 0) {
        float normalized = static_cast<float>(raw_trigger) / 1023.0f;
        if (normalized < 0.0f) {
            normalized = 0.0f;
        } else if (normalized > 1.0f) {
            normalized = 1.0f;
        }
        return normalized;
    }
    return digital_pressed ? 1.0f : 0.0f;
}

void ControllerUIData::ConfigureButtonLayout(ControllerLayoutType layout) {
    const bool usingShapes = (layout == CONTROLLER_LAYOUT_SHAPES);
    const bool reverseButtons = (layout == CONTROLLER_LAYOUT_REVERSE);

    controllerButtonInfo[UIBUTTON_A].enabled = !usingShapes;
    controllerButtonInfo[UIBUTTON_B].enabled = !usingShapes;
    controllerButtonInfo[UIBUTTON_X].enabled = !usingShapes;
    controllerButtonInfo[UIBUTTON_Y].enabled = !usingShapes;
    controllerButtonInfo[UIBUTTON_CROSS].enabled = usingShapes;
    controllerButtonInfo[UIBUTTON_CIRCLE].enabled = usingShapes;
    controllerButtonInfo[UIBUTTON_SQUARE].enabled = usingShapes;
    controllerButtonInfo[UIBUTTON_TRIANGLE].enabled = usingShapes;

    if (reverseButtons) {
        // Why both `basePosition` AND `buttonMask` must be swapped for Nintendo Switch:
        //   Bluepad32 (`uni_hid_parser_switch.c` with `UNI_GAMEPAD_MAPPINGS_TYPE_XBOX`)
        //   normalizes gamepad buttons by physical cardinal position rather than label:
        //     - Physical South ('B' on Switch) sets `BUTTON_A` (`UI_BUTTON_MASK_A`).
        //     - Physical East  ('A' on Switch) sets `BUTTON_B` (`UI_BUTTON_MASK_B`).
        //     - Physical West  ('Y' on Switch) sets `BUTTON_X` (`UI_BUTTON_MASK_X`).
        //     - Physical North ('X' on Switch) sets `BUTTON_Y` (`UI_BUTTON_MASK_Y`).
        //   Therefore, when we place the 'B' sprite (`UIBUTTON_B`) at South and the 'A'
        //   sprite (`UIBUTTON_A`) at East, we must also bind `UIBUTTON_B` to
        //   `UI_BUTTON_MASK_A` and `UIBUTTON_A` to `UI_BUTTON_MASK_B` (and likewise for
        //   `UIBUTTON_Y` and `UIBUTTON_X`) so pressing physical 'B' illuminates the 'B'
        //   sprite at South.
        controllerButtonInfo[UIBUTTON_A].basePosition = getButtonQuadPosition(UIBUTTON_DPAD_RIGHT);
        controllerButtonInfo[UIBUTTON_A].buttonMask = UI_BUTTON_MASK_B;

        controllerButtonInfo[UIBUTTON_B].basePosition = getButtonQuadPosition(UIBUTTON_DPAD_DOWN);
        controllerButtonInfo[UIBUTTON_B].buttonMask = UI_BUTTON_MASK_A;

        controllerButtonInfo[UIBUTTON_X].basePosition = getButtonQuadPosition(UIBUTTON_DPAD_UP);
        controllerButtonInfo[UIBUTTON_X].buttonMask = UI_BUTTON_MASK_Y;

        controllerButtonInfo[UIBUTTON_Y].basePosition = getButtonQuadPosition(UIBUTTON_DPAD_LEFT);
        controllerButtonInfo[UIBUTTON_Y].buttonMask = UI_BUTTON_MASK_X;
    } else {
        // Restore both standard Xbox/PlayStation quad positions and standard button masks.
        controllerButtonInfo[UIBUTTON_A].basePosition = getButtonQuadPosition(UIBUTTON_DPAD_DOWN);
        controllerButtonInfo[UIBUTTON_A].buttonMask = UI_BUTTON_MASK_A;

        controllerButtonInfo[UIBUTTON_B].basePosition = getButtonQuadPosition(UIBUTTON_DPAD_RIGHT);
        controllerButtonInfo[UIBUTTON_B].buttonMask = UI_BUTTON_MASK_B;

        controllerButtonInfo[UIBUTTON_X].basePosition = getButtonQuadPosition(UIBUTTON_DPAD_LEFT);
        controllerButtonInfo[UIBUTTON_X].buttonMask = UI_BUTTON_MASK_X;

        controllerButtonInfo[UIBUTTON_Y].basePosition = getButtonQuadPosition(UIBUTTON_DPAD_UP);
        controllerButtonInfo[UIBUTTON_Y].buttonMask = UI_BUTTON_MASK_Y;
    }
}

ImVec2 ControllerUIData::getButtonQuadPosition(const ControllerUIButtons dpadButton) {
    switch (dpadButton) {
        case UIBUTTON_DPAD_UP:
            return ImVec2(BUTTON_QUAD_TOP_X, BUTTON_QUAD_TOP_Y);
        case UIBUTTON_DPAD_LEFT:
            return ImVec2(BUTTON_QUAD_LEFT_X, BUTTON_QUAD_LEFT_Y);
        case UIBUTTON_DPAD_DOWN:
            return ImVec2(BUTTON_QUAD_BOTTOM_X, BUTTON_QUAD_BOTTOM_Y);
        case UIBUTTON_DPAD_RIGHT:
            return ImVec2(BUTTON_QUAD_RIGHT_X, BUTTON_QUAD_RIGHT_Y);
        default:
            break;
    }
    return ImVec2(0.0f, 0.0f);
}

void ControllerUIData::LoadControllerUIData() {
    if (!uiDataInitialized) {
        for (int index = 0; index < UIBUTTON_COUNT; ++index) {
            buttonTextures[index].textureHandles[UIBUTTON_STATE_ACTIVE] = TextureAssetLoader::loadTextureAsset(
                buttonDefinitions[index].assetName_Active, &buttonTextures[index].textureWidth,
                &buttonTextures[index].textureHeight);
            buttonTextures[index].textureHandles[UIBUTTON_STATE_IDLE] =
                TextureAssetLoader::loadTextureAsset(buttonDefinitions[index].assetName_Idle, nullptr, nullptr);
        }

        stickTextures.textureHandles[UISTICK_STATE_ACTIVE] = TextureAssetLoader::loadTextureAsset(
            stickDefinition.assetName_Active, &stickTextures.textureWidth, &stickTextures.textureHeight);
        stickTextures.textureHandles[UISTICK_STATE_DEPRESSED] =
            TextureAssetLoader::loadTextureAsset(stickDefinition.assetName_Depressed, nullptr, nullptr);
        stickTextures.textureHandles[UISTICK_STATE_IDLE] =
            TextureAssetLoader::loadTextureAsset(stickDefinition.assetName_Idle, nullptr, nullptr);

        stickRegionTexture.textureHandles[0] = TextureAssetLoader::loadTextureAsset(
            stickRegionAssetName, &stickRegionTexture.textureWidth, &stickRegionTexture.textureHeight);
        uiDataInitialized = true;
    }
}

void ControllerUIData::UnloadControllerUIData() {
    for (int index = 0; index < UIBUTTON_COUNT; ++index) {
        TextureAssetLoader::unloadTextureAsset(buttonTextures[index].textureHandles[UIBUTTON_STATE_ACTIVE]);
        TextureAssetLoader::unloadTextureAsset(buttonTextures[index].textureHandles[UIBUTTON_STATE_IDLE]);
    }

    TextureAssetLoader::unloadTextureAsset(stickTextures.textureHandles[UISTICK_STATE_ACTIVE]);
    TextureAssetLoader::unloadTextureAsset(stickTextures.textureHandles[UISTICK_STATE_DEPRESSED]);
    TextureAssetLoader::unloadTextureAsset(stickTextures.textureHandles[UISTICK_STATE_IDLE]);

    TextureAssetLoader::unloadTextureAsset(stickRegionTexture.textureHandles[0]);
    uiDataInitialized = false;
}

const UITextureInfo& ControllerUIData::getUIButtonTextures(const ControllerUIButtons uiButton) {
    return buttonTextures[uiButton];
}

const UITextureInfo& ControllerUIData::getUIStickTextures() {
    return stickTextures;
}

const UITextureInfo& ControllerUIData::getUIStickRegionTexture() {
    return stickRegionTexture;
}

ControllerButtonInfo& ControllerUIData::getControllerButtonInfo(const ControllerUIButtons uiButton) {
    return controllerButtonInfo[uiButton];
}

ControllerButtonInfo& ControllerUIData::getControllerButtonInfo_ArcadeStick(const ControllerUIButtons uiButton) {
    return controllerButtonInfo_ArcadeStick[uiButton];
}

ImVec2 ControllerUIData::getStickPosition(bool isLeftStick) {
    if (isLeftStick) {
        return ImVec2(STICK_LEFT_X, STICK_BASE_Y);
    }
    return ImVec2(STICK_RIGHT_X, STICK_BASE_Y);
}

float ControllerUIData::getStickScale() {
    return STICK_SCALE;
}

void ControllerUIData::getTriggerRectExtents(const ControllerUIButtons uiButton, ImVec2* rectMin, ImVec2* rectMax) {
    if (rectMin != nullptr && rectMax != nullptr) {
        if (uiButton == UIBUTTON_L1) {
            rectMin->x = BUTTON_L1_X + TRIGGER_BAR_ADJUST_X;
            rectMin->y = BUTTON_L1_Y + TRIGGER_BAR_ADJUST_Y;
            rectMax->x = BUTTON_L1_X + TRIGGER_BAR_ADJUST_X + TRIGGER_BAR_WIDTH;
            rectMax->y = BUTTON_L1_Y + TRIGGER_BAR_ADJUST_Y + TRIGGER_BAR_HEIGHT;
        } else if (uiButton == UIBUTTON_L2) {
            rectMin->x = BUTTON_L2_X + TRIGGER_BAR_ADJUST_X;
            rectMin->y = BUTTON_L2_Y + TRIGGER_BAR_ADJUST_Y;
            rectMax->x = BUTTON_L2_X + TRIGGER_BAR_ADJUST_X + TRIGGER_BAR_WIDTH;
            rectMax->y = BUTTON_L2_Y + TRIGGER_BAR_ADJUST_Y + TRIGGER_BAR_HEIGHT;
        } else if (uiButton == UIBUTTON_R1) {
            rectMin->x = BUTTON_R1_X - TRIGGER_BAR_ADJUST_X - TRIGGER_BAR_WIDTH;
            rectMin->y = BUTTON_R1_Y + TRIGGER_BAR_ADJUST_Y;
            rectMax->x = BUTTON_R1_X - TRIGGER_BAR_ADJUST_X;
            rectMax->y = BUTTON_R1_Y + TRIGGER_BAR_ADJUST_Y + TRIGGER_BAR_HEIGHT;
        } else if (uiButton == UIBUTTON_R2) {
            rectMin->x = BUTTON_R2_X - TRIGGER_BAR_ADJUST_X - TRIGGER_BAR_WIDTH;
            rectMin->y = BUTTON_R2_Y + TRIGGER_BAR_ADJUST_Y;
            rectMax->x = BUTTON_R2_X - TRIGGER_BAR_ADJUST_X;
            rectMax->y = BUTTON_R2_Y + TRIGGER_BAR_ADJUST_Y + TRIGGER_BAR_HEIGHT;
        } else {
            rectMin->x = 0.0f;
            rectMin->y = 0.0f;
            rectMax->x = 0.0f;
            rectMax->y = 0.0f;
        }
    }
}

const char* const* ControllerUIData::getAllSpriteAssetNames(size_t* out_count) {
    if (out_count != nullptr) {
        *out_count = kTotalControllerSpriteAssets;
    }
    return kAllSpriteAssets;
}
