// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_property.h"

#include <btstack_tlv_posix.h>
#include <btstack_util.h>
#include <hci.h>

#include "uni_common.h"
#include "uni_log.h"

#define TLV_DB_PATH_PREFIX "/tmp/bp32_property.tvl"

static const btstack_tlv_t* tlv_impl;
static btstack_tlv_posix_t tlv_context;

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

    tlv_impl = btstack_tlv_posix_init_instance(&tlv_context, TLV_DB_PATH_PREFIX);
    btstack_tlv_set_instance(tlv_impl, &tlv_context);

    logi("uni_property TLV path: %s\n", TLV_DB_PATH_PREFIX);
}
