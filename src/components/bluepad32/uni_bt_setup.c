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

#include "uni_bt_setup.h"

#include <inttypes.h>

#include <btstack.h>

#include "sdkconfig.h"
#include "uni_ble.h"
#include "uni_bluetooth.h"
#include "uni_bt_defines.h"
#include "uni_bt_sdp.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_hci_cmd.h"
#include "uni_log.h"
#include "uni_platform.h"
#include "uni_property.h"

typedef enum {
    SETUP_STATE_BTSTACK_IN_PROGRESS,
    SETUP_STATE_BLUEPAD32_IN_PROGRESS,
    SETUP_STATE_READY,
} setup_state_t;

typedef uint8_t (*fn_t)(void);

static void maybe_delete_or_list_link_keys(void);
static void setup_call_next_fn(void);
static uint8_t setup_set_event_filter(void);
static uint8_t setup_write_simple_pairing_mode(void);

static int setup_fn_idx = 0;
static fn_t setup_fns[] = {
    &setup_write_simple_pairing_mode,
    &setup_set_event_filter,
};
static setup_state_t setup_state = SETUP_STATE_BTSTACK_IN_PROGRESS;
static btstack_packet_callback_registration_t hci_event_callback_registration;

static void maybe_delete_or_list_link_keys(void) {
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

    uni_ble_delete_bonded_keys();
}

static uint8_t setup_set_event_filter(void) {
    // Filter out inquiry results before we start the inquiry
    return hci_send_cmd(&hci_set_event_filter_inquiry_cod, 0x01, 0x01, UNI_BT_COD_MAJOR_PERIPHERAL,
                        UNI_BT_COD_MAJOR_MASK);
}

static uint8_t setup_write_simple_pairing_mode(void) {
    return hci_send_cmd(&hci_write_simple_pairing_mode, true);
}

static void setup_call_next_fn(void) {
    bd_addr_t event_addr;
    uint8_t status;

    if (!hci_can_send_command_packet_now()) {
        logi("HCI not ready, cannot send packet, will again try later. Current state idx=%d\n", setup_fn_idx);
        return;
    }

    // Assume it is safe to call an HCI command
    fn_t fn = setup_fns[setup_fn_idx];
    status = fn();
    if (status) {
        loge("Failed to call idx=%d, status=0x%02x... retrying...\n", setup_fn_idx, status);
        return;
    }

    setup_fn_idx++;
    if (setup_fn_idx == ARRAY_SIZE(setup_fns)) {
        setup_state = SETUP_STATE_READY;

        // If finished with the "setup" commands, just finish the setup
        // by printing some debug version.

        gap_local_bd_addr(event_addr);
        logi("BTstack up and running on %s.\n", bd_addr_to_str(event_addr));
        maybe_delete_or_list_link_keys();

        // Start inquiry now, once we know that HCI is running.
        status =
            gap_inquiry_periodic_start(UNI_BT_INQUIRY_LENGTH, UNI_BT_MAX_PERIODIC_LENGTH, UNI_BT_MIN_PERIODIC_LENGTH);
        if (status)
            loge("Failed to start period inquiry, error=0x%02x\n", status);

        uni_ble_scan_start();

        uni_get_platform()->on_init_complete();
        uni_get_platform()->on_oob_event(UNI_PLATFORM_OOB_BLUETOOTH_ENABLED, (void*)true);
    }
}

// Public functions

void uni_bt_setup_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    uint8_t event;

    ARG_UNUSED(channel);
    ARG_UNUSED(packet);
    ARG_UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;

    if (setup_state == SETUP_STATE_BLUEPAD32_IN_PROGRESS) {
        setup_call_next_fn();
        return;
    }

    event = hci_event_packet_get_type(packet);
    switch (event) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                setup_state = SETUP_STATE_BLUEPAD32_IN_PROGRESS;

                setup_call_next_fn();
            }
            break;
        case BTSTACK_EVENT_POWERON_FAILED:
            loge("Failed to initialize HCI. Please file a Bluepad32 bug\n");
            break;
        default:
            break;
    }
}

bool uni_bt_setup_is_ready() {
    return setup_state == SETUP_STATE_READY;
}

// Properties
void uni_bt_setup_set_gap_security_level(int gap) {
    uni_property_value_t val;

    val.u32 = gap;
    uni_property_set(UNI_PROPERTY_KEY_GAP_LEVEL, UNI_PROPERTY_TYPE_U32, val);
}

