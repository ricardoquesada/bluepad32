// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "controller/uni_gamepad.h"

#include <stdbool.h>
#include <stddef.h>

#include "controller/uni_controller_type.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_log.h"

static uni_gamepad_mappings_t map;
static uni_gamepad_mappings_type_t mappings_type;

static struct {
    uni_controller_type_t type;
    const char* name;
} controller_names[] = {
    {CONTROLLER_TYPE_UnknownSteamController, "Unknown Steam"},
    {CONTROLLER_TYPE_SteamController, "Steam"},
    {CONTROLLER_TYPE_SteamControllerV2, "Steam V2"},

    {CONTROLLER_TYPE_XBox360Controller, "XBox 360"},
    {CONTROLLER_TYPE_XBoxOneController, "XBox One"},
    {CONTROLLER_TYPE_PS3Controller, "DualShock 3"},
    {CONTROLLER_TYPE_PS4Controller, "DualShock 4"},
    {CONTROLLER_TYPE_WiiController, "Wii"},
    {CONTROLLER_TYPE_AppleController, "Apple"},
    {CONTROLLER_TYPE_AndroidController, "Android"},
    {CONTROLLER_TYPE_SwitchProController, "Switch Pro"},
    {CONTROLLER_TYPE_SwitchJoyConLeft, "Switch JoyCon Left"},
    {CONTROLLER_TYPE_SwitchJoyConRight, "Switch JoyCon Right"},
    {CONTROLLER_TYPE_SwitchJoyConPair, "Switch JoyCon Pair"},
    {CONTROLLER_TYPE_SwitchInputOnlyController, "Switch Input Only"},
    {CONTROLLER_TYPE_MobileTouch, "Mobile Touch"},
    {CONTROLLER_TYPE_XInputSwitchController, "XInput Switch"},
    {CONTROLLER_TYPE_PS5Controller, "DualSense"},

    {CONTROLLER_TYPE_iCadeController, "iCade"},
    {CONTROLLER_TYPE_SmartTVRemoteController, "Smart TV Remote"},
    {CONTROLLER_TYPE_8BitdoController, "8BitDo"},
    {CONTROLLER_TYPE_GenericController, "Generic"},
    {CONTROLLER_TYPE_NimbusController, "Nimbus"},
    {CONTROLLER_TYPE_OUYAController, "OUYA"},
    {CONTROLLER_TYPE_PSMoveController, "PS Move"},
    {CONTROLLER_TYPE_AtariJoystick, "Atari Joystick"},

    {CONTROLLER_TYPE_GenericKeyboard, "Keyboard"},
    {CONTROLLER_TYPE_GenericMouse, "Mouse"},
};

// extern
const uni_gamepad_mappings_t GAMEPAD_DEFAULT_MAPPINGS = {
    .dpad_up = UNI_GAMEPAD_MAPPINGS_DPAD_UP,
    .dpad_down = UNI_GAMEPAD_MAPPINGS_DPAD_DOWN,
    .dpad_left = UNI_GAMEPAD_MAPPINGS_DPAD_LEFT,
    .dpad_right = UNI_GAMEPAD_MAPPINGS_DPAD_RIGHT,

    .button_a = UNI_GAMEPAD_MAPPINGS_BUTTON_A,
    .button_b = UNI_GAMEPAD_MAPPINGS_BUTTON_B,
    .button_x = UNI_GAMEPAD_MAPPINGS_BUTTON_X,
    .button_y = UNI_GAMEPAD_MAPPINGS_BUTTON_Y,

    .button_shoulder_l = UNI_GAMEPAD_MAPPINGS_BUTTON_SHOULDER_L,
    .button_shoulder_r = UNI_GAMEPAD_MAPPINGS_BUTTON_SHOULDER_R,
    .button_trigger_l = UNI_GAMEPAD_MAPPINGS_BUTTON_TRIGGER_L,
    .button_trigger_r = UNI_GAMEPAD_MAPPINGS_BUTTON_TRIGGER_R,
    .button_thumb_l = UNI_GAMEPAD_MAPPINGS_BUTTON_THUMB_L,
    .button_thumb_r = UNI_GAMEPAD_MAPPINGS_BUTTON_THUMB_R,

    .brake = UNI_GAMEPAD_MAPPINGS_PEDAL_BRAKE,
    .throttle = UNI_GAMEPAD_MAPPINGS_PEDAL_THROTTLE,

    .axis_x = UNI_GAMEPAD_MAPPINGS_AXIS_X,
    .axis_y = UNI_GAMEPAD_MAPPINGS_AXIS_Y,
    .axis_rx = UNI_GAMEPAD_MAPPINGS_AXIS_RX,
    .axis_ry = UNI_GAMEPAD_MAPPINGS_AXIS_RY,

    .misc_button_select = UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SELECT,
    .misc_button_start = UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_START,
    .misc_button_system = UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SYSTEM,
    .misc_button_capture = UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_CAPTURE,
};

