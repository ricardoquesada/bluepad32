// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_GAMEPAD_H
#define UNI_GAMEPAD_H

#include <stdbool.h>
#include <stdint.h>

#include "uni_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file uni_gamepad.h
 * @brief Standardized virtual gamepad state (`uni_gamepad_t`) and button/axis remapping matrix
 * (`uni_gamepad_mappings_t`).
 *
 * Defines the canonical hardware-agnostic virtual gamepad model populated by all Bluepad32 HID
 * parsers and consumed by target platform drivers, together with the remapping engine that
 * transforms physical controller inputs before delivery to the active platform:
 * - **Preset Layouts (`uni_gamepad_mappings_type_t`):** Supports the default Xbox layout
 *   (`UNI_GAMEPAD_MAPPINGS_TYPE_XBOX`), the Nintendo Switch face-button swap
 *   (`UNI_GAMEPAD_MAPPINGS_TYPE_SWITCH`), and a user-defined 28-byte remapping matrix
 *   (`UNI_GAMEPAD_MAPPINGS_TYPE_CUSTOM`).
 * - **28-Byte Wire Contract (`uni_gamepad_mappings_t`):** Consists of 28 `uint8_t` fields
 *   (`sizeof(uni_gamepad_mappings_t) == 28`) shared directly with the BLE GATT configuration
 *   service (`AC06` 29-byte atomic write and `AC0F` 28-byte read/write).
 * - **Static `.data` Initialization (`UNI_GAMEPAD_DEFAULT_MAPPINGS_INIT`):** Ensures both the
 *   public `GAMEPAD_DEFAULT_MAPPINGS` constant and the internal static `map` table in
 *   `uni_gamepad.c` are initialized to the Xbox identity mapping at compile time rather than
 *   starting zero-initialized in `.bss`.
 */

extern const int AXIS_NORMALIZE_RANGE;
extern const int AXIS_THRESHOLD;

/**
 * @brief Active controller button/axis mapping mode exposed via GATT characteristic `AC06`.
 */
typedef enum {
    UNI_GAMEPAD_MAPPINGS_TYPE_XBOX,   /**< `0`: Default Xbox layout — A (South), B (East), X (West), Y (North). */
    UNI_GAMEPAD_MAPPINGS_TYPE_SWITCH, /**< `1`: Nintendo Switch layout — A (East), B (South), X (North), Y (West). */
    UNI_GAMEPAD_MAPPINGS_TYPE_CUSTOM, /**< `2`: User-defined 28-byte `uni_gamepad_mappings_t` remapping matrix. */

    UNI_GAMEPAD_MAPPINGS_TYPE_COUNT, /**< Number of valid mapping preset types (`3`). */
} uni_gamepad_mappings_type_t;

/**
 * @brief Virtual D-pad bit-index target (`0..3`) used in `uni_gamepad_mappings_t` offsets `0..3`.
 *
 * @note **C Enum vs. Struct Field Ordering Asymmetry:**
 * The enum constants are ordered `UP (0)`, `DOWN (1)`, `RIGHT (2)`, `LEFT (3)`, whereas
 * the struct fields in `uni_gamepad_mappings_t` at wire offsets `0..3` are ordered
 * `dpad_up`, `dpad_down`, `dpad_left`, `dpad_right`. Consequently, the default Xbox identity
 * wire bytes at offsets `0..3` are `{0, 1, 3, 2}`.
 */
typedef enum {
    UNI_GAMEPAD_MAPPINGS_DPAD_UP,    /**< `0`: Emits `DPAD_UP` (`BIT(0)`). */
    UNI_GAMEPAD_MAPPINGS_DPAD_DOWN,  /**< `1`: Emits `DPAD_DOWN` (`BIT(1)`). */
    UNI_GAMEPAD_MAPPINGS_DPAD_RIGHT, /**< `2`: Emits `DPAD_RIGHT` (`BIT(2)`). */
    UNI_GAMEPAD_MAPPINGS_DPAD_LEFT,  /**< `3`: Emits `DPAD_LEFT` (`BIT(3)`). */

    UNI_GAMEPAD_MAPPINGS_DPAD_COUNT, /**< Domain upper bound (`4`) enforced by `uni_gamepad_mappings_is_valid()`. */
} uni_gamepad_mappings_dpad_t;