int uni_bt_setup_get_gap_security_level() {
    uni_property_value_t val;
    uni_property_value_t def;

    // It seems that with gap_security_level(0) all gamepads work except Nintendo Switch Pro controller.
#if CONFIG_BLUEPAD32_GAP_SECURITY
    def.u32 = 2;
#else
    def.u32 = 0;
#endif  // CONFIG_BLUEPAD32_GAP_SECURITY

    val = uni_property_get(UNI_PROPERTY_KEY_GAP_LEVEL, UNI_PROPERTY_TYPE_U32, def);
    return val.u32;
}

void uni_bt_setup_set_gap_inquiry_length(int len) {
    uni_property_value_t val;

    val.u8 = len;
    uni_property_set(UNI_PROPERTY_KEY_GAP_INQ_LEN, UNI_PROPERTY_TYPE_U8, val);
}

int uni_bt_setup_get_gap_inquiry_lenght(void) {
    uni_property_value_t val;
    uni_property_value_t def;

    def.u8 = UNI_BT_INQUIRY_LENGTH;
    val = uni_property_get(UNI_PROPERTY_KEY_GAP_INQ_LEN, UNI_PROPERTY_TYPE_U8, def);
    return val.u8;
}

void uni_bt_setup_set_gap_max_peridic_length(int len) {
    uni_property_value_t val;

    val.u8 = len;
    uni_property_set(UNI_PROPERTY_KEY_GAP_MAX_PERIODIC_LEN, UNI_PROPERTY_TYPE_U8, val);
}

int uni_bt_setup_get_gap_max_periodic_lenght(void) {
    uni_property_value_t val;
    uni_property_value_t def;

    def.u8 = UNI_BT_MAX_PERIODIC_LENGTH;
    val = uni_property_get(UNI_PROPERTY_KEY_GAP_MAX_PERIODIC_LEN, UNI_PROPERTY_TYPE_U8, def);
    return val.u8;
}

void uni_bt_setup_set_gap_min_peridic_length(int len) {
    uni_property_value_t val;

    val.u8 = len;
    uni_property_set(UNI_PROPERTY_KEY_GAP_MIN_PERIODIC_LEN, UNI_PROPERTY_TYPE_U8, val);
}

int uni_bt_setup_get_gap_min_periodic_lenght(void) {
    uni_property_value_t val;
    uni_property_value_t def;

    def.u8 = UNI_BT_MIN_PERIODIC_LENGTH;
    val = uni_property_get(UNI_PROPERTY_KEY_GAP_MIN_PERIODIC_LEN, UNI_PROPERTY_TYPE_U8, def);
    return val.u8;
}

int uni_bt_setup(void) {
    int gap = uni_bt_setup_get_gap_security_level();
    gap_set_security_level(gap);

    gap_connectable_control(1);

    // Enable once we add support for "BP32 BT Service"
    gap_discoverable_control(0);

    gap_set_page_scan_type(PAGE_SCAN_MODE_INTERLACED);
    // gap_set_page_timeout(0x2000);
    // gap_set_page_scan_activity(0x50, 0x12);
    // gap_inquiry_set_scan_activity(0x50, 0x12);

    // Initialize L2CAP
    l2cap_init();

    // Needed for some incoming connections
    uni_bt_sdp_server_init();

    int security_level = gap_get_security_level();
    logi("Gap security level: %d\n", security_level);
    logi("Periodic Inquiry: max=%d, min=%d, len=%d\n", uni_bt_setup_get_gap_max_periodic_lenght(),
         uni_bt_setup_get_gap_min_periodic_lenght(), uni_bt_setup_get_gap_inquiry_lenght());
    logi("Max connected gamepads: %d\n", CONFIG_BLUEPAD32_MAX_DEVICES);

    logi("BR/EDR support: enabled\n");
    logi("BLE support: %s\n", uni_ble_is_enabled() ? "enabled" : "disabled");

    l2cap_register_service(uni_bluetooth_packet_handler, BLUETOOTH_PSM_HID_INTERRUPT, UNI_BT_L2CAP_CHANNEL_MTU,
                           security_level);
    l2cap_register_service(uni_bluetooth_packet_handler, BLUETOOTH_PSM_HID_CONTROL, UNI_BT_L2CAP_CHANNEL_MTU,
                           security_level);

    // register for HCI events
    hci_event_callback_registration.callback = &uni_bluetooth_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Enable RSSI and EIR for gap_inquiry
    // TODO: Do we need EIR, since the name will be requested if not provided?
    hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);

    // Allow sniff mode requests by HID device and support role switch
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

    // btstack_stdin_setup(stdin_process);
    hci_set_master_slave_policy(HCI_ROLE_MASTER);

    if (uni_ble_is_enabled())
        uni_ble_setup();

    // Disable stdout buffering
    setbuf(stdout, NULL);

    // Turn on the device
    hci_power_control(HCI_POWER_ON);

    return 0;
}
