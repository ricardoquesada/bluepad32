// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_ERROR_H
#define UNI_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UNI_ERROR_SUCCESS = 0,

    UNI_ERROR_INVALID_DEVICE = 1,
    UNI_ERROR_INVALID_CONTROLLER = 2,
    UNI_ERROR_NO_SLOTS = 3,
    UNI_ERROR_INIT_FAILED = 4,
    UNI_ERROR_IGNORE_DEVICE = 5,
} uni_error_t;

#ifdef __cplusplus
}
#endif

#endif  // UNI_ERROR_H