/**
 * @brief Virtual face, shoulder, trigger, and thumbstick button bit-index target (`0..9`)
 * used in `uni_gamepad_mappings_t` offsets `4..13`.
 */
typedef enum {
    UNI_GAMEPAD_MAPPINGS_BUTTON_A,          /**< `0`: Emits `BUTTON_A` (`BIT(0)`, South). */
    UNI_GAMEPAD_MAPPINGS_BUTTON_B,          /**< `1`: Emits `BUTTON_B` (`BIT(1)`, East). */
    UNI_GAMEPAD_MAPPINGS_BUTTON_X,          /**< `2`: Emits `BUTTON_X` (`BIT(2)`, West). */
    UNI_GAMEPAD_MAPPINGS_BUTTON_Y,          /**< `3`: Emits `BUTTON_Y` (`BIT(3)`, North). */
    UNI_GAMEPAD_MAPPINGS_BUTTON_SHOULDER_L, /**< `4`: Emits `BUTTON_SHOULDER_L` (`BIT(4)`, L1). */
    UNI_GAMEPAD_MAPPINGS_BUTTON_SHOULDER_R, /**< `5`: Emits `BUTTON_SHOULDER_R` (`BIT(5)`, R1). */
    UNI_GAMEPAD_MAPPINGS_BUTTON_TRIGGER_L,  /**< `6`: Emits `BUTTON_TRIGGER_L` (`BIT(6)`, L2 digital). */
    UNI_GAMEPAD_MAPPINGS_BUTTON_TRIGGER_R,  /**< `7`: Emits `BUTTON_TRIGGER_R` (`BIT(7)`, R2 digital). */
    UNI_GAMEPAD_MAPPINGS_BUTTON_THUMB_L,    /**< `8`: Emits `BUTTON_THUMB_L` (`BIT(8)`, L3). */
    UNI_GAMEPAD_MAPPINGS_BUTTON_THUMB_R,    /**< `9`: Emits `BUTTON_THUMB_R` (`BIT(9)`, R3). */

    UNI_GAMEPAD_MAPPINGS_BUTTON_COUNT, /**< Domain upper bound (`10`) enforced by `uni_gamepad_mappings_is_valid()`. */
} uni_gamepad_mappings_button_t;

/**
 * @brief Virtual auxiliary/system button bit-index target (`0..3`) used in
 * `uni_gamepad_mappings_t` offsets `14..17`.
 *
 * @note **C Enum vs. Struct Field Ordering Asymmetry:**
 * The enum constants are ordered `SYSTEM (0)`, `SELECT (1)`, `START (2)`, `CAPTURE (3)`,
 * whereas the struct fields in `uni_gamepad_mappings_t` at wire offsets `14..17` are ordered
 * `misc_button_select`, `misc_button_start`, `misc_button_system`, `misc_button_capture`.
 * Consequently, the default Xbox identity wire bytes at offsets `14..17` are `{1, 2, 0, 3}`.
 */
typedef enum {
    UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SYSTEM,  /**< `0`: Emits `MISC_BUTTON_SYSTEM` (`BIT(0)`). */
    UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SELECT,  /**< `1`: Emits `MISC_BUTTON_SELECT` (`BIT(1)`). */
    UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_START,   /**< `2`: Emits `MISC_BUTTON_START` (`BIT(2)`). */
    UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_CAPTURE, /**< `3`: Emits `MISC_BUTTON_CAPTURE` (`BIT(3)`). */

    UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_COUNT, /**< Domain upper bound (`4`) enforced by `uni_gamepad_mappings_is_valid()`.
                                             */
} uni_gamepad_mappings_misc_button_t;

/**
 * @brief Physical analog stick axis source (`0..3`) selected for a virtual stick axis in
 * `uni_gamepad_mappings_t` offsets `18..21`.
 */
