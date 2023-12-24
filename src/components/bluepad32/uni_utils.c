// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

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
