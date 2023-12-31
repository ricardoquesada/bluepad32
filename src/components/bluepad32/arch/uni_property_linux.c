// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_property.h"

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
    return uni_property_get_property_for_index(idx)->default_value;
}

void uni_property_init(void) {
    uni_property_init_debug();
}
