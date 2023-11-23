// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_H
#define UNI_H

#ifdef __cplusplus
extern "C" {
#endif

// A include file that includes all files.
// Useful for 3rd party developers that only cares about including just one file.

#include "sdkconfig.h"

#include "bt/uni_bt.h"
#include "bt/uni_bt_allowlist.h"
#include "bt/uni_bt_bredr.h"
#include "bt/uni_bt_conn.h"
#include "bt/uni_bt_le.h"
#include "bt/uni_bt_sdp.h"
#include "bt/uni_bt_setup.h"
#include "parser/uni_hid_parser.h"
#include "platform/uni_platform.h"
#include "uni_balance_board.h"
#include "uni_circular_buffer.h"
#include "uni_console.h"
#include "uni_controller.h"
#include "uni_esp32.h"
#include "uni_gamepad.h"
#include "uni_hci_cmd.h"
#include "uni_hid_device.h"
#include "uni_init.h"
#include "uni_joystick.h"
#include "uni_keyboard.h"
#include "uni_log.h"
#include "uni_mouse.h"
#include "uni_mouse_quadrature.h"
#include "uni_property.h"
#include "uni_property_mem.h"
#include "uni_property_nvs.h"
#include "uni_utils.h"
#include "uni_virtual_device.h"

#ifdef __cplusplus
}
#endif

#endif  // UNI_H
