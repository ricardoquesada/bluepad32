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

#ifndef UNI_COMMON_H
#define UNI_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

// Some common macros

#ifndef BIT
#define BIT(nr) (1UL << (nr))
#endif  // BIT

#ifndef BIT64
#define BIT64(nr) (1ULL << (nr))
#endif  // BIT64

#ifndef GENMASK
#define GENMASK(h, l) (((BIT(h) << 1) - 1) ^ (BIT(l) - 1))
#endif  // GENMASK

#ifndef GENMASK64
#define GENMASK64(h, l) (((BIT64(h) << 1) - 1) ^ (BIT64(l) - 1))
#endif  // GENMASK64

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#endif  // ARRAY_SIZE

#ifndef ARG_UNUSED
#define ARG_UNUSED(x) (void)(sizeof(x))
#endif  // ARG_UNUSED

#ifdef __cplusplus
}
#endif

#endif  // UNI_COMMON_H
