// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "uni_init.h"

#include "sdkconfig.h"

#include "bt/uni_bt_allowlist.h"
#include "bt/uni_bt_setup.h"
#include "platform/uni_platform.h"
#include "uni_config.h"
#include "uni_console.h"
#include "uni_hid_device.h"
#include "uni_log.h"
#include "uni_property.h"
#include "uni_version.h"
#include "uni_virtual_device.h"

int uni_init(int argc, const char** argv) {
    // Disable stdout buffering
    setbuf(stdout, NULL);

    loge("Bluepad32 (C) 2016-2024 Ricardo Quesada and contributors.\n");
    loge("Version: v" UNI_VERSION "\n");

    // Honoring BTstack license
    loge("BTstack: Copyright (C) 2017 BlueKitchen GmbH.\n");

    uni_property_init();
    uni_platform_init(argc, argv);
    uni_hid_device_setup();

    // Continue with bluetooth setup.
    uni_bt_setup();
    uni_bt_allowlist_init();
    uni_virtual_device_init();

#if CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
    uni_console_init();
#endif  // CONFIG_BLUEPAD32_CONSOLE_ENABLE

    uni_balance_board_init();

    return 0;
}