typedef enum {
    UNI_GAMEPAD_MAPPINGS_AXIS_X,  /**< `0`: Samples physical Left Stick X (`gp->axis_x`). */
    UNI_GAMEPAD_MAPPINGS_AXIS_Y,  /**< `1`: Samples physical Left Stick Y (`gp->axis_y`). */
    UNI_GAMEPAD_MAPPINGS_AXIS_RX, /**< `2`: Samples physical Right Stick X (`gp->axis_rx`). */
    UNI_GAMEPAD_MAPPINGS_AXIS_RY, /**< `3`: Samples physical Right Stick Y (`gp->axis_ry`). */

    UNI_GAMEPAD_MAPPINGS_AXIS_COUNT, /**< Domain upper bound (`4`) enforced by `uni_gamepad_mappings_is_valid()`. */
} uni_gamepad_mappings_axis_t;

/**
 * @brief Physical analog pedal/trigger source (`0..1`) selected for virtual `brake` or `throttle`
 * in `uni_gamepad_mappings_t` offsets `26..27`.
 */
typedef enum {
    UNI_GAMEPAD_MAPPINGS_PEDAL_BRAKE,    /**< `0`: Samples physical brake/L2 analog input (`gp->brake`). */
    UNI_GAMEPAD_MAPPINGS_PEDAL_THROTTLE, /**< `1`: Samples physical throttle/R2 analog input (`gp->throttle`). */

    UNI_GAMEPAD_MAPPINGS_PEDAL_COUNT, /**< Domain upper bound (`2`) enforced by `uni_gamepad_mappings_is_valid()`. */
} uni_gamepad_mappings_pedal_t;

// DPAD constants.
enum {
    DPAD_UP = BIT(UNI_GAMEPAD_MAPPINGS_DPAD_UP),
    DPAD_DOWN = BIT(UNI_GAMEPAD_MAPPINGS_DPAD_DOWN),
    DPAD_RIGHT = BIT(UNI_GAMEPAD_MAPPINGS_DPAD_RIGHT),
    DPAD_LEFT = BIT(UNI_GAMEPAD_MAPPINGS_DPAD_LEFT),
};

// BUTTON_ are the main gamepad buttons, like X, Y, A, B, etc.
enum {
    BUTTON_A = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_A),
    BUTTON_B = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_B),
    BUTTON_X = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_X),
    BUTTON_Y = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_Y),
    BUTTON_SHOULDER_L = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_SHOULDER_L),
    BUTTON_SHOULDER_R = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_SHOULDER_R),
    BUTTON_TRIGGER_L = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_TRIGGER_L),
    BUTTON_TRIGGER_R = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_TRIGGER_R),
    BUTTON_THUMB_L = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_THUMB_L),
    BUTTON_THUMB_R = BIT(UNI_GAMEPAD_MAPPINGS_BUTTON_THUMB_R),
};

// MISC_BUTTONS_ are buttons that are usually not used in the game, but are
// helpers like "select", "start", "system", "mute", etc.
enum {
    MISC_BUTTON_SYSTEM = BIT(UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SYSTEM),    // AKA: PS, Xbox, etc.
    MISC_BUTTON_SELECT = BIT(UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SELECT),    // AKA: Select, Share, Create, -
    MISC_BUTTON_START = BIT(UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_START),      // AKA: Start, Options, +
    MISC_BUTTON_CAPTURE = BIT(UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_CAPTURE),  // AKA: Mute, Capture, Share

    // Deprecated
    MISC_BUTTON_BACK = MISC_BUTTON_SELECT,
    MISC_BUTTON_HOME = MISC_BUTTON_START,
};

// Represents which "seat" the gamepad is using. Multiple gamepads can be
// connected at the same time, and the "seat" is the ID of each gamepad.
// In the Legacy firmware it was possible for a gamepad to take more than one
// seat, but since the v2.0 it might not be needed anymore.
// TODO: Investigate if this really needs to be a "bit".
typedef enum {
    GAMEPAD_SEAT_NONE = 0,
    GAMEPAD_SEAT_A = BIT(0),
    GAMEPAD_SEAT_B = BIT(1),
    GAMEPAD_SEAT_C = BIT(2),
    GAMEPAD_SEAT_D = BIT(3),

    // Masks
    GAMEPAD_SEAT_AB_MASK = (GAMEPAD_SEAT_A | GAMEPAD_SEAT_B),
} uni_gamepad_seat_t;

