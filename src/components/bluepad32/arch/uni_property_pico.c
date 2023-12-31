// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_property.h"

#include <stddef.h>

#include "uni_common.h"

// TODO: Implement a memory cache.
// Used only in non-ESP32 platforms.
// Short-term solution: just return the default value.

void uni_property_set(uni_property_idx_t idx, uni_property_value_t value) {
    ARG_UNUSED(idx);
    ARG_UNUSED(value);
    /* Nothing */
}

uni_property_value_t uni_property_get(uni_property_idx_t idx) {
    uni_property_value_t ret;
    const uni_property_t *p;

    p = uni_property_get_property_for_index(idx);
    if (p == NULL) {
        ret.u8 = 0;
        return ret;
    }

    return p->default_value;
}

void uni_property_init(void) {
    uni_property_init_debug();
}
