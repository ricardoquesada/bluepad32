// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "bt/uni_bt_service.h"

#include <inttypes.h>

#include <btstack.h>

#include "bt/uni_bt.h"
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

// General Discoverable = 0x02
// BR/EDR Not supported = 0x04
#define APP_AD_FLAGS 0x06

// Where the 'XXXX' start. To be replaced with the two latest local bdaddr.
#define ADV_DATA_LOCAL_NAME_ADDR_OFFSET 15
// clang-format off
// Not const since the local name will be overwritten with the local address
static uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    // Name
    15, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,'B', 'l','u','e','p','a','d','3','2','-','X','X','X','X',
    // Incomplete List of 16-bit Service Class UUIDs -- AC00 - only valid for testing!
    3, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x00, 0xac,
};
static const uint8_t adv_data_len = sizeof(adv_data);
// clang-format on

static bool service_enabled = true;

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

    logi("att_read_callback handle: %#x\n", att_handle);
    switch (att_handle) {
        case ATT_CHARACTERISTIC_4627C4A4_AC01_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL_01_CLIENT_CONFIGURATION_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_MANUFACTURER_NAME_STRING_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_MODEL_NUMBER_STRING_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_SERIAL_NUMBER_STRING_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_HARDWARE_REVISION_STRING_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_FIRMWARE_REVISION_STRING_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_SOFTWARE_REVISION_STRING_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_SYSTEM_ID_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_IEEE_11073_20601_REGULATORY_CERTIFICATION_DATA_LIST_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_ORG_BLUETOOTH_CHARACTERISTIC_PNP_ID_01_VALUE_HANDLE:
            break;
        default:
            break;
    }
    return 0;
}

/* It configures the ATT Server with the pre-compiled ATT Database generated from the .gatt file.
 * Finally, it configures the advertisements.
 */
void uni_bt_service_init(void) {
    logi("Initializing Bluepad32 BLE service\n");
    // Setup ATT server.
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

    // Update LocalName with the latest 2 bytes of local addr.
    bd_addr_t local_addr;
    gap_local_bd_addr(local_addr);
    const char* addr_str = bd_addr_to_str(local_addr);
    memcpy(&adv_data[ADV_DATA_LOCAL_NAME_ADDR_OFFSET], &addr_str[12], 2);
    memcpy(&adv_data[ADV_DATA_LOCAL_NAME_ADDR_OFFSET+2], &addr_str[15], 2);
}

bool uni_bt_service_is_enabled() {
    return service_enabled;
}
void uni_bt_service_set_enabled(bool enabled) {
    service_enabled = enabled;
}