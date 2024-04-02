// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_property.h"

#include <nvs.h>
#include <nvs_flash.h>
#include <string.h>

#include "uni_log.h"

#define PROPERTY_STRING_MAX_LEN 128

static const char* STORAGE_NAMESPACE = "bp32";

// Uses NVS for storage. Used in all ESP32 Bluepad32 platforms.

void uni_property_set_with_property(const uni_property_t* p, uni_property_value_t value) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    uint32_t* float_alias;

    if (!p) {
        loge("Cannot set invalid property\n");
        return;
    }

    if (p->flags & UNI_PROPERTY_FLAG_READ_ONLY) {
        loge("Cannot set READ_ONLY property: '%s'\n", p->name);
        return;
    }

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        loge("Could not open readwrite NVS storage, key: %s, err=%#x\n", p->name, err);
        return;
    }

    switch (p->type) {
        case UNI_PROPERTY_TYPE_BOOL:
        case UNI_PROPERTY_TYPE_U8:
            err = nvs_set_u8(nvs_handle, p->name, value.u8);
            break;
        case UNI_PROPERTY_TYPE_U32:
            err = nvs_set_u32(nvs_handle, p->name, value.u32);
            break;
        case UNI_PROPERTY_TYPE_FLOAT:
            float_alias = (uint32_t*)&value.f32;
            err = nvs_set_u32(nvs_handle, p->name, *float_alias);
            break;
        case UNI_PROPERTY_TYPE_STRING:
            err = nvs_set_str(nvs_handle, p->name, value.str);
            break;
    }

    if (err != ESP_OK) {
        loge("Could not store '%s' in NVS, err=%#x\n", p->name, err);
        goto out;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        loge("Could not commit '%s' in NVS, err=%#x\n", p->name, err);
    }

out:
    nvs_close(nvs_handle);
}

uni_property_value_t uni_property_get_with_property(const uni_property_t* p) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    uni_property_value_t ret;
    size_t str_len = PROPERTY_STRING_MAX_LEN - 1;
    static char str_ret[PROPERTY_STRING_MAX_LEN];

    if (!p) {
        loge("Cannot get invalid property\n");
        ret.u8 = 0;
        return ret;
    }

    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        // Might be valid if no bp32 keys were stored
        logd("Could not open readonly NVS storage, key:'%s'\n", p->name);
        return p->default_value;
    }

    switch (p->type) {
        case UNI_PROPERTY_TYPE_BOOL:
        case UNI_PROPERTY_TYPE_U8:
            err = nvs_get_u8(nvs_handle, p->name, &ret.u8);
            break;
        case UNI_PROPERTY_TYPE_U32:
            err = nvs_get_u32(nvs_handle, p->name, &ret.u32);
            break;
        case UNI_PROPERTY_TYPE_FLOAT:
            err = nvs_get_u32(nvs_handle, p->name, (uint32_t*)&ret.f32);
            break;
        case UNI_PROPERTY_TYPE_STRING:
            ret.str = str_ret;
            memset(str_ret, 0, sizeof(str_ret));
            err = nvs_get_str(nvs_handle, p->name, str_ret, &str_len);
            break;
    }

    if (err != ESP_OK) {
        // Might be valid if the key was not previously stored
        logd("could not read property '%s' from NVS, err=%#x\n", p->name, err);
        ret = p->default_value;
        /* falltrhough */
    }

    nvs_close(nvs_handle);
    return ret;
}

void uni_property_init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        logi("Erasing flash\n");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}