const int AXIS_NORMALIZE_RANGE = 1024;  // 10-bit resolution (1024)
const int AXIS_THRESHOLD = (1024 / 8);

static int32_t get_mappings_value_for_axis(uni_gamepad_mappings_axis_t axis_type, const uni_gamepad_t* gp) {
    switch (axis_type) {
        case UNI_GAMEPAD_MAPPINGS_AXIS_X:
            return gp->axis_x;
        case UNI_GAMEPAD_MAPPINGS_AXIS_Y:
            return gp->axis_y;
        case UNI_GAMEPAD_MAPPINGS_AXIS_RX:
            return gp->axis_rx;
        case UNI_GAMEPAD_MAPPINGS_AXIS_RY:
            return gp->axis_ry;
        default:
            break;
    }
    loge("get_mappings_value_for_axis(): should not happen\n");
    return -1;
}

static int32_t get_mappings_value_for_pedal(uni_gamepad_mappings_pedal_t pedal_type, const uni_gamepad_t* gp) {
    switch (pedal_type) {
        case UNI_GAMEPAD_MAPPINGS_PEDAL_THROTTLE:
            return gp->throttle;
        case UNI_GAMEPAD_MAPPINGS_PEDAL_BRAKE:
            return gp->brake;
        default:
            break;
    }
    loge("get_mappings_value_for_pedal(): should not happen\n");
    return -1;
}

