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

// This is a well-known function. No need to add the "uni_" prefix.
#define CRCPOLY 0xedb88320
uint32_t crc32_le(uint32_t seed, const void* data, size_t len) {
  uint32_t crc = seed;
  const uint8_t* src = data;
  uint32_t mult;
  int i;

  while (len--) {
    crc ^= *src++;
    for (i = 0; i < 8; i++) {
      mult = (crc & 1) ? CRCPOLY : 0;
      crc = (crc >> 1) ^ mult;
    }
  }

  return crc;
}
