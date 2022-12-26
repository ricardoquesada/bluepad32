/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2022 Ricardo Quesada

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
****************************************************************************/

#include "uni_property_nvs.h"

#include <nvs.h>
#include <nvs_flash.h>
#include <string.h>

#include "uni_log.h"

static const char* STORAGE_NAMESPACE = "bp32";

// Uses NVS for storage. Used in all ESP32 Bluepad32 platforms.

void uni_property_nvs_set(const char* key, uni_property_type_t type, uni_property_value_t value) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    uint32_t* float_alias;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        loge("Could not open readwrite NVS storage, key: %s, err=%#x\n", key, err);
        return;
    }

    switch (type) {
        case UNI_PROPERTY_TYPE_U8:
            err = nvs_set_u8(nvs_handle, key, value.u8);
            break;
        case UNI_PROPERTY_TYPE_U32:
            err = nvs_set_u32(nvs_handle, key, value.u32);
            break;
        case UNI_PROPERTY_TYPE_FLOAT:
            float_alias = (uint32_t*)&value.f32;
            err = nvs_set_u32(nvs_handle, key, *float_alias);
            break;
        case UNI_PROPERTY_TYPE_STRING:
            err = nvs_set_str(nvs_handle, key, value.str);
            break;
    }

    if (err != ESP_OK) {
        loge("Could not store '%s' in NVS, err=%#x\n", key, err);
        goto out;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        loge("Could not commit '%s' in NVS, err=%#x\n", key, err);
    }

out:
    nvs_close(nvs_handle);
}

uni_property_value_t uni_property_nvs_get(const char* key, uni_property_type_t type, uni_property_value_t def) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    uni_property_value_t ret;
    size_t str_len;
    static char str_ret[64];

    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        // Might be valid if no bp32 keys were stored
        logd("Could not open readonly NVS storage, key:'%s'\n", key);
        return def;
    }

    switch (type) {
        case UNI_PROPERTY_TYPE_U8:
            err = nvs_get_u8(nvs_handle, key, &ret.u8);
            break;
        case UNI_PROPERTY_TYPE_U32:
            err = nvs_get_u32(nvs_handle, key, &ret.u32);
            break;
        case UNI_PROPERTY_TYPE_FLOAT:
            err = nvs_get_u32(nvs_handle, key, (uint32_t*)&ret.f32);
            break;
        case UNI_PROPERTY_TYPE_STRING:
            ret.str = str_ret;
            memset(str_ret, 0, sizeof(str_ret));
            err = nvs_get_str(nvs_handle, key, str_ret, &str_len);
            break;
    }

    if (err != ESP_OK) {
        // Might be valid if the key was not previously stored
        logd("could not read property '%s' from NVS, err=%#x\n", key, err);
        ret = def;
        /* falltrhough */
    }

    nvs_close(nvs_handle);
    return ret;
}

void uni_property_nvs_init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        logi("Erasing flash\n");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}