uni_gamepad_t uni_gamepad_remap(const uni_gamepad_t* gp) {
    uni_gamepad_t new_gp = {0};

    // Quick return if using default mappings
    if (mappings_type == UNI_GAMEPAD_MAPPINGS_TYPE_XBOX)
        return *gp;

    if (mappings_type == UNI_GAMEPAD_MAPPINGS_TYPE_SWITCH) {
        new_gp = *gp;

        // Turn off original buttons
        new_gp.buttons &= ~(BUTTON_A | BUTTON_B | BUTTON_X | BUTTON_Y);

        // Invert A with B, and X with Y
        if (gp->buttons & BUTTON_A)
            new_gp.buttons |= BUTTON_B;
        if (gp->buttons & BUTTON_B)
            new_gp.buttons |= BUTTON_A;
        if (gp->buttons & BUTTON_X)
            new_gp.buttons |= BUTTON_Y;
        if (gp->buttons & BUTTON_Y)
            new_gp.buttons |= BUTTON_X;
        return new_gp;
    }

    // else UNI_GAMEPAD_MAPPINGS_TYPE_CUSTOM

    if (gp->buttons & BUTTON_A)
        new_gp.buttons |= BIT(map.button_a);
    if (gp->buttons & BUTTON_B)
        new_gp.buttons |= BIT(map.button_b);
    if (gp->buttons & BUTTON_X)
        new_gp.buttons |= BIT(map.button_x);
    if (gp->buttons & BUTTON_Y)
        new_gp.buttons |= BIT(map.button_y);
    if (gp->buttons & BUTTON_SHOULDER_L)
        new_gp.buttons |= BIT(map.button_shoulder_l);
    if (gp->buttons & BUTTON_SHOULDER_R)
        new_gp.buttons |= BIT(map.button_shoulder_r);
    if (gp->buttons & BUTTON_TRIGGER_L)
        new_gp.buttons |= BIT(map.button_trigger_l);
    if (gp->buttons & BUTTON_TRIGGER_R)
        new_gp.buttons |= BIT(map.button_trigger_r);
    if (gp->buttons & BUTTON_THUMB_L)
        new_gp.buttons |= BIT(map.button_thumb_l);
    if (gp->buttons & BUTTON_THUMB_R)
        new_gp.buttons |= BIT(map.button_thumb_r);

    if (gp->dpad & DPAD_UP)
        new_gp.dpad |= BIT(map.dpad_up);
    if (gp->dpad & DPAD_DOWN)
        new_gp.dpad |= BIT(map.dpad_down);
    if (gp->dpad & DPAD_LEFT)
        new_gp.dpad |= BIT(map.dpad_left);
    if (gp->dpad & DPAD_RIGHT)
        new_gp.dpad |= BIT(map.dpad_right);

    if (gp->misc_buttons & MISC_BUTTON_SYSTEM)
        new_gp.misc_buttons |= BIT(map.misc_button_system);
    if (gp->misc_buttons & MISC_BUTTON_SELECT)
        new_gp.misc_buttons |= BIT(map.misc_button_select);
    if (gp->misc_buttons & MISC_BUTTON_START)
        new_gp.misc_buttons |= BIT(map.misc_button_start);
    if (gp->misc_buttons & MISC_BUTTON_CAPTURE)
        new_gp.misc_buttons |= BIT(map.misc_button_capture);

    new_gp.axis_x = get_mappings_value_for_axis(map.axis_x, gp);
    if (map.axis_x_inverted)
        new_gp.axis_x = -new_gp.axis_x;
    new_gp.axis_y = get_mappings_value_for_axis(map.axis_y, gp);
    if (map.axis_y_inverted)
        new_gp.axis_y = -new_gp.axis_y;
    new_gp.axis_rx = get_mappings_value_for_axis(map.axis_rx, gp);
    if (map.axis_rx_inverted)
        new_gp.axis_rx = -new_gp.axis_rx;
    new_gp.axis_ry = get_mappings_value_for_axis(map.axis_ry, gp);
    if (map.axis_ry_inverted)
        new_gp.axis_ry = -new_gp.axis_ry;

    new_gp.brake = get_mappings_value_for_pedal(map.brake, gp);
    new_gp.throttle = get_mappings_value_for_pedal(map.throttle, gp);

    return new_gp;
}

void uni_gamepad_set_mappings(const uni_gamepad_mappings_t* mappings) {
    mappings_type = UNI_GAMEPAD_MAPPINGS_TYPE_CUSTOM;
    map = *mappings;
}

void uni_gamepad_set_mappings_type(uni_gamepad_mappings_type_t type) {
    mappings_type = type;
}

uni_gamepad_mappings_type_t uni_gamepad_get_mappings_type(void) {
    return mappings_type;
}

void uni_gamepad_dump(const uni_gamepad_t* gp) {
    // Don't add "\n"
    logi(
        "dpad=0x%02x, x=%4d, y=%4d, rx=%4d, ry=%4d, brake=%4d, throttle=%4d, buttons=0x%04x, misc=0x%02x, "
        "gyro=%7d,%7d,%7d accel=%7d,%7d,%7d",
        gp->dpad,                                          // dpad
        gp->axis_x, gp->axis_y, gp->axis_rx, gp->axis_ry,  // axis
        gp->brake, gp->throttle,                           // brake/gas
        gp->buttons, gp->misc_buttons,                     // buttons
        gp->gyro[0], gp->gyro[1], gp->gyro[2],             // gyro
        gp->accel[0], gp->accel[1], gp->accel[2]           // accel
    );
}

const char* uni_gamepad_get_model_name(int type) {
    for (size_t i = 0; i < ARRAY_SIZE(controller_names); i++) {
        if (controller_names[i].type == type)
            return controller_names[i].name;
    }
    return "Unknown";
}
