// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_SERVICE_H
#define UNI_BT_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "uni_hid_device.h"

void uni_bt_service_init(void);
void uni_bt_service_deinit(void);
bool uni_bt_service_is_enabled();
void uni_bt_service_set_enabled(bool enabled);

// Callbacks from uni_hid_device that will be notified to the BLE client.
void uni_bt_service_on_device_ready(const uni_hid_device_t* d);
void uni_bt_service_on_device_connected(const uni_hid_device_t* d);
void uni_bt_service_on_device_disconnected(const uni_hid_device_t* d);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_SERVICE_H