// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_property.h"

#include "uni_log.h"

// TODO: Implement a memory cache.
// Used only in non-ESP32 platforms.
// Short-term solution: just return the default value.

void uni_property_set_with_property(const uni_property_t* p, uni_property_value_t value) {
    ARG_UNUSED(value);

    if (!p) {
        loge("Invalid set property\n");
        return;
    }

    if (p->flags & UNI_PROPERTY_FLAG_READ_ONLY)
        return;
    return;
}

uni_property_value_t uni_property_get_with_property(const uni_property_t* p) {
    if (!p) {
        uni_property_value_t ret;
        loge("Invalid get property\n");
        ret.u8 = 0;
        return ret;
    }

    return p->default_value;
}

void uni_property_init(void) {
    uni_property_init_debug();
}
