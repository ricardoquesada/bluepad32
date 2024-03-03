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
static btstack_tlv_posix_t* tlv_context_ptr;

// Prevent possible clashes from user using TLV directly
static const char tag_0 = 'B';
static const char tag_1 = 'P';
static const char tag_2 = '3';

static uint32_t posix_get_tag_for_index(uint8_t index) {
    return (tag_0 << 24) | (tag_1 << 16) | (tag_2 << 8) | index;
}

static void create_instance_tlv(void) {
    logi("uni_property TLV path: %s\n", TLV_DB_PATH_PREFIX);
    tlv_impl = btstack_tlv_posix_init_instance(&tlv_context, TLV_DB_PATH_PREFIX);
    btstack_tlv_set_instance(tlv_impl, &tlv_context);
    tlv_context_ptr = &tlv_context;
}

static void get_or_create_instance_tlv(void) {
    btstack_tlv_get_instance(&tlv_impl, (void**)&tlv_context_ptr);
    if (!tlv_impl || !tlv_context_ptr)
        create_instance_tlv();
}

void uni_property_set_with_property(const uni_property_t* p, uni_property_value_t value) {
    uint8_t* data;
    int size;

    if (!p) {
        loge("Invalid set property\n");
        return;
    }

    if (p->flags & UNI_PROPERTY_FLAG_READ_ONLY)
        return;

    switch (p->type) {
        case UNI_PROPERTY_TYPE_BOOL:
            data = (uint8_t*)&value.boolean;
            size = sizeof(value.boolean);
            break;
        case UNI_PROPERTY_TYPE_U8:
            data = (uint8_t*)&value.u8;
            size = sizeof(value.u8);
            break;
        case UNI_PROPERTY_TYPE_U32:
            data = (uint8_t*)&value.u32;
            size = sizeof(value.u32);
            break;
        case UNI_PROPERTY_TYPE_FLOAT:
            data = (uint8_t*)&value.f32;
            size = sizeof(value.f32);
            break;
        default:
            loge("uni_property_set_with_property: unsupported type %d\n", p->type);
            return;
    }

    if (tlv_impl->store_tag(tlv_context_ptr, posix_get_tag_for_index(p->idx), data, size)) {
        loge("Failed to store property %s(%d)\n", p->name, posix_get_tag_for_index(p->idx));
    }
}

uni_property_value_t uni_property_get_with_property(const uni_property_t* p) {
    uni_property_value_t value;
    int size;
    int read;

    if (!p) {
        loge("Invalid get property\n");
        value.u8 = 0;
        return value;
    }

    if (p->type == UNI_PROPERTY_TYPE_STRING) {
        loge("No TLV for %s, returning default value\n", p->name);
        return p->default_value;
    }

    switch (p->type) {
        case UNI_PROPERTY_TYPE_BOOL:
            size = sizeof(value.boolean);
            break;
        case UNI_PROPERTY_TYPE_U8:
            size = sizeof(value.u8);
            break;
        case UNI_PROPERTY_TYPE_U32:
            size = sizeof(value.u32);
            break;
        case UNI_PROPERTY_TYPE_FLOAT:
            size = sizeof(value.f32);
            break;
        default:
            loge("uni_property_set_with_property: unsupported type %d\n", p->type);
            value.u8 = 0;
            return value;
    }

    read = tlv_impl->get_tag(tlv_context_ptr, posix_get_tag_for_index(p->idx), (uint8_t*)&value, size);
    if (read == 0) {
        logd("Property %s (%#x) not found in DB, returning default\n", p->name, posix_get_tag_for_index(p->idx));
        return p->default_value;
    }
    return value;
}

void uni_property_init(void) {
    get_or_create_instance_tlv();
    uni_property_init_debug();
}
