/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2023 Ricardo Quesada

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

#include "uni_bt_allowlist.h"

#include "sdkconfig.h"
#include "uni_common.h"
#include "uni_log.h"
#include "uni_property.h"

static bd_addr_t addr_allow_list[CONFIG_BLUEPAD32_MAX_ALLOWLIST];
static bool enforced = false;
static const bd_addr_t zero_addr = {0, 0, 0, 0, 0, 0};

//
// Private functions
//
static void update_allowlist_to_property(void) {
    // Convert binary address to a string list
    // Example of a list of two elements:
    // 00:22:33:44:55:66,11:AB:8B:99:44:8A
    uni_property_value_t val;

    char str[128];

    str[0] = 0;

    for (size_t i = 0; i < ARRAY_SIZE(addr_allow_list); i++) {
        if (bd_addr_cmp(addr_allow_list[i], zero_addr) == 0)
            continue;
        char* tmp_str = bd_addr_to_str(addr_allow_list[i]);
        strcat(str, tmp_str);
        // Append delimeter between addresses
        strcat(str, ",");
    }

    val.str = str;
    uni_property_set(UNI_PROPERTY_KEY_ALLOWLIST_LIST, UNI_PROPERTY_TYPE_STRING, val);
}

static void update_allowlist_from_property(void) {
    // Parses the list from the property and stored it locally.
    uni_property_value_t def;
    uni_property_value_t val;
    bd_addr_t addr;
    int offset;
    int len;

    // Whether or not it is enabled.
    def.str = NULL;
    val = uni_property_get(UNI_PROPERTY_KEY_ALLOWLIST_LIST, UNI_PROPERTY_TYPE_STRING, def);

    if (val.str == NULL)
        return;

    offset = 0;
    len = strlen(val.str);

    while (offset < len) {
        if (!sscanf_bd_addr(&val.str[offset], addr)) {
            loge("Failed to parse allowlist: '%s' ('%s')\n", &val.str[offset], val.str);
            return;
        }
        uni_bt_allowlist_add_addr(addr);
        // Each address takes 18 bytes:
        // 00:11:22:33:44:55,
        offset += 6 * 2 + 5 + 1;
    }
}

//
// Public functions
//
bool uni_bt_allowlist_allow_addr(bd_addr_t addr) {
    // If not enforced, all addresses are allowed.
    if (!enforced)
        return true;

    for (size_t i = 0; i < ARRAY_SIZE(addr_allow_list); i++) {
        if (bd_addr_cmp(addr, addr_allow_list[i]) == 0)
            return true;
    }

    return false;
}

bool uni_bt_allowlist_add_addr(bd_addr_t addr) {
    for (size_t i = 0; i < ARRAY_SIZE(addr_allow_list); i++) {
        if (bd_addr_cmp(addr_allow_list[i], zero_addr) == 0) {
            bd_addr_copy(addr_allow_list[i], addr);
            update_allowlist_to_property();
            return true;
        }
    }
    return false;
}

bool uni_bt_allowlist_remove_addr(bd_addr_t addr) {
    for (size_t i = 0; i < ARRAY_SIZE(addr_allow_list); i++) {
        if (bd_addr_cmp(addr_allow_list[i], addr) == 0) {
            bd_addr_copy(addr_allow_list[i], zero_addr);
            update_allowlist_to_property();
            return true;
        }
    }
    return false;
}

void uni_bt_allowlist_list(void) {
    logi("Bluetooth allowlist addresses:\n");
    for (size_t i = 0; i < ARRAY_SIZE(addr_allow_list); i++) {
        if (bd_addr_cmp(addr_allow_list[i], zero_addr) == 0)
            continue;
        logi(" - %s\n", bd_addr_to_str(addr_allow_list[i]));
    }
}

bool uni_bt_allowlist_is_enabled(void) {
    return enforced;
}

void uni_bt_allowlist_set_enabled(bool enabled) {
    uni_property_value_t val;

    if (enabled != enforced) {
        enforced = enabled;

        val.u8 = enforced;
        uni_property_set(UNI_PROPERTY_KEY_ALLOWLIST_ENABLED, UNI_PROPERTY_TYPE_U8, val);
    }
}

void uni_bt_allowlist_init(void) {
    uni_property_value_t def;
    uni_property_value_t val;

    // Whether or not it is enabled.
    def.u8 = 0;
    val = uni_property_get(UNI_PROPERTY_KEY_ALLOWLIST_ENABLED, UNI_PROPERTY_TYPE_U8, def);
    enforced = val.u8;

    // The list of allowed-list addresses.
    // Need to fetch it, even if it is not enabled.
    update_allowlist_from_property();

    logi("Bluetooth Allowlist: %s\n", enforced ? "Enabled" : "Disabled");
    if (enforced)
        uni_bt_allowlist_list();
}