// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_property.h"

#include <string.h>

#include "sdkconfig.h"

#include <btstack_tlv.h>
#include <btstack_util.h>
#ifdef CONFIG_TARGET_POSIX
#include <btstack_tlv_posix.h>
#include <stdlib.h>
#endif

#include "uni_common.h"
#include "uni_log.h"

#ifdef CONFIG_TARGET_POSIX
#define TLV_DB_PATH_PREFIX "/tmp/bp32_property.tlv"
static btstack_tlv_posix_t tlv_context;
#endif

#define PROPERTY_STRING_MAX_LEN 128

typedef struct {
    const btstack_tlv_t* impl;
    void* context;
} tlv_handle_t;

// Prevent possible clashes from user using TLV directly
static const char tag_0 = 'B';
static const char tag_1 = 'P';
static const char tag_2 = '3';

static uint32_t get_tag_for_index(uint8_t index) {
    return ((uint32_t)tag_0 << 24) | ((uint32_t)tag_1 << 16) | ((uint32_t)tag_2 << 8) | index;
}

// Query the BTstack TLV singleton dynamically on every get/set call rather than caching
// it during uni_property_init(). On embedded targets like Pico W (CYW43), uni_property_init()
// runs inside uni_init() BEFORE BTstack reaches HCI_STATE_WORKING and registers its flash-bank
// TLV instance via btstack_tlv_set_instance(). Dynamic lookup ensures calls before HCI_STATE_WORKING
// safely fall back to property defaults, and calls after HCI_STATE_WORKING automatically use flash TLV.
static tlv_handle_t get_tlv(void) {
    tlv_handle_t h = {0};
    btstack_tlv_get_instance(&h.impl, &h.context);
    return h;
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

    tlv_handle_t tlv = get_tlv();
    if (!tlv.impl || !tlv.context) {
        logd("uni_property_set_with_property: TLV not initialized, skipping %s\n", p->name);
        return;
    }

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
        case UNI_PROPERTY_TYPE_STRING:
            // Reject NULL strings and enforce PROPERTY_STRING_MAX_LEN (including NUL terminator).
            if (!value.str) {
                loge("uni_property_set_with_property: NULL string for %s\n", p->name);
                return;
            }
            data = (uint8_t*)value.str;
            size = (int)strlen(value.str) + 1;
            if (size > PROPERTY_STRING_MAX_LEN) {
                loge("uni_property_set_with_property: string too long (%d)\n", size);
                return;
            }
            break;
        default:
            loge("uni_property_set_with_property: unsupported type %d\n", p->type);
            return;
    }

    if (tlv.impl->store_tag(tlv.context, get_tag_for_index(p->idx), data, size)) {
        loge("Failed to store property %s(%d)\n", p->name, p->idx);
    }
}

uni_property_value_t uni_property_get_with_property(const uni_property_t* p) {
    uni_property_value_t value;
    int size;
    int read;
    // Static buffer holds the most recently retrieved string property value;
    // zeroed before each TLV read to guarantee NUL-termination.
    static char str_ret[PROPERTY_STRING_MAX_LEN];

    memset(&value, 0, sizeof(value));
    if (!p) {
        loge("Invalid get property\n");
        return value;
    }

    tlv_handle_t tlv = get_tlv();
    if (!tlv.impl || !tlv.context) {
        logd("uni_property_get_with_property: TLV not initialized, returning default for %s\n", p->name);
        return p->default_value;
    }

    if (p->type == UNI_PROPERTY_TYPE_STRING) {
        memset(str_ret, 0, PROPERTY_STRING_MAX_LEN);
        read =
            tlv.impl->get_tag(tlv.context, get_tag_for_index(p->idx), (uint8_t*)str_ret, PROPERTY_STRING_MAX_LEN - 1);
        if (read == 0) {
            logd("Property %s (idx=%d, tag=%#x) not found in DB, returning default\n", p->name, p->idx,
                 get_tag_for_index(p->idx));
            return p->default_value;
        }
        str_ret[PROPERTY_STRING_MAX_LEN - 1] = '\0';
        value.str = str_ret;
        return value;
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
            loge("uni_property_get_with_property: unsupported type %d\n", p->type);
            return value;
    }

    read = tlv.impl->get_tag(tlv.context, get_tag_for_index(p->idx), (uint8_t*)&value, size);
    if (read == 0) {
        logd("Property %s (idx=%d, tag=%#x) not found in DB, returning default\n", p->name, p->idx,
             get_tag_for_index(p->idx));
        return p->default_value;
    }
    return value;
}

void uni_property_init(void) {
#ifdef CONFIG_TARGET_POSIX
    tlv_handle_t tlv = get_tlv();
    if (!tlv.impl || !tlv.context) {
        const char* tlv_path = getenv("BLUEPAD32_TLV_PATH");
        if (!tlv_path || tlv_path[0] == '\0') {
            tlv_path = TLV_DB_PATH_PREFIX;
        }
        logi("uni_property TLV path: %s\n", tlv_path);
        const btstack_tlv_t* tlv_impl = btstack_tlv_posix_init_instance(&tlv_context, tlv_path);
        btstack_tlv_set_instance(tlv_impl, &tlv_context);
    }
#else
    tlv_handle_t tlv = get_tlv();
    if (!tlv.impl || !tlv.context) {
        logd("TLV not initialized yet (will use defaults until BTstack TLV instance is registered)\n");
    }
#endif
    uni_property_init_debug();
}
