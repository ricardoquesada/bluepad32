// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "bt/uni_bt_service.h"

#include <inttypes.h>

#include <btstack.h>

#include "bt/uni_bt_service.gatt.h"
#include "uni_common.h"
#include "uni_log.h"

static int att_write_callback(hci_con_handle_t con_handle,
                              uint16_t att_handle,
                              uint16_t transaction_mode,
                              uint16_t offset,
                              uint8_t* buffer,
                              uint16_t buffer_size);
static uint16_t att_read_callback(hci_con_handle_t connection_handle,
                                  uint16_t att_handle,
                                  uint16_t offset,
                                  uint8_t* buffer,
                                  uint16_t buffer_size);
static void streamer(void);

// Flags general discoverable, BR/EDR supported (== not supported flag not set) when ENABLE_GATT_OVER_CLASSIC is enabled
#ifdef ENABLE_GATT_OVER_CLASSIC
#define APP_AD_FLAGS 0x02
#else
#define APP_AD_FLAGS 0x06
#endif

// clang-format off
// Not const since the local name will be overwritten with the local address
static uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    // Name
    21, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'B', 'l','u','e','p','a','d','3','2','-','X','X','X','X','X','X','X','X','X','X',
    // Incomplete List of 16-bit Service Class UUIDs -- AC00 - only valid for testing!
    3, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x00, 0xac,
};
static const uint8_t adv_data_len = sizeof(adv_data);
// clang-format on

static bool service_enabled = true;

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It initializes L2CAP, the Security Manager, and configures the ATT Server with the pre-compiled
 * ATT Database generated from $le_streamer.gatt$. Finally, it configures the advertisements
 * and boots the Bluetooth stack.
 */
static void le_streamer_setup(void) {
    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*)adv_data);
    gap_advertisements_enable(true);
}

/*
 * @section ATT Write
 *
 * @text The only valid ATT write in this example is to the Client Characteristic Configuration, which configures
 * notification and indication. If the ATT handle matches the client configuration handle, the new configuration value
 * is stored. If notifications get enabled, an ATT_EVENT_CAN_SEND_NOW is requested. See Listing attWrite.
 */
static int att_write_callback(hci_con_handle_t con_handle,
                              uint16_t att_handle,
                              uint16_t transaction_mode,
                              uint16_t offset,
                              uint8_t* buffer,
                              uint16_t buffer_size) {
    ARG_UNUSED(offset);

    switch (att_handle) {
        case ATT_CHARACTERISTIC_4627C4A4_AC01_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            logi("Value handle: %04x, mode=%#x, offset=%d, buffer=%p, buffer_size=%d\n", att_handle, transaction_mode,
                 offset, buffer, buffer_size);
            break;
        default:
            logi("Default Write to 0x%04x, len %u\n", att_handle, buffer_size);
            break;
    }
    return 0;
}

/*
 * @section ATT Read
 *
 * @text The ATT Server handles all reads to constant data. For dynamic data like the custom characteristic, the
 * registered att_read_callback is called. To handle long characteristics and long reads, the att_read_callback is first
 * called with buffer == NULL, to request the total value length. Then it will be called again requesting a chunk of the
 * value. See Listing attRead.
 */
static uint16_t att_read_callback(hci_con_handle_t connection_handle,
                                  uint16_t att_handle,
                                  uint16_t offset,
                                  uint8_t* buffer,
                                  uint16_t buffer_size) {
    ARG_UNUSED(connection_handle);

    logi("att_read_callback\n");

    if (att_handle == ATT_CHARACTERISTIC_4627C4A4_AC01_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE) {
        logi("conn handle: %#x, att handle: %#x, offset: %d, buffer: %p, buffer size: %d\n", connection_handle,
             att_handle, offset, buffer, buffer_size);
    }
    return 0;
}

void uni_bt_service_init(void) {
    logi("**** SERVER INIT ****\n");
    le_streamer_setup();
}

bool uni_bt_service_is_enabled() {
    return service_enabled;
}
void uni_bt_service_set_enabled(bool enabled) {
    service_enabled = enabled;
}