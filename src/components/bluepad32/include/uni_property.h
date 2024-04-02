// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_PROPERTY_H
#define UNI_PROPERTY_H

#include <stdbool.h>
#include <stdint.h>

#include "uni_common.h"

// Bluepad32-global properties
// Keep them sorted
#define UNI_PROPERTY_NAME_ALLOWLIST_ENABLED "bp.bt.allow_en"
#define UNI_PROPERTY_NAME_ALLOWLIST_LIST "bp.bt.allowlist"
#define UNI_PROPERTY_NAME_BLE_ENABLED "bp.ble.enabled"
#define UNI_PROPERTY_NAME_GAP_INQ_LEN "bp.gap.inq_len"
#define UNI_PROPERTY_NAME_GAP_LEVEL "bp.gap.level"
#define UNI_PROPERTY_NAME_GAP_MAX_PERIODIC_LEN "bp.gap.max_len"
#define UNI_PROPERTY_NAME_GAP_MIN_PERIODIC_LEN "bp.gap.min_len"
#define UNI_PROPERTY_NAME_MOUSE_SCALE "bp.mouse.scale"
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
    UNI_PROPERTY_IDX_LAST,

    // Unijoysticle only properties
    // TODO: Should be moved to the platform file
    // Or could be conditionally compiled.
    UNI_PROPERTY_IDX_UNI_AUTOFIRE_CPS = UNI_PROPERTY_IDX_LAST,
    UNI_PROPERTY_IDX_UNI_BB_FIRE_THRESHOLD,
    UNI_PROPERTY_IDX_UNI_BB_MOVE_THRESHOLD,
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
    bool boolean;
    uint8_t u8;
    uint32_t u32;
    float f32;
    const char* str;
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

void uni_property_set(uni_property_idx_t idx, uni_property_value_t value);
uni_property_value_t uni_property_get(uni_property_idx_t idx);
void uni_property_dump_all(void);
__attribute__((deprecated("Use `uni_property_dump_all` instead"))) inline void uni_property_list_all(void) {
    uni_property_dump_all();
}
void uni_property_dump_property(const uni_property_t* p);
void uni_property_init_debug(void);
const uni_property_t* uni_property_get_property_by_name(const char* name);

// Interface
// Each arch needs to implement these functions:
void uni_property_init(void);
void uni_property_set_with_property(const uni_property_t* p, uni_property_value_t value);
uni_property_value_t uni_property_get_with_property(const uni_property_t* p);

#endif  // UNI_PROPERTY_H
