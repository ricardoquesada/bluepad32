// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_property.h"

#include <stdbool.h>
#include <stddef.h>

#include "bt/uni_bt_defines.h"
#include "platform/uni_platform.h"
#include "sdkconfig.h"
#include "uni_log.h"
#include "uni_version.h"

static const uni_property_t properties[] = {
    {UNI_PROPERTY_IDX_ALLOWLIST_ENABLED, UNI_PROPERTY_NAME_ALLOWLIST_ENABLED, UNI_PROPERTY_TYPE_BOOL,
     .default_value.boolean = false},
    {UNI_PROPERTY_IDX_ALLOWLIST_LIST, UNI_PROPERTY_NAME_ALLOWLIST_LIST, UNI_PROPERTY_TYPE_STRING,
     .default_value.str = NULL},
    {UNI_PROPERTY_IDX_BLE_ENABLED, UNI_PROPERTY_NAME_BLE_ENABLED, UNI_PROPERTY_TYPE_BOOL,
#ifdef CONFIG_BLUEPAD32_ENABLE_BLE_BY_DEFAULT
     .default_value.boolean = true
#else
     .default_value.boolean = false
#endif  // CONFIG_BLUEPAD32_ENABLE_BLE_BY_DEFAULT
    },
    {UNI_PROPERTY_IDX_GAP_INQ_LEN, UNI_PROPERTY_NAME_GAP_INQ_LEN, UNI_PROPERTY_TYPE_U8,
     .default_value.u8 = UNI_BT_INQUIRY_LENGTH},
    // It seems that with gap_security_level(0) all controllers work except Nintendo Switch Pro controller.
    {UNI_PROPERTY_IDX_GAP_LEVEL, UNI_PROPERTY_NAME_GAP_LEVEL, UNI_PROPERTY_TYPE_U8,
#ifdef CONFIG_BLUEPAD32_GAP_SECURITY
     .default_value.u8 = 2
#else
     .default_value.u8 = 0
#endif  // CONFIG_BLUEPAD32_GAP_SECURITY
    },
    {UNI_PROPERTY_IDX_GAP_MAX_PERIODIC_LEN, UNI_PROPERTY_NAME_GAP_MAX_PERIODIC_LEN, UNI_PROPERTY_TYPE_U8,
     .default_value.u8 = UNI_BT_MAX_PERIODIC_LENGTH},
    {UNI_PROPERTY_IDX_GAP_MIN_PERIODIC_LEN, UNI_PROPERTY_NAME_GAP_MIN_PERIODIC_LEN, UNI_PROPERTY_TYPE_U8,
     .default_value.u8 = UNI_BT_MIN_PERIODIC_LENGTH},
    {UNI_PROPERTY_IDX_MOUSE_SCALE, UNI_PROPERTY_NAME_MOUSE_SCALE, UNI_PROPERTY_TYPE_FLOAT, .default_value.f32 = 1.0f},
    {UNI_PROPERTY_IDX_VERSION, UNI_PROPERTY_NAME_VERSION, UNI_PROPERTY_TYPE_STRING, .default_value.str = UNI_VERSION,
     .flags = UNI_PROPERTY_FLAG_READ_ONLY},
    {UNI_PROPERTY_IDX_VIRTUAL_DEVICE_ENABLED, UNI_PROPERTY_NAME_VIRTUAL_DEVICE_ENABLED, UNI_PROPERTY_TYPE_BOOL,
#ifdef CONFIG_BLUEPAD32_ENABLE_VIRTUAL_DEVICE_BY_DEFAULT
     .default_value.boolean = true
#else
     .default_value.boolean = false
#endif  // CONFIG_BLUEPAD32_ENABLE_VIRTUAL_DEVICE_BY_DEFAULT
    },

    // TODO: Platform specific. Should be defined in its own file.
};
_Static_assert(ARRAY_SIZE(properties) == UNI_PROPERTY_IDX_LAST, "Invalid properties size");

static const uni_property_t* get_property(uni_property_idx_t idx);

// Helpers
static const uni_property_t* get_property(uni_property_idx_t idx) {
    if (idx >= UNI_PROPERTY_IDX_LAST) {
        if (uni_get_platform()->get_property)
            return uni_get_platform()->get_property(idx);
        // Invalid
        return NULL;
    }

    return &properties[idx];
}

// Public functions

void uni_property_init_debug(void) {
    for (int i = 0; i < ARRAY_SIZE(properties); i++) {
        const uni_property_t* p = &properties[i];
        if (p->idx != i) {
            loge("Invalid property index: %d != %d\n", i, p->idx);
        }
    }
}

const uni_property_t* uni_property_get_property_by_name(const char* name) {
    if (!name)
        return NULL;

    // TODO: parse property name
    for (int i = 0; i < UNI_PROPERTY_IDX_COUNT; i++) {
        const uni_property_t* p = get_property(i);
        if (!p || !p->name)
            continue;
        if (strcmp(p->name, name) == 0)
            return p;
    }
    return NULL;
}

void uni_property_dump_property(const uni_property_t* p) {
    if (!p)
        return;

    uni_property_value_t val = uni_property_get_with_property(p);
    switch (p->type) {
        case UNI_PROPERTY_TYPE_BOOL:
            logi("%s = %s\n", p->name, val.boolean ? "true" : "false");
            break;
        case UNI_PROPERTY_TYPE_U8:
            logi("%s = %d\n", p->name, val.u8);
            break;
        case UNI_PROPERTY_TYPE_U32:
            logi("%s = %u (%#x)\n", p->name, val.u32, val.u32);
            break;
        case UNI_PROPERTY_TYPE_FLOAT:
            logi("%s = %f\n", p->name, val.f32);
            break;
            break;
        case UNI_PROPERTY_TYPE_STRING:
            if (val.str)
                logi("%s = '%s'\n", p->name, val.str);
            else
                logi("%s = <empty>\n", p->name);
            break;
        default:
            loge("%s = Unsupported property type %d\n", p->name, p->type);
            break;
    }
}

void uni_property_dump_all(void) {
    logi("properties:\n");
    for (int i = 0; i < UNI_PROPERTY_IDX_COUNT; i++) {
        const uni_property_t* p = get_property(i);
        if (!p)
            // Means the property is not implemented, safe to break here.
            break;
        uni_property_dump_property(p);
    }
}

void uni_property_set(uni_property_idx_t idx, uni_property_value_t value) {
    const uni_property_t* p = get_property(idx);
    if (!p) {
        loge("Could not find property %d\n", idx);
        return;
    }
    uni_property_set_with_property(p, value);
}

uni_property_value_t uni_property_get(uni_property_idx_t idx) {
    const uni_property_t* p = get_property(idx);
    if (!p) {
        uni_property_value_t ret;
        loge("Could not find property %d\n", idx);
        ret.u8 = 0;
        return ret;
    }
    return uni_property_get_with_property(p);
}
