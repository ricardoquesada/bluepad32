// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_property.h"

#include "uni_common.h"

// TODO: Implement a memory cache.
// Used only in non-ESP32 platforms.
// Short-term solution: just return the default value.

void uni_property_set(const char* key, uni_property_type_t type, uni_property_value_t value) {
    ARG_UNUSED(key);
    ARG_UNUSED(type);
    ARG_UNUSED(value);
    /* Nothing */
}

uni_property_value_t uni_property_get(const char* key, uni_property_type_t type, uni_property_value_t def) {
    ARG_UNUSED(key);
    ARG_UNUSED(type);

    return def;
}

void uni_property_init(void) {
    /* Nothing */
}
