/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2023 Ricardo Quesada

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

#ifndef UNI_BT_ALLOWLIST_H
#define UNI_BT_ALLOWLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include <btstack.h>

//
// IMPORTANT:
// These functions are not %100 thread safe, but "safe-enough".
// If another task calls them, the worst case that can happen is a race condition
// where a connection is accepted/declined when it shouldn't.
// But no crashes should happen since no deletions/insertions are performed.
//

// Whether or not the address is allowed to connect.
bool uni_bt_allowlist_allow_addr(bd_addr_t addr);
bool uni_bt_allowlist_add_addr(bd_addr_t addr);
bool uni_bt_allowlist_remove_addr(bd_addr_t addr);
void uni_bt_allowlist_list(void);

bool uni_bt_allowlist_is_enabled(void);
void uni_bt_allowlist_set_enabled(bool enabled);

void uni_bt_allowlist_init(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_ALLOWLIST_H