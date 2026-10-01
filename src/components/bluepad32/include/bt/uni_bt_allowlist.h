// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_ALLOWLIST_H
#define UNI_BT_ALLOWLIST_H

#include <stdbool.h>

#include <btstack.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file uni_bt_allowlist.h
 * @brief Bluetooth connection allowlist interface.
 */

//
// IMPORTANT:
// These functions are not %100 thread safe, but "safe-enough".
// If another task calls them, the worst case that can happen is a race condition
// where a connection is accepted/declined when it shouldn't.
// But no crashes should happen since no deletions/insertions are performed.
//

// IMPORTANT:
// These functions modify NVS (non-volatile storage).
// If you add an address to the allow list, it will persist reboots.
// Similar if you enable or disable allow list.

/**
 * @brief Whether the address is allowed to connect.
 * @param addr The Bluetooth address to check.
 * @return true if allowed, false otherwise.
 */
bool uni_bt_allowlist_is_allowed_addr(bd_addr_t addr);

/**
 * @brief Add a new address to the allow list.
 * @param addr The Bluetooth address to add.
 * @return true on success, false otherwise.
 */
bool uni_bt_allowlist_add_addr(bd_addr_t addr);

/**
 * @brief Remove an existing address from the allow list.
 * @param addr The Bluetooth address to remove.
 * @return true on success, false otherwise.
 */
bool uni_bt_allowlist_remove_addr(bd_addr_t addr);

/**
 * @brief Remove all entries from the allow list.
 * @return true on success, false otherwise.
 */
bool uni_bt_allowlist_remove_all(void);

/**
 * @brief Print the allowed addresses to the console.
 */
void uni_bt_allowlist_list(void);

/**
 * @brief Return a pointer to the addresses.
 * Do not modify the returned data.
 * @param addresses Pointer to store the array of addresses.
 * @param total Pointer to store the total number of addresses.
 */
void uni_bt_allowlist_get_all(const bd_addr_t** addresses, int* total);

/**
 * @brief Whether the allowlist is enabled.
 * @return true if enabled, false otherwise.
 */
bool uni_bt_allowlist_is_enabled(void);

/**
 * @brief Enables/Disables the allowlist feature.
 * @param enabled true to enable, false to disable.
 */
void uni_bt_allowlist_set_enabled(bool enabled);

/**
 * @brief Initialize the Allowlist feature.
 */
void uni_bt_allowlist_init(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_ALLOWLIST_H