// Standard gravity in m/s^2 and degrees-to-radians conversion factor for IMU normalization.
// All IMU-capable HID parsers scale their raw or factory-calibrated sensor counts into
// physical SI units (m/s^2 for linear acceleration and rad/s for angular velocity)
// within a canonical right-handed Y-up coordinate frame:
//   - X ([0]): Right (+accel when tilted right-side down) / Pitch (+gyro around X)
//   - Y ([1]): Up (+9.80665 m/s^2 normal reaction when resting flat face-up) / Yaw (+gyro around Y)
//   - Z ([2]): Back/Toward Player (+accel when tilted nose-up) / Roll (+gyro around Z)
#define UNI_STANDARD_GRAVITY 9.80665f
#define UNI_DEG_TO_RAD 0.017453292519943295f

// uni_gamepad_t is a virtual gamepad.
// Different parsers should populate this virtual gamepad accordingly.
// For example, the virtual gamepad doesn't have a Hat, but has a D-pad.
// If the real gamepad has a Hat, it should populate the D-pad instead.
//
// If the real device only has one joy-pad, it should populate the left side
// of the virtual gamepad.
// Example: a TV remote control should populate the left D-pad and the left
// joypad.
//
// The virtual gamepad will then be processed by the different "platforms".
//
// Virtual Gamepad layout:
//
//  Left             Center            Right
//
//  brake: 0-1023    Menu button       throttle: 0-1023
//  L-shoulder button                  R-shoulder button
//  L-trigger button                   R-trigger button
//  d-pad                              buttons: A,B,X,Y,
//  L-joypad (axis: -512, 511)         R-joypad (axis: -512, 511)
//  axis-L button                      axis-R button
//  Gyro: [0]=X (pitch), [1]=Y (yaw), [2]=Z (roll) in radians/second (rad/s)
//  Accelerometer: [0]=X (right), [1]=Y (up), [2]=Z (back) in meters/second^2 (m/s^2)

typedef struct {
    // Usage Page: 0x01 (Generic Desktop Controls)
    uint8_t dpad;
    int32_t axis_x;
    int32_t axis_y;
    int32_t axis_rx;
    int32_t axis_ry;

    // Usage Page: 0x02 (Sim controls)
    int32_t brake;
    int32_t throttle;

    // Usage Page: 0x09 (Button)
    uint16_t buttons;

    // Misc buttons (from 0x0c (Consumer) and others)
    uint8_t misc_buttons;

    // 3-axis gyroscope angular velocity [X=pitch, Y=yaw, Z=roll] in radians/second (rad/s).
    float gyro[3];
    // 3-axis linear acceleration (including gravity) [X=right, Y=up, Z=back] in meters/second^2 (m/s^2).
    float accel[3];
} uni_gamepad_t;

/**
 * @brief 28-byte controller button, D-pad, analog axis, inversion, and pedal remapping matrix
 * (`sizeof(uni_gamepad_mappings_t) == 28`).
 *
 * Each field stores the virtual target bit-index or source selector to emit when the corresponding
 * physical control is actuated (or the physical source axis/pedal to sample for each virtual output).
 * This struct is transmitted directly over BLE GATT characteristic `AC0F` (28-byte read/write) and
 * bytes `1..28` of the 29-byte atomic custom-mappings write on `AC06`.
 *
 * **Binary Wire Layout (28 `uint8_t` bytes, naturally packed with zero padding):**
 * - Offsets `0..3` (`dpad_up`, `dpad_down`, `dpad_left`, `dpad_right`):
 *   `uni_gamepad_mappings_dpad_t` (`0..3`). Note that because `UNI_GAMEPAD_MAPPINGS_DPAD_RIGHT` is `2`
 *   and `UNI_GAMEPAD_MAPPINGS_DPAD_LEFT` is `3`, the default Xbox identity bytes at `0..3` are `{0, 1, 3, 2}`.
 * - Offsets `4..13` (`button_a` .. `button_thumb_r`):
 *   `uni_gamepad_mappings_button_t` (`0..9`). Default Xbox identity bytes at `4..13` are `{0, 1, 2, 3, 4, 5, 6, 7, 8,
 * 9}`.
 * - Offsets `14..17` (`misc_button_select`, `misc_button_start`, `misc_button_system`, `misc_button_capture`):
 *   `uni_gamepad_mappings_misc_button_t` (`0..3`). Note that because `UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SYSTEM`
 *   is `0`, `SELECT` is `1`, `START` is `2`, and `CAPTURE` is `3`, the default Xbox identity bytes at
 *   `14..17` are `{1, 2, 0, 3}`.
 * - Offsets `18..21` (`axis_x`, `axis_y`, `axis_rx`, `axis_ry`):
 *   `uni_gamepad_mappings_axis_t` (`0..3`). Default Xbox identity bytes at `18..21` are `{0, 1, 2, 3}`.
 * - Offsets `22..25` (`axis_x_inverted`, `axis_y_inverted`, `axis_rx_inverted`, `axis_ry_inverted`):
 *   Boolean flags (`0` = normal, `1` = negate sampled axis value). Default bytes at `22..25` are `{0, 0, 0, 0}`.
 * - Offsets `26..27` (`brake`, `throttle`):
 *   `uni_gamepad_mappings_pedal_t` (`0..1`). Default Xbox identity bytes at `26..27` are `{0, 1}`.
 */
