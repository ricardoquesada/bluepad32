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

#ifndef UNI_PROPERTY_H
#define UNI_PROPERTY_H

#include <stdint.h>

typedef enum {
    UNI_PROPERTY_TYPE_U8,
    UNI_PROPERTY_TYPE_U32,
    UNI_PROPERTY_TYPE_FLOAT,
    UNI_PROPERTY_TYPE_STRING,
} uni_property_type_t;

// Bluepad32-global properties
// Keep them sorted
extern const char* UNI_PROPERTY_KEY_BLE_ENABLED;
extern const char* UNI_PROPERTY_KEY_GAP_INQ_LEN;
extern const char* UNI_PROPERTY_KEY_GAP_LEVEL;
extern const char* UNI_PROPERTY_KEY_GAP_MAX_PERIODIC_LEN;
extern const char* UNI_PROPERTY_KEY_GAP_MIN_PERIODIC_LEN;
extern const char* UNI_PROPERTY_KEY_MOUSE_SCALE;

// Specific to Unijoysticle
// TODO: Move them to unijoysticle file
// Keep them sorted
extern const char* UNI_PROPERTY_KEY_UNI_AUTOFIRE_CPS;
extern const char* UNI_PROPERTY_KEY_UNI_MODEL;
extern const char* UNI_PROPERTY_KEY_UNI_MOUSE_EMULATION;
extern const char* UNI_PROPERTY_KEY_UNI_SERIAL_NUMBER;
extern const char* UNI_PROPERTY_KEY_UNI_VENDOR;
extern const char* UNI_PROPERTY_KEY_UNI_C64_POT_MODE;

typedef union {
    uint8_t u8;
    uint32_t u32;
    float f32;
    char* str;
} uni_property_value_t;

void uni_property_init(void);
void uni_property_set(const char* key, uni_property_type_t type, uni_property_value_t value);
uni_property_value_t uni_property_get(const char* key, uni_property_type_t type, uni_property_value_t def);

#endif  // UNI_PROPERTY_H
