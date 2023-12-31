// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_property.h"

#include <stddef.h>
#include <assert.h>

#include "bt/uni_bt_defines.h"
#include "platform/uni_platform.h"
#include "sdkconfig.h"
#include "uni_log.h"

static const uni_property_t properties[] = {
    {UNI_PROPERTY_IDX_ALLOWLIST_ENABLED, UNI_PROPERTY_NAME_ALLOWLIST_ENABLED, UNI_PROPERTY_TYPE_U8,
     .default_value.u8 = 0},
    {UNI_PROPERTY_IDX_ALLOWLIST_LIST, UNI_PROPERTY_NAME_ALLOWLIST_LIST, UNI_PROPERTY_TYPE_STRING,
     .default_value.str = NULL},
    {UNI_PROPERTY_IDX_BLE_ENABLED, UNI_PROPERTY_NAME_BLE_ENABLED, UNI_PROPERTY_TYPE_U8,
#ifdef CONFIG_BLUEPAD32_ENABLE_BLE_BY_DEFAULT
     .default_value.u8 = 1
#else
     .default_value.u8 = 0
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
    {UNI_PROPERTY_IDX_VIRTUAL_DEVICE_ENABLED, UNI_PROPERTY_NAME_VIRTUAL_DEVICE_ENABLED, UNI_PROPERTY_TYPE_U8,
#ifdef CONFIG_BLUEPAD32_ENABLE_VIRTUAL_DEVICE_BY_DEFAULT
     .default_value.u8 = 1
#else
     .default_value.u8 = 0
#endif  // CONFIG_BLUEPAD32_ENABLE_VIRTUAL_DEVICE_BY_DEFAULT
    },

    // TODO: Platform specific. Should be defined in its own file.
};
_Static_assert(ARRAY_SIZE(properties) == UNI_PROPERTY_IDX_LAST, "Invalid properties size");

void uni_property_init_debug(void) {
    for (int i = 0; i < ARRAY_SIZE(properties); i++) {
        const uni_property_t* p = &properties[i];
        if (p->idx != i) {
            loge("Invalid property index: %d != %d\n", i, p->idx);
        }
    }
}

const uni_property_t* uni_property_get_property_for_index(uni_property_idx_t idx) {
    if (idx >= UNI_PROPERTY_IDX_LAST) {
        if (uni_get_platform()->get_property)
            return uni_get_platform()->get_property(idx);
        // Invalid
        return NULL;
    }

    return &properties[idx];
}