typedef struct {
    // Remaps for Dpad (offsets 0..3)
    uint8_t dpad_up;
    uint8_t dpad_down;
    uint8_t dpad_left;
    uint8_t dpad_right;

    // Remaps for the buttons (offsets 4..13)
    uint8_t button_a;
    uint8_t button_b;
    uint8_t button_x;
    uint8_t button_y;

    uint8_t button_shoulder_l;
    uint8_t button_shoulder_r;
    uint8_t button_trigger_l;
    uint8_t button_trigger_r;

    uint8_t button_thumb_l;
    uint8_t button_thumb_r;

    // Remaps for misc/system buttons (offsets 14..17)
    uint8_t misc_button_select;
    uint8_t misc_button_start;
    uint8_t misc_button_system;
    uint8_t misc_button_capture;

    // Remaps for analog stick axes (offsets 18..21)
    uint8_t axis_x;
    uint8_t axis_y;
    uint8_t axis_rx;
    uint8_t axis_ry;

    // Whether each analog stick axis should be negated (offsets 22..25; 0 or 1)
    uint8_t axis_x_inverted;
    uint8_t axis_y_inverted;
    uint8_t axis_rx_inverted;
    uint8_t axis_ry_inverted;

    // Remaps for brake / throttle pedals (offsets 26..27)
    uint8_t brake;
    uint8_t throttle;
} uni_gamepad_mappings_t;

#if defined(__cplusplus)
static_assert(sizeof(uni_gamepad_mappings_t) == 28, "uni_gamepad_mappings_t must be 28 bytes for GATT AC0F");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(uni_gamepad_mappings_t) == 28, "uni_gamepad_mappings_t must be 28 bytes for GATT AC0F");
#endif

/**
 * @brief Compile-time designated initializer for the default Xbox gamepad mappings.
 *
 * Shared between `GAMEPAD_DEFAULT_MAPPINGS` and the internal static `map` table in
 * `uni_gamepad.c` so that C99/C11 static storage initialization is constant-expression
 * compliant and places `map` in `.data` instead of `.bss`. Without this compile-time
 * initializer, selecting `UNI_GAMEPAD_MAPPINGS_TYPE_CUSTOM` before calling
 * `uni_gamepad_set_mappings()` would read an all-zero `.bss` table, collapsing every
 * button press into `BUTTON_A` (`BIT(0)`) and routing all four analog axes from `axis_x`.
 */
