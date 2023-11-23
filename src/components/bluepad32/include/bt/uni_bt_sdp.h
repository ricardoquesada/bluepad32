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