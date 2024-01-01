// SPDX-License-Identifier: Apache-2.0
// Copyright 2021 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_UTILS_H
#define UNI_UTILS_H

#include <stddef.h>
#include <stdint.h>

// Little-endian CRC32.
// ESP32 has its own crc32_le as well, but they don't return the same values (?).
// It is important to use ours with the "uni_" prefix.
uint32_t uni_crc32_le(uint32_t crc, const uint8_t* data, size_t len);

#endif  // UNI_UTILS_H