#define UNI_GAMEPAD_DEFAULT_MAPPINGS_INIT                                \
    {                                                                    \
        .dpad_up = UNI_GAMEPAD_MAPPINGS_DPAD_UP,                         \
        .dpad_down = UNI_GAMEPAD_MAPPINGS_DPAD_DOWN,                     \
        .dpad_left = UNI_GAMEPAD_MAPPINGS_DPAD_LEFT,                     \
        .dpad_right = UNI_GAMEPAD_MAPPINGS_DPAD_RIGHT,                   \
        .button_a = UNI_GAMEPAD_MAPPINGS_BUTTON_A,                       \
        .button_b = UNI_GAMEPAD_MAPPINGS_BUTTON_B,                       \
        .button_x = UNI_GAMEPAD_MAPPINGS_BUTTON_X,                       \
        .button_y = UNI_GAMEPAD_MAPPINGS_BUTTON_Y,                       \
        .button_shoulder_l = UNI_GAMEPAD_MAPPINGS_BUTTON_SHOULDER_L,     \
        .button_shoulder_r = UNI_GAMEPAD_MAPPINGS_BUTTON_SHOULDER_R,     \
        .button_trigger_l = UNI_GAMEPAD_MAPPINGS_BUTTON_TRIGGER_L,       \
        .button_trigger_r = UNI_GAMEPAD_MAPPINGS_BUTTON_TRIGGER_R,       \
        .button_thumb_l = UNI_GAMEPAD_MAPPINGS_BUTTON_THUMB_L,           \
        .button_thumb_r = UNI_GAMEPAD_MAPPINGS_BUTTON_THUMB_R,           \
        .misc_button_select = UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SELECT,   \
        .misc_button_start = UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_START,     \
        .misc_button_system = UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_SYSTEM,   \
        .misc_button_capture = UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_CAPTURE, \
        .axis_x = UNI_GAMEPAD_MAPPINGS_AXIS_X,                           \
        .axis_y = UNI_GAMEPAD_MAPPINGS_AXIS_Y,                           \
        .axis_rx = UNI_GAMEPAD_MAPPINGS_AXIS_RX,                         \
        .axis_ry = UNI_GAMEPAD_MAPPINGS_AXIS_RY,                         \
        .axis_x_inverted = 0,                                            \
        .axis_y_inverted = 0,                                            \
        .axis_rx_inverted = 0,                                           \
        .axis_ry_inverted = 0,                                           \
        .brake = UNI_GAMEPAD_MAPPINGS_PEDAL_BRAKE,                       \
        .throttle = UNI_GAMEPAD_MAPPINGS_PEDAL_THROTTLE,                 \
    }

/**
 * @brief Default Xbox identity remapping matrix (`{0,1,3,2, 0..9, 1,2,0,3, 0..3, 0,0,0,0, 0,1}`).
 */
extern const uni_gamepad_mappings_t GAMEPAD_DEFAULT_MAPPINGS;

/**
 * @brief Dumps a gamepad state to the console log.
 * @param gp Pointer to the gamepad state to log.
 */
void uni_gamepad_dump(const uni_gamepad_t* gp);

/**
 * @brief Applies the currently active gamepad mapping preset or custom matrix to a gamepad state.
 *
 * - `UNI_GAMEPAD_MAPPINGS_TYPE_XBOX`: Returns `*gp` unchanged (fast-path identity).
 * - `UNI_GAMEPAD_MAPPINGS_TYPE_SWITCH`: Swaps `BUTTON_A <-> BUTTON_B` and `BUTTON_X <-> BUTTON_Y`.
 * - `UNI_GAMEPAD_MAPPINGS_TYPE_CUSTOM`: Remaps all 10 buttons, 4 D-pad directions, 4 misc buttons,
 *   4 analog stick axes (with optional negation), and 2 analog pedals according to the active
 *   28-byte `uni_gamepad_mappings_t` table while preserving `gyro[3]` and `accel[3]`.
 *
 * @param gp Non-NULL pointer to the physical input gamepad state.
 * @return The remapped virtual `uni_gamepad_t` state.
 */
uni_gamepad_t uni_gamepad_remap(const uni_gamepad_t* gp);

/**
 * @brief Validates and activates a user-defined 28-byte gamepad remapping matrix.
 *
 * Validates @p mappings with `uni_gamepad_mappings_is_valid()`. If valid, copies the
 * 28-byte mappings table into internal static storage and transitions the active mappings
 * type to `UNI_GAMEPAD_MAPPINGS_TYPE_CUSTOM`. If @p mappings is `NULL` or contains any
 * out-of-range byte, logs an error and leaves both the mappings table and mappings type
 * unchanged.
 *
 * **Example:**
 * @code
 * uni_gamepad_mappings_t custom = GAMEPAD_DEFAULT_MAPPINGS;
 * custom.button_a = UNI_GAMEPAD_MAPPINGS_BUTTON_B;
 * custom.button_b = UNI_GAMEPAD_MAPPINGS_BUTTON_A;
 * custom.axis_y_inverted = 1;
 * uni_gamepad_set_mappings(&custom);
 * // Active mappings type is now UNI_GAMEPAD_MAPPINGS_TYPE_CUSTOM (2).
 * @endcode
 *
 * @param mappings Pointer to the 28-byte gamepad mappings structure to apply.
 */
