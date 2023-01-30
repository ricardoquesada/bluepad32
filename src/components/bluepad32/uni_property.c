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

#include "uni_property.h"
#include "uni_property_mem.h"
#include "uni_property_nvs.h"

#include "sdkconfig.h"

// Globals
// Keep them sorted
// Keys name should not be longer than NVS_KEY_NAME_MAX_SIZE (16 chars).
const char* UNI_PROPERTY_KEY_BLE_ENABLED = "bp.ble.enabled";
const char* UNI_PROPERTY_KEY_GAP_INQ_LEN = "bp.gap.inq_len";
const char* UNI_PROPERTY_KEY_GAP_LEVEL = "bp.gap.level";
const char* UNI_PROPERTY_KEY_GAP_MAX_PERIODIC_LEN = "bp.gap.max_len";
const char* UNI_PROPERTY_KEY_GAP_MIN_PERIODIC_LEN = "bp.gap.min_len";
const char* UNI_PROPERTY_KEY_MOUSE_SCALE = "bp.mouse.scale";

// Unijoysticle only
// TODO: Move them to the Unijoysticle file.
// Keep them sorted
const char* UNI_PROPERTY_KEY_UNI_AUTOFIRE_CPS = "bp.uni.autofire";
const char* UNI_PROPERTY_KEY_UNI_MODEL = "bp.uni.model";
const char* UNI_PROPERTY_KEY_UNI_MOUSE_EMULATION = "bp.uni.mouseemu";
const char* UNI_PROPERTY_KEY_UNI_SERIAL_NUMBER = "bp.uni.serial";
const char* UNI_PROPERTY_KEY_UNI_VENDOR = "bp.uni.vendor";
const char* UNI_PROPERTY_KEY_UNI_C64_POT_MODE = "bp.uni.c64pot";

// TODO: Implement "property interface" instead of doing #ifdef

void uni_property_set(const char* key, uni_property_type_t type, uni_property_value_t value) {
#if defined(CONFIG_BLUEPAD32_PLATFORM_PC_DEBUG)
    uni_property_mem_set(key, type, value);
#else
    uni_property_nvs_set(key, type, value);
#endif
}

uni_property_value_t uni_property_get(const char* key, uni_property_type_t type, uni_property_value_t def) {
#if defined(CONFIG_BLUEPAD32_PLATFORM_PC_DEBUG)
    return uni_property_mem_get(key, type, def);
#else
    return uni_property_nvs_get(key, type, def);
#endif
}

void uni_property_init(void) {
#if defined(CONFIG_BLUEPAD32_PLATFORM_PC_DEBUG)
    return uni_property_mem_init();
#else
    return uni_property_nvs_init();
#endif
}
