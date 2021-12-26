/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2021 Ricardo Quesada

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

#include "uni_utils.h"

#define CRCPOLY 0xedb88320

uint32_t uni_crc32_le(uint32_t crc, const uint8_t* data, size_t len) {
    uint32_t mult;
    int i;

    while (len--) {
        crc ^= *data++;
        for (i = 0; i < 8; i++) {
            mult = (crc & 1) ? CRCPOLY : 0;
            crc = (crc >> 1) ^ mult;
        }
    }

    return crc;
}