void uni_gamepad_set_mappings(const uni_gamepad_mappings_t* mappings);

/**
 * @brief Copies the currently configured custom gamepad mappings into @p out_mappings.
 *
 * On startup before any custom mappings are written, this outputs `GAMEPAD_DEFAULT_MAPPINGS`.
 * Does nothing if @p out_mappings is `NULL`.
 *
 * **Example:**
 * @code
 * uni_gamepad_mappings_t current;
 * uni_gamepad_get_mappings(&current);
 * // On fresh boot: current.dpad_left == 3, current.misc_button_select == 1.
 * @endcode
 *
 * @param out_mappings Output pointer to receive the 28-byte gamepad mappings structure.
 */
void uni_gamepad_get_mappings(uni_gamepad_mappings_t* out_mappings);

/**
 * @brief Validates that all 28 fields of a gamepad mappings structure are within valid bounds.
 *
 * Enforces strict per-field domain checks to prevent out-of-range bit-shift undefined behavior
 * (`BIT(n)` where `1 << n` with `n >= 31` is UB in C99/C11 on 32-bit signed `int`) and invalid
 * axis/pedal switch lookups when applying untrusted mappings (such as payloads received over
 * BLE GATT characteristics `AC06` or `AC0F`):
 * - `dpad_up`, `dpad_down`, `dpad_left`, `dpad_right` < `UNI_GAMEPAD_MAPPINGS_DPAD_COUNT` (`4`)
 * - `button_a` .. `button_thumb_r` < `UNI_GAMEPAD_MAPPINGS_BUTTON_COUNT` (`10`)
 * - `misc_button_select`, `misc_button_start`, `misc_button_system`, `misc_button_capture` <
 * `UNI_GAMEPAD_MAPPINGS_MISC_BUTTON_COUNT` (`4`)
 * - `axis_x`, `axis_y`, `axis_rx`, `axis_ry` < `UNI_GAMEPAD_MAPPINGS_AXIS_COUNT` (`4`)
 * - `axis_x_inverted`, `axis_y_inverted`, `axis_rx_inverted`, `axis_ry_inverted` <= `1`
 * - `brake`, `throttle` < `UNI_GAMEPAD_MAPPINGS_PEDAL_COUNT` (`2`)
 *
 * @param mappings Pointer to the 28-byte mappings structure to validate.
 * @return `true` if @p mappings is non-NULL and every field is within its valid domain; `false` otherwise.
 */
bool uni_gamepad_mappings_is_valid(const uni_gamepad_mappings_t* mappings);

/**
 * @brief Sets the active gamepad mappings preset type (`UNI_GAMEPAD_MAPPINGS_TYPE_XBOX`,
 * `UNI_GAMEPAD_MAPPINGS_TYPE_SWITCH`, or `UNI_GAMEPAD_MAPPINGS_TYPE_CUSTOM`).
 *
 * Switching to `XBOX` or `SWITCH` does not erase the stored 28-byte custom `map` table,
 * allowing callers or BLE clients (`AC06`) to toggle between presets and `CUSTOM` without
 * re-uploading `AC0F`.
 *
 * @param type The mappings preset type to activate.
 */
void uni_gamepad_set_mappings_type(uni_gamepad_mappings_type_t type);

/**
 * @brief Gets the currently active gamepad mappings preset type (`AC06`).
 * @return The active `uni_gamepad_mappings_type_t` (`0` = Xbox, `1` = Switch, `2` = Custom).
 */
uni_gamepad_mappings_type_t uni_gamepad_get_mappings_type(void);

/**
 * @brief Retrieves the human-readable controller model name for a `uni_controller_type_t` ID.
 * @param type Controller type identifier (`uni_controller_type_t`).
 * @return NUL-terminated string literal (e.g. `"DualSense"`, `"Switch Pro"`, or `"Unknown"`).
 */
const char* uni_gamepad_get_model_name(int type);

#ifdef __cplusplus
}
#endif

#endif  // UNI_GAMEPAD_H
