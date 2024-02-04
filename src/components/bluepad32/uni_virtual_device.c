// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_virtual_device.h"

#include "uni_property.h"

#include "sdkconfig.h"

static bool virtual_device_enabled;

void uni_virtual_device_set_enabled(bool enabled) {
    uni_property_value_t val;

    if (enabled != virtual_device_enabled) {
        virtual_device_enabled = enabled;

        val.boolean = enabled;
        uni_property_set(UNI_PROPERTY_IDX_VIRTUAL_DEVICE_ENABLED, val);
    }
}

bool uni_virtual_device_is_enabled(void) {
    return virtual_device_enabled;
}

void uni_virtual_device_init(void) {
    uni_property_value_t val;

    val = uni_property_get(UNI_PROPERTY_IDX_VIRTUAL_DEVICE_ENABLED);
    virtual_device_enabled = val.boolean;
}