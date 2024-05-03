// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_H
#define UNI_H

#ifdef __cplusplus
extern "C" {
#endif

// An include file that includes all files.
// Useful for 3rd party developers that only care about including just one file.

#include "sdkconfig.h"

#include "bt/uni_bt.h"
#include "bt/uni_bt_allowlist.h"
#include "bt/uni_bt_bredr.h"
#include "bt/uni_bt_conn.h"
#include "bt/uni_bt_defines.h"
#include "bt/uni_bt_hci_cmd.h"
#include "bt/uni_bt_le.h"
#include "bt/uni_bt_sdp.h"
#include "bt/uni_bt_service.h"
#include "bt/uni_bt_setup.h"
#include "controller/uni_balance_board.h"
#include "controller/uni_controller.h"
#include "controller/uni_controller_type.h"
#include "controller/uni_gamepad.h"
#include "controller/uni_keyboard.h"
#include "controller/uni_mouse.h"
#include "parser/uni_hid_parser.h"
#include "parser/uni_hid_parser_ds5.h"
#include "parser/uni_hid_parser_keyboard.h"
#include "parser/uni_hid_parser_mouse.h"
#include "parser/uni_hid_parser_xboxone.h"
#include "platform/uni_platform.h"
#include "uni_circular_buffer.h"
#include "uni_console.h"
#include "uni_hid_device.h"
#include "uni_init.h"
#include "uni_joystick.h"
#include "uni_log.h"
#include "uni_mouse_quadrature.h"
#include "uni_property.h"
#include "uni_utils.h"
#include "uni_virtual_device.h"

#ifdef __cplusplus
}
#endif

#endif  // UNI_H
