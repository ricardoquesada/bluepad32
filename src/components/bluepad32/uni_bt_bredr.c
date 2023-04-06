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

#include "uni_bt_bredr.h"

#include <inttypes.h>
#include <stdbool.h>

#include <btstack.h>

#include "sdkconfig.h"
#include "uni_bluetooth.h"
#include "uni_bt_defines.h"
#include "uni_bt_sdp.h"
#include "uni_bt_setup.h"
#include "uni_config.h"
#include "uni_log.h"
#include "uni_platform.h"

static bool bt_bredr_enabled = true;

void uni_bt_bredr_scan_start(void) {
#ifdef UNI_ENABLE_BREDR
    uint8_t status;

    status =
        gap_inquiry_periodic_start(uni_bt_setup_get_gap_inquiry_lenght(), uni_bt_setup_get_gap_max_periodic_lenght(),
                                   uni_bt_setup_get_gap_min_periodic_lenght());
    if (status)
        loge("Failed to start period inquiry, error=0x%02x\n", status);
#endif  // UNI_ENABLE_BREDR
}

void uni_bt_bredr_scan_stop(void) {
#ifdef UNI_ENABLE_BREDR
    uint8_t status;

    status = gap_inquiry_stop();
    if (status)
        loge("Error: cannot stop inquiry (0x%02x), please try again\n", status);
#endif  // UNI_ENABLE_BREDR
}

// Called from uni_hid_device_disconnect()
void uni_bt_bredr_disconnect(uni_bt_conn_t* conn) {
#ifdef UNI_ENABLE_BREDR
    if (gap_get_connection_type(conn->handle) != GAP_CONNECTION_INVALID) {
        gap_disconnect(conn->handle);
        conn->handle = UNI_BT_CONN_HANDLE_INVALID;
    } else {
        // After calling gap_disconnect() we should not call l2cap_disonnect(),
        // since gap_disconnect() will take care of it.
        // But if the handle is not present, then call it manually.
        if (conn->control_cid) {
            l2cap_disconnect(conn->control_cid);
            conn->control_cid = 0;
        }

        if (conn->interrupt_cid) {
            l2cap_disconnect(conn->interrupt_cid);
            conn->interrupt_cid = 0;
        }
    }
#endif  // UNI_ENABLE_BREDR
}

void uni_bt_bredr_delete_bonded_keys(void) {
#ifdef UNI_ENABLE_BREDR
    bd_addr_t addr;
    link_key_t link_key;
    link_key_type_t type;
    btstack_link_key_iterator_t it;

    int32_t delete_keys = uni_get_platform()->get_property(UNI_PLATFORM_PROPERTY_DELETE_STORED_KEYS);
    if (delete_keys != 1)
        return;

    // BR/EDR
    int ok = gap_link_key_iterator_init(&it);
    if (!ok) {
        loge("Link key iterator not implemented\n");
        return;
    }

    logi("Deleting stored BR/ERD link keys:\n");
    while (gap_link_key_iterator_get_next(&it, addr, link_key, &type)) {
        logi("%s - type %u, key: ", bd_addr_to_str(addr), (int)type);
        printf_hexdump(link_key, 16);
        gap_drop_link_key_for_bd_addr(addr);
    }

    logi(".\n");
    gap_link_key_iterator_done(&it);
#endif  // UNI_ENABLE_BREDR
}

void uni_bt_bredr_list_bonded_keys(void) {
#ifdef UNI_ENABLE_BREDR
    bd_addr_t addr;
    link_key_t link_key;
    link_key_type_t type;
    btstack_link_key_iterator_t it;

    int ok = gap_link_key_iterator_init(&it);
    if (!ok) {
        loge("Link key iterator not implemented\n");
        return;
    }

    logi("Bluetooth BR/EDR keys:\n");
    while (gap_link_key_iterator_get_next(&it, addr, link_key, &type)) {
        logi("%s - type %u - key: ", bd_addr_to_str(addr), (int)type);
        printf_hexdump(link_key, 16);
    }
    gap_link_key_iterator_done(&it);
#endif  // UNI_ENABLE_BREDR
}

void uni_bt_bredr_setup(void) {
#ifdef UNI_ENABLE_BREDR
    int security_level = uni_bt_setup_get_gap_security_level();
    gap_set_security_level(security_level);

    gap_connectable_control(1);

    // Enable once we add support for "BP32 BT Service"
    gap_discoverable_control(0);

    gap_set_page_scan_type(PAGE_SCAN_MODE_INTERLACED);
    // gap_set_page_timeout(0x2000);
    // gap_set_page_scan_activity(0x50, 0x12);
    // gap_inquiry_set_scan_activity(0x50, 0x12);

    // Needed for some incoming connections
    uni_bt_sdp_server_init();

    l2cap_register_service(uni_bluetooth_packet_handler, BLUETOOTH_PSM_HID_INTERRUPT, UNI_BT_L2CAP_CHANNEL_MTU,
                           security_level);
    l2cap_register_service(uni_bluetooth_packet_handler, BLUETOOTH_PSM_HID_CONTROL, UNI_BT_L2CAP_CHANNEL_MTU,
                           security_level);

    // Allow sniff mode requests by HID device and support role switch
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

    // btstack_stdin_setup(stdin_process);
    hci_set_master_slave_policy(HCI_ROLE_MASTER);
#endif  // UNI_ENABLE_BREDR
}

void uni_bt_bredr_set_enabled(bool enabled) {
    if (enabled == bt_bredr_enabled)
        return;
    bt_bredr_enabled = enabled;
}

bool uni_bt_bredr_is_enabled(void) {
#ifdef UNI_ENABLE_BREDR
    return bt_bredr_enabled;
#else
    return false;
#endif
}