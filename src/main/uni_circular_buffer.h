/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2019 Ricardo Quesada

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

#ifndef UNI_CIRCULAR_BUFFER_H
#define UNI_CIRCULAR_BUFFER_H

#include <stdint.h>

// UNI_CIRCULAR_BUFFER_SIZE represents how many packets can be queued
#define UNI_CIRCULAR_BUFFER_SIZE 6
// UNI_CIRCULAR_BUFFER_DATA_SIZE represents the max size of each packet
#define UNI_CIRCULAR_BUFFER_DATA_SIZE 128

enum {
  UNI_CIRCULAR_BUFFER_ERROR_OK = 0,
  UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL,
  UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY,
  UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG,
};

typedef struct uni_ciruclar_buffer_data_s {
  int16_t cid;
  uint8_t data[UNI_CIRCULAR_BUFFER_DATA_SIZE];
  int data_len;
} uni_circular_buffer_data_t;

typedef struct uni_circular_buffer_s {
  uni_circular_buffer_data_t buffer[UNI_CIRCULAR_BUFFER_SIZE];
  int16_t head_idx;
  int16_t tail_idx;
} uni_circular_buffer_t;

uint8_t uni_circular_buffer_put(uni_circular_buffer_t* b, int16_t cid,
                                const void* data, int len);
uint8_t uni_circular_buffer_get(uni_circular_buffer_t* b, int16_t* cid,
                                void** data, int* len);
uint8_t uni_circular_buffer_is_empty(uni_circular_buffer_t* b);
uint8_t uni_circular_buffer_is_full(uni_circular_buffer_t* b);
void uni_circular_buffer_reset(uni_circular_buffer_t* b);

#endif  // UNI_CIRCULAR_BUFFER_H
