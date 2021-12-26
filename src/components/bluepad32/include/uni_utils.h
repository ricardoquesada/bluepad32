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

#ifndef UNI_UTILS_H
#define UNI_UTILS_H

#include <stddef.h>
#include <stdint.h>

// Little-endian CRC32.
// ESP32 has its own crc32_le as well, but they don't return the same values (?).
// It is important to use ours with the "uni_" prefix.
uint32_t uni_crc32_le(uint32_t crc, const uint8_t* data, size_t len);

#endif  // UNI_UTILS_H
