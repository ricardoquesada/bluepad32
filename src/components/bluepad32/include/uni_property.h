// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_PROPERTY_H
#define UNI_PROPERTY_H

#include <stdbool.h>
#include <stdint.h>

#include "uni_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file uni_property.h
 * @brief Unified non-volatile property storage interface.
 *
 * Defines the keys and types for persistent device configuration (e.g. BT allowlists,
 * thresholds, scanning options) and abstracts the underlying storage (NVS or BTstack TLV).
 */

// Bluepad32-global properties
// Keep them sorted
#define UNI_PROPERTY_NAME_ALLOWLIST_ENABLED "bp.bt.allow_en"
#define UNI_PROPERTY_NAME_ALLOWLIST_LIST "bp.bt.allowlist"
#define UNI_PROPERTY_NAME_BLE_ENABLED "bp.ble.enabled"
#define UNI_PROPERTY_NAME_BLE_SERVICE_ENABLED "bp.ble.svc_en"
#define UNI_PROPERTY_NAME_BLE_SERVICE_NAME "bp.ble.name"
#define UNI_PROPERTY_NAME_BLE_SERVICE_PASSWORD "bp.ble.pass"
#define UNI_PROPERTY_NAME_GAP_INQ_LEN "bp.gap.inq_len"
#define UNI_PROPERTY_NAME_GAP_LEVEL "bp.gap.level"
#define UNI_PROPERTY_NAME_GAP_MAX_PERIODIC_LEN "bp.gap.max_len"
#define UNI_PROPERTY_NAME_GAP_MIN_PERIODIC_LEN "bp.gap.min_len"
#define UNI_PROPERTY_NAME_MOUSE_SCALE "bp.mouse.scale"
#define UNI_PROPERTY_NAME_UNI_BB_FIRE_THRESHOLD "bp.uni.bb_fire"
#define UNI_PROPERTY_NAME_UNI_BB_MOVE_THRESHOLD "bp.uni.bb_move"
#define UNI_PROPERTY_NAME_VERSION "bp.version"
#define UNI_PROPERTY_NAME_VIRTUAL_DEVICE_ENABLED "bp.virt_dev_en"

typedef enum {
    UNI_PROPERTY_IDX_ALLOWLIST_ENABLED,
    UNI_PROPERTY_IDX_ALLOWLIST_LIST,
    UNI_PROPERTY_IDX_BLE_ENABLED,
    UNI_PROPERTY_IDX_GAP_INQ_LEN,
    UNI_PROPERTY_IDX_GAP_LEVEL,
    UNI_PROPERTY_IDX_GAP_MAX_PERIODIC_LEN,
    UNI_PROPERTY_IDX_GAP_MIN_PERIODIC_LEN,
    UNI_PROPERTY_IDX_MOUSE_SCALE,
    UNI_PROPERTY_IDX_VERSION,
    UNI_PROPERTY_IDX_VIRTUAL_DEVICE_ENABLED,
    // Placed immediately before UNI_PROPERTY_IDX_LAST so that Balance Board thresholds are
    // registered in the global property table on all platforms (called from uni_balance_board_init()
    // during uni_init()) without shifting the numeric indices (0..9) of existing global properties
    // stored as BTstack TLV tags ('BP3' | idx).
    UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD,
    UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD,
    UNI_PROPERTY_IDX_BLE_SERVICE_ENABLED,
    UNI_PROPERTY_IDX_BLE_SERVICE_NAME,
    UNI_PROPERTY_IDX_BLE_SERVICE_PASSWORD,
    UNI_PROPERTY_IDX_LAST,

    // Unijoysticle only properties
    // TODO: Should be moved to the platform file
    // Or could be conditionally compiled.
    UNI_PROPERTY_IDX_UNI_AUTOFIRE_CPS = UNI_PROPERTY_IDX_LAST,
    UNI_PROPERTY_IDX_UNI_C64_POT_MODE,
    UNI_PROPERTY_IDX_UNI_MODEL,
    UNI_PROPERTY_IDX_UNI_MOUSE_EMULATION,
    UNI_PROPERTY_IDX_UNI_SERIAL_NUMBER,
    UNI_PROPERTY_IDX_UNI_VENDOR,
    UNI_PROPERTY_IDX_UNI_LAST,

    // Should be the last one
    UNI_PROPERTY_IDX_COUNT = UNI_PROPERTY_IDX_UNI_LAST
} uni_property_idx_t;

typedef enum {
    UNI_PROPERTY_TYPE_BOOL,
    UNI_PROPERTY_TYPE_U8,
    UNI_PROPERTY_TYPE_U32,
    UNI_PROPERTY_TYPE_FLOAT,
    UNI_PROPERTY_TYPE_STRING,
} uni_property_type_t;

typedef union {
    // Keep the pointer-sized `str` member first so that `= {0}` zero-initializes
    // the entire union on both 32-bit and 64-bit targets.
    const char* str;
    float f32;
    uint32_t u32;
    uint8_t u8;
    bool boolean;
} uni_property_value_t;

typedef enum {
    UNI_PROPERTY_FLAG_READ_ONLY = BIT(0),
} uni_property_flag_t;

typedef struct {
    uni_property_idx_t idx;  // Used for debugging: idx must match order, and for tlv
    const char* name;
    uni_property_type_t type;
    uni_property_value_t default_value;
    uni_property_flag_t flags;
} uni_property_t;

/**
 * @brief Sets the value of a specific property.
 *
 * @param idx The index of the property to set.
 * @param value The value to assign to the property.
 */
void uni_property_set(uni_property_idx_t idx, uni_property_value_t value);

/**
 * @brief Gets the value of a specific property.
 *
 * @param idx The index of the property to retrieve.
 * @return The retrieved value, or the default value if the property is not found.
 */
uni_property_value_t uni_property_get(uni_property_idx_t idx);

/**
 * @brief Dumps all properties and their current values to the log.
 */
void uni_property_dump_all(void);
__attribute__((deprecated("Use `uni_property_dump_all` instead"))) static inline void uni_property_list_all(void) {
    uni_property_dump_all();
}

/**
 * @brief Dumps a specific property's details to the log.
 *
 * @param p Pointer to the property definition.
 */
void uni_property_dump_property(const uni_property_t* p);

/**
 * @brief Initializes the property debugging subsystem.
 */
void uni_property_init_debug(void);

/**
 * @brief Retrieves a property definition by its name.
 *
 * @param name The name of the property.
 * @return Pointer to the property definition, or NULL if not found.
 */
const uni_property_t* uni_property_get_property_by_name(const char* name);

// Architecture-specific storage backend interface (implemented by NVS on ESP32,
// BTstack TLV on POSIX / Pico W, or in-memory storage).

/**
 * @brief Initializes the underlying architecture-specific storage backend.
 */
void uni_property_init(void);

/**
 * @brief Sets a property value directly using its property definition.
 *
 * @param p Pointer to the property definition.
 * @param value The value to assign.
 */
void uni_property_set_with_property(const uni_property_t* p, uni_property_value_t value);

/**
 * @brief Gets a property value directly using its property definition.
 *
 * @param p Pointer to the property definition.
 * @return The retrieved value, or the default value if not found.
 */
uni_property_value_t uni_property_get_with_property(const uni_property_t* p);

#ifdef __cplusplus
}
#endif

#endif  // UNI_PROPERTY_H
