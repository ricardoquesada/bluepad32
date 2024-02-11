// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "bt/uni_bt_setup.h"

#include <btstack.h>

#include "sdkconfig.h"

#include "bt/uni_bt.h"
#include "bt/uni_bt_bredr.h"
#include "bt/uni_bt_defines.h"
#include "bt/uni_bt_hci_cmd.h"
#include "bt/uni_bt_le.h"
#include "bt/uni_bt_service.h"
#include "platform/uni_platform.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_log.h"

typedef enum {
    SETUP_STATE_BTSTACK_IN_PROGRESS,
    SETUP_STATE_BLUEPAD32_IN_PROGRESS,
    SETUP_STATE_READY,
} setup_state_t;

typedef uint8_t (*fn_t)(void);

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

// SDP
// #define MAX_ATTRIBUTE_VALUE_SIZE 300
// static uint8_t hid_descriptor_storage[MAX_ATTRIBUTE_VALUE_SIZE];

static uint8_t setup_set_event_filter(void) {
    // Filter out inquiry results before we start the inquiry
    return hci_send_cmd(&hci_set_event_filter_inquiry_cod, 0x01, 0x01, UNI_BT_COD_MAJOR_PERIPHERAL,
                        UNI_BT_COD_MAJOR_MASK);
}

static uint8_t setup_write_simple_pairing_mode(void) {
    return hci_send_cmd(&hci_write_simple_pairing_mode, true);
}

static void setup_call_next_fn(void) {
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

        // If finished with the "setup" commands, finish the setup
        // by printing some debug version.

        // Populate global variable here, and just once.
        gap_local_bd_addr(uni_local_bd_addr);

        // Only after all BT setup is done, call on_init_complete()
        uni_get_platform()->on_init_complete();

        // Platform can disable the service.
        if (IS_ENABLED(UNI_ENABLE_BLE) && uni_bt_service_is_enabled())
            uni_bt_service_init();
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

int uni_bt_setup(void) {
    bool bredr_enabled = false;
    bool ble_enabled = false;

    // Initialize L2CAP
    l2cap_init();

    if (IS_ENABLED(UNI_ENABLE_BREDR))
        bredr_enabled = uni_bt_bredr_is_enabled();
    if (IS_ENABLED(UNI_ENABLE_BLE))
        ble_enabled = uni_bt_le_is_enabled();

    logi("Max connected gamepads: %d\n", CONFIG_BLUEPAD32_MAX_DEVICES);

    logi("BR/EDR support: %s\n", bredr_enabled ? "enabled" : "disabled");
    logi("BLE support: %s\n", ble_enabled ? "enabled" : "disabled");

    // register for HCI events
    hci_event_callback_registration.callback = &uni_bt_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    if (IS_ENABLED(UNI_ENABLE_BREDR) && bredr_enabled)
        uni_bt_bredr_setup();

    if (IS_ENABLED(UNI_ENABLE_BLE) && ble_enabled)
        uni_bt_le_setup();

    // Initialize HID Host
    // hid_host_init(hid_descriptor_storage, sizeof(hid_descriptor_storage));
    // hid_host_register_packet_handler(uni_bt_packet_handler);

    // Turn on the device
    int err = hci_power_control(HCI_POWER_ON);
    if (err != 0) {
        loge("Failed to power on HCI, err = %x#\n", err);
        return UNI_ERROR_INIT_FAILED;
    }

    return UNI_ERROR_SUCCESS;
}
