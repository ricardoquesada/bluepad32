// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "bt/uni_bt_service.h"

#include <btstack.h>

#include "bt/uni_bt.h"
#include "bt/uni_bt_le.h"
#include "bt/uni_bt_service.gatt.h"
#include "uni_common.h"
#include "uni_log.h"
#include "uni_version.h"

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

// General Discoverable = 0x02
// BR/EDR Not supported = 0x04
#define APP_AD_FLAGS 0x06

// clang-format off
static const uint8_t adv_data[] = {
    // Flags general discoverable
    2, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    // Name
    5, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,'B', 'P', '3', '2',
    // 4627C4A4-AC00-46B9-B688-AFC5C1BF7F63
    17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS,
    0x63, 0x7F, 0xBF, 0xC1, 0xC5, 0xAF, 0x88, 0xB6, 0xB9, 0x46, 0x00, 0xAC, 0xA4, 0xC4, 0x27, 0x46,

};
// clang-format on
static const uint8_t adv_data_len = sizeof(adv_data);

static bool service_enabled = true;

static int att_write_callback(hci_con_handle_t con_handle,
                              uint16_t att_handle,
                              uint16_t transaction_mode,
                              uint16_t offset,
                              uint8_t* buffer,
                              uint16_t buffer_size) {
    ARG_UNUSED(offset);

    switch (att_handle) {
        case ATT_CHARACTERISTIC_4627C4A4_AC03_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Whether to enable BLE connections
            if (buffer_size != 1)
                return 0;
            const bool enabled = buffer[0];
            uni_bt_le_set_enabled(enabled);
            return 1;
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC04_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Scan for new connections
            if (buffer_size != 1)
                return 0;
            const bool enabled = buffer[0];
            uni_bt_enable_new_connections_unsafe(enabled);
            return 1;
        }
        default:
            logi("Default Write to 0x%04x, len %u\n", att_handle, buffer_size);
            break;
    }
    return 0;
}

static uint16_t att_read_callback(hci_con_handle_t connection_handle,
                                  uint16_t att_handle,
                                  uint16_t offset,
                                  uint8_t* buffer,
                                  uint16_t buffer_size) {
    ARG_UNUSED(connection_handle);

    switch (att_handle) {
        case ATT_CHARACTERISTIC_4627C4A4_AC01_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            // version
            return att_read_callback_handle_blob((const uint8_t*)uni_version, (uint16_t)strlen(uni_version), offset,
                                                 buffer, buffer_size);
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC02_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Max supported connections
            const uint8_t max = CONFIG_BLUEPAD32_MAX_DEVICES;
            return att_read_callback_handle_blob(&max, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC03_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Whether to enable BLE connections
            const uint8_t enabled = uni_bt_le_is_enabled();
            return att_read_callback_handle_blob(&enabled, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC04_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE: {
            // Scan for new connections
            const uint8_t scanning = uni_bt_enable_new_connections_is_enabled();
            return att_read_callback_handle_blob(&scanning, (uint16_t)1, offset, buffer, buffer_size);
        }
        case ATT_CHARACTERISTIC_4627C4A4_AC05_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC06_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC07_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC08_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC09_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC0A_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC0B_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC0C_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
            break;
        case ATT_CHARACTERISTIC_4627C4A4_AC0D_46B9_B688_AFC5C1BF7F63_01_VALUE_HANDLE:
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

/*
 * Configures the ATT Server with the pre-compiled ATT Database generated from the .gatt file.
 * Finally, it configures the advertisements.
 */
void uni_bt_service_init(void) {
    logi("Starting Bluepad32 BLE service UUID: 4627C4A4-AC00-46B9-B688-AFC5C1BF7F63\n");
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
}

bool uni_bt_service_is_enabled() {
    return service_enabled;
}
void uni_bt_service_set_enabled(bool enabled) {
    service_enabled = enabled;
}