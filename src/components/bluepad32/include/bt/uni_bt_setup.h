// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_BT_SETUP_H
#define UNI_BT_SETUP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

int uni_bt_setup(void);
bool uni_bt_setup_is_ready(void);
void uni_bt_setup_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif  // UNI_BT_SETUP_H