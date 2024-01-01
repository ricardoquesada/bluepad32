// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_SDP_H
#define UNI_BT_SDP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uni_hid_device.h"

void uni_bt_sdp_query_start(uni_hid_device_t* d);
void uni_bt_sdp_query_end(uni_hid_device_t* d);
void uni_bt_sdp_query_start_vid_pid(uni_hid_device_t* d);
void uni_bt_sdp_query_start_hid_descriptor(uni_hid_device_t* d);

void uni_bt_sdp_server_init(void);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_SDP_H