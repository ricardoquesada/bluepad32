/*
 * Copyright (C) 2017 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

/*
 * Copyright (C) 2023 Ricardo Quesada
 * Unijoysticle additions based on BlueKitchen's test/example code
 */

/*
 * Execution order:
 *  uni_ble_on_gap_event_advertising_report()
 *      -> hog_connect()
 *  sm_packet_handler()
 *  device_information_packet_handler()
 *  hids_client_packet_handler()
 *  uni_hid_device_set_ready()
 */

#include "uni_ble.h"

#include <btstack.h>
#include <btstack_config.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "uni_bt_conn.h"
#include "uni_bt_defines.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_log.h"
#include "uni_property.h"

static bool is_scanning;

// Temporal space for SDP in BLE
static uint8_t hid_descriptor_storage[500];
static btstack_packet_callback_registration_t sm_event_callback_registration;

/**
 * Connect to remote device but set timer for timeout
 */
static void hog_connect(bd_addr_t addr, bd_addr_type_t addr_type) {
    // Stop scan, otherwise it will be able to connect.
    // Happens in ESP32, but not in libusb
    gap_stop_scan();
    logi("BLE scan -> 0\n");

    gap_connect(addr, addr_type);
}

static void resume_scanning_hint(void) {
    // Resume scanning, only if it was scanning before connecting
    if (is_scanning) {
        gap_start_scan();
        logi("BLE scan -> 1\n");
    }
}

static void hog_disconnect(hci_con_handle_t con_handle) {
    // MUST not call uni_hid_device_disconnect(), called from it.
    uni_hid_device_t* device;

    loge("**** hog_disconnect()\n");
    if (gap_get_connection_type(con_handle) != GAP_CONNECTION_INVALID)
        gap_disconnect(con_handle);
    device = uni_hid_device_get_instance_for_connection_handle(con_handle);

    hids_client_disconnect(device->hids_cid);
    resume_scanning_hint();
}

static void get_advertisement_data(const uint8_t* adv_data, uint8_t adv_size, uint16_t* appearance, char* name) {
    ad_context_t context;

    for (ad_iterator_init(&context, adv_size, (uint8_t*)adv_data); ad_iterator_has_more(&context);
         ad_iterator_next(&context)) {
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t size = ad_iterator_get_data_len(&context);
        const uint8_t* data = ad_iterator_get_data(&context);

        int i;
        // Assigned Numbers GAP

        switch (data_type) {
            case BLUETOOTH_DATA_TYPE_FLAGS:
                break;
            case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_LIST_OF_16_BIT_SERVICE_SOLICITATION_UUIDS:
                break;
            case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_32_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_32_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_LIST_OF_32_BIT_SERVICE_SOLICITATION_UUIDS:
                break;
            case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_LIST_OF_128_BIT_SERVICE_SOLICITATION_UUIDS:
                break;
            case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                for (i = 0; i < size; i++) {
                    name[i] = data[i];
                }
                name[size] = 0;
                break;
            case BLUETOOTH_DATA_TYPE_TX_POWER_LEVEL:
                break;
            case BLUETOOTH_DATA_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE:
                break;
            case BLUETOOTH_DATA_TYPE_SERVICE_DATA:
                break;
            case BLUETOOTH_DATA_TYPE_PUBLIC_TARGET_ADDRESS:
            case BLUETOOTH_DATA_TYPE_RANDOM_TARGET_ADDRESS:
                break;
            case BLUETOOTH_DATA_TYPE_APPEARANCE:
                // https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.gap.appearance.xml
                *appearance = little_endian_read_16(data, 0);
                break;
            case BLUETOOTH_DATA_TYPE_ADVERTISING_INTERVAL:
                break;
            case BLUETOOTH_DATA_TYPE_3D_INFORMATION_DATA:
                break;
            case BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA:  // Manufacturer Specific Data
                break;
            case BLUETOOTH_DATA_TYPE_CLASS_OF_DEVICE:
                logi("class of device: %#x\n", little_endian_read_16(data, 0));
                break;
            case BLUETOOTH_DATA_TYPE_SIMPLE_PAIRING_HASH_C:
            case BLUETOOTH_DATA_TYPE_SIMPLE_PAIRING_RANDOMIZER_R:
            case BLUETOOTH_DATA_TYPE_DEVICE_ID:
                logi("device id: %#x\n", little_endian_read_16(data, 0));
                break;
            case BLUETOOTH_DATA_TYPE_SECURITY_MANAGER_OUT_OF_BAND_FLAGS:
            default:
                printf("Advertising Data Type 0x%2x not handled yet", data_type);
                break;
        }
    }
}

static void adv_event_get_data(const uint8_t* packet, uint16_t* appearance, char* name) {
    const uint8_t* ad_data;
    uint16_t ad_len;

    ad_data = gap_event_advertising_report_get_data(packet);
    ad_len = gap_event_advertising_report_get_data_length(packet);

    // if (!ad_data_contains_uuid16(ad_len, ad_data, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE))
    get_advertisement_data(ad_data, ad_len, appearance, name);
}

static void parse_report(uint8_t* packet, uint16_t size) {
    uint16_t service_index;
    uint16_t hids_cid;
    uni_hid_device_t* device;
    const uint8_t* descriptor_data;
    uint16_t descriptor_len;
    const uint8_t* report_data;
    uint16_t report_len;

    ARG_UNUSED(size);

    service_index = gattservice_subevent_hid_report_get_service_index(packet);
    hids_cid = gattservice_subevent_hid_report_get_hids_cid(packet);
    device = uni_hid_device_get_instance_for_hids_cid(hids_cid);

    if (!device) {
        loge("BLE parser report: Invalid device for hids_cid=%d\n", hids_cid);
        return;
    }

    // FIXME: Copying the HID descriptor should be done at setup time since some device, like Xbox requires it
    // to set the correct parser.
    // But not clear how to get the "service_index" from setup
    if (device->hid_descriptor_len == 0) {
        descriptor_data = hids_client_descriptor_storage_get_descriptor_data(hids_cid, service_index);
        descriptor_len = hids_client_descriptor_storage_get_descriptor_len(hids_cid, service_index);

        device->hid_descriptor_len = descriptor_len;
        memcpy(device->hid_descriptor, descriptor_data, sizeof(device->hid_descriptor));
    }
    report_data = gattservice_subevent_hid_report_get_report(packet);
    report_len = gattservice_subevent_hid_report_get_report_len(packet);

    uni_hid_parse_input_report(device, report_data, report_len);
    uni_hid_device_process_controller(device);
}

static void hids_client_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    uint8_t status;
    uint16_t hids_cid;
    uni_hid_device_t* device;
    uint8_t event_type;
    hid_protocol_mode_t protocol_mode;

    ARG_UNUSED(packet_type);
    ARG_UNUSED(channel);
    ARG_UNUSED(size);

#if 0
    // FIXME: Bug in BTStack??? This comparison fails because packet_type is HCI_EVENT_GATTSERVICE_META
    if (packet_type != HCI_EVENT_PACKET) {
        loge("hids_client_packet_handler: unsupported packet type: %#x\n", packet_type);
        return;
    }
#endif

    event_type = hci_event_packet_get_type(packet);
    if (event_type != HCI_EVENT_GATTSERVICE_META) {
        loge("hids_client_packet_handler: unsupported event type: %#x\n", event_type);
        return;
    }

    switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
        case GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED:
            status = gattservice_subevent_hid_service_connected_get_status(packet);
            logi("GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED, status=0x%02x\n", status);
            switch (status) {
                case ERROR_CODE_SUCCESS:
                    protocol_mode = gattservice_subevent_hid_service_connected_get_protocol_mode(packet);
                    logi("HID service client connected, found %d services, protocol_mode=%d\n",
                         gattservice_subevent_hid_service_connected_get_num_instances(packet), protocol_mode);

                    // XXX TODO: store device as bonded
                    hids_cid = gattservice_subevent_hid_service_connected_get_hids_cid(packet);
                    device = uni_hid_device_get_instance_for_hids_cid(hids_cid);
                    if (!device) {
                        loge("Hids Cid: Could not find valid device for hids_cid=%d\n", hids_cid);
                        break;
                    }
                    uni_hid_device_guess_controller_type_from_pid_vid(device);
                    uni_hid_device_connect(device);
                    uni_hid_device_set_ready(device);
                    resume_scanning_hint();
                    break;
                default:
                    loge("HID service client connection failed, err 0x%02x.\n", status);
                    break;
            }
            break;

        case GATTSERVICE_SUBEVENT_HID_REPORT:
            logd("GATTSERVICE_SUBEVENT_HID_REPORT\n");
            parse_report(packet, size);
            break;
        case GATTSERVICE_SUBEVENT_HID_INFORMATION:
            logi(
                "Hid Information: service index %d, USB HID 0x%02X, country code %d, remote wake %d, normally "
                "connectable %d\n",
                gattservice_subevent_hid_information_get_service_index(packet),
                gattservice_subevent_hid_information_get_base_usb_hid_version(packet),
                gattservice_subevent_hid_information_get_country_code(packet),
                gattservice_subevent_hid_information_get_remote_wake(packet),
                gattservice_subevent_hid_information_get_normally_connectable(packet));
            break;

        case GATTSERVICE_SUBEVENT_HID_PROTOCOL_MODE:
            logi("Protocol Mode: service index %d, mode 0x%02X (Boot mode: 0x%02X, Report mode 0x%02X)\n",
                 gattservice_subevent_hid_protocol_mode_get_service_index(packet),
                 gattservice_subevent_hid_protocol_mode_get_protocol_mode(packet), HID_PROTOCOL_MODE_BOOT,
                 HID_PROTOCOL_MODE_REPORT);
            break;

        case GATTSERVICE_SUBEVENT_HID_SERVICE_REPORTS_NOTIFICATION:
            if (gattservice_subevent_hid_service_reports_notification_get_configuration(packet) == 0) {
                logi("Reports disabled\n");
            } else {
                logi("Reports enabled\n");
            }
            break;
        default:
            logi("Unsupported gatt client event: 0x%02x\n", hci_event_gattservice_meta_get_subevent_code(packet));
            break;
    }
}

static void device_information_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    uint8_t code;
    uint8_t status;
    uint8_t att_status;
    hci_con_handle_t con_handle;
    uni_hid_device_t* device;
    uint8_t event_type;
    uint16_t hids_cid;

    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) {
        loge("device_information_packet_handler: unsupported packet type: %#x\n", packet_type);
        return;
    }

    event_type = hci_event_packet_get_type(packet);
    if (event_type != HCI_EVENT_GATTSERVICE_META) {
        loge("device_information_packet_handler: unsupported event type: %#x\n", event_type);
        return;
    }

    code = hci_event_gattservice_meta_get_subevent_code(packet);
    switch (code) {
        case GATTSERVICE_SUBEVENT_SCAN_PARAMETERS_SERVICE_CONNECTED:
            logi("PnP ID: vendor source ID 0x%02X, vendor ID 0x%02X, product ID 0x%02X, product version 0x%02X\n",
                 gattservice_subevent_device_information_pnp_id_get_vendor_source_id(packet),
                 gattservice_subevent_device_information_pnp_id_get_vendor_id(packet),
                 gattservice_subevent_device_information_pnp_id_get_product_id(packet),
                 gattservice_subevent_device_information_pnp_id_get_product_version(packet));
            break;

        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_DONE:
            status = gattservice_subevent_device_information_done_get_att_status(packet);
            con_handle = gattservice_subevent_device_information_done_get_con_handle(packet);
            switch (status) {
                case ERROR_CODE_SUCCESS:
                    loge("Device Information service found\n");
                    device = uni_hid_device_get_instance_for_connection_handle(con_handle);
                    if (!device) {
                        loge("Device not found!\n");
                        break;
                    }

                    // continue - query primary services
                    logi("Search for HID service.\n");
                    status = hids_client_connect(con_handle, hids_client_packet_handler, HID_PROTOCOL_MODE_REPORT,
                                                 &hids_cid);
                    if (status != ERROR_CODE_SUCCESS) {
                        logi("HID client connection failed, status 0x%02x\n", status);
                        hog_disconnect(con_handle);
                        break;
                    }
                    logi("Using hids_cid=%d\n", hids_cid);
                    device->hids_cid = hids_cid;
                    break;
                default:
                    logi("Device Information service client connection failed, err 0x%02x.\n", status);
                    hog_disconnect(con_handle);
                    break;
            }
            break;
        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_MANUFACTURER_NAME:
            att_status = gattservice_subevent_device_information_manufacturer_name_get_att_status(packet);
            if (att_status != ATT_ERROR_SUCCESS) {
                logi("Manufacturer Name read failed, ATT Error 0x%02x\n", att_status);
            } else {
                logi("Manufacturer Name: %s\n",
                     gattservice_subevent_device_information_manufacturer_name_get_value(packet));
            }
            break;
        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_MODEL_NUMBER:
            att_status = gattservice_subevent_device_information_model_number_get_att_status(packet);
            if (att_status != ATT_ERROR_SUCCESS) {
                logi("Model Number read failed, ATT Error 0x%02x\n", att_status);
            } else {
                logi("Model Number:     %s\n", gattservice_subevent_device_information_model_number_get_value(packet));
            }
            break;

        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_SERIAL_NUMBER:
            att_status = gattservice_subevent_device_information_serial_number_get_att_status(packet);
            if (att_status != ATT_ERROR_SUCCESS) {
                logi("Serial Number read failed, ATT Error 0x%02x\n", att_status);
            } else {
                logi("Serial Number:    %s\n", gattservice_subevent_device_information_serial_number_get_value(packet));
            }
            break;

        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_HARDWARE_REVISION:
            att_status = gattservice_subevent_device_information_hardware_revision_get_att_status(packet);
            if (att_status != ATT_ERROR_SUCCESS) {
                logi("Hardware Revision read failed, ATT Error 0x%02x\n", att_status);
            } else {
                logi("Hardware Revision: %s\n",
                     gattservice_subevent_device_information_hardware_revision_get_value(packet));
            }
            break;

        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_FIRMWARE_REVISION:
            att_status = gattservice_subevent_device_information_firmware_revision_get_att_status(packet);
            if (att_status != ATT_ERROR_SUCCESS) {
                logi("Firmware Revision read failed, ATT Error 0x%02x\n", att_status);
            } else {
                logi("Firmware Revision: %s\n",
                     gattservice_subevent_device_information_firmware_revision_get_value(packet));
            }
            break;

        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_SOFTWARE_REVISION:
            att_status = gattservice_subevent_device_information_software_revision_get_att_status(packet);
            if (att_status != ATT_ERROR_SUCCESS) {
                logi("Software Revision read failed, ATT Error 0x%02x\n", att_status);
            } else {
                logi("Software Revision: %s\n",
                     gattservice_subevent_device_information_software_revision_get_value(packet));
            }
            break;

        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_SYSTEM_ID:
            att_status = gattservice_subevent_device_information_system_id_get_att_status(packet);
            if (att_status != ATT_ERROR_SUCCESS) {
                logi("System ID read failed, ATT Error 0x%02x\n", att_status);
            } else {
                uint32_t manufacturer_identifier_low =
                    gattservice_subevent_device_information_system_id_get_manufacturer_id_low(packet);
                uint8_t manufacturer_identifier_high =
                    gattservice_subevent_device_information_system_id_get_manufacturer_id_high(packet);

                logi("Manufacturer ID:  0x%02x%08x\n", manufacturer_identifier_high, manufacturer_identifier_low);
                logi("Organizationally Unique ID:  0x%06x\n",
                     gattservice_subevent_device_information_system_id_get_organizationally_unique_id(packet));
            }
            break;

        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_IEEE_REGULATORY_CERTIFICATION:
            att_status = gattservice_subevent_device_information_ieee_regulatory_certification_get_att_status(packet);
            if (att_status != ATT_ERROR_SUCCESS) {
                logi("IEEE Regulatory Certification read failed, ATT Error 0x%02x\n", att_status);
            } else {
                logi("value_a:          0x%04x\n",
                     gattservice_subevent_device_information_ieee_regulatory_certification_get_value_a(packet));
                logi("value_b:          0x%04x\n",
                     gattservice_subevent_device_information_ieee_regulatory_certification_get_value_b(packet));
            }
            break;

        case GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_PNP_ID:
            con_handle = gattservice_subevent_device_information_pnp_id_get_con_handle(packet);
            device = uni_hid_device_get_instance_for_connection_handle(con_handle);
            if (!device) {
                loge("Invalid device for in GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_PNP_ID");
                break;
            }

            att_status = gattservice_subevent_device_information_pnp_id_get_att_status(packet);
            if (att_status != ATT_ERROR_SUCCESS) {
                logi("PNP ID read failed, ATT Error 0x%02x\n", att_status);
            } else {
                logi("Vendor Source ID: 0x%02x\n",
                     gattservice_subevent_device_information_pnp_id_get_vendor_source_id(packet));
                logi("Vendor  ID:       0x%04x\n",
                     gattservice_subevent_device_information_pnp_id_get_vendor_id(packet));
                logi("Product ID:       0x%04x\n",
                     gattservice_subevent_device_information_pnp_id_get_product_id(packet));
                logi("Product Version:  0x%04x\n",
                     gattservice_subevent_device_information_pnp_id_get_product_version(packet));
            }
            uni_hid_device_set_vendor_id(device, gattservice_subevent_device_information_pnp_id_get_vendor_id(packet));
            uni_hid_device_set_product_id(device,
                                          gattservice_subevent_device_information_pnp_id_get_product_id(packet));

            break;

        default:
            logi("Unknown gattservice meta subevent code: %#x\n", code);
            break;
    }
}

/* HCI packet handler
 *
 * text The SM packet handler receives Security Manager Events required for
 * pairing. It also receives events generated during Identity Resolving see
 * Listing SMPacketHandler.
 */
static void sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    bd_addr_t addr;
    uni_hid_device_t* device;
    uint8_t status;
    uint8_t type;

    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) {
        loge("sm_packet_handler: unsupported packet type: %#x\n", packet_type);
        return;
    }

    type = hci_event_packet_get_type(packet);
    switch (type) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            logi("Just works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            logi("Confirming numeric comparison: %" PRIu32 "\n",
                 sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            logi("Display Passkey: %" PRIu32 "\n", sm_event_passkey_display_number_get_passkey(packet));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_STARTED:
            logi("SM_EVENT_PAIRING_STARTED\n");
            break;
        case SM_EVENT_IDENTITY_RESOLVING_FAILED:
            sm_event_identity_created_get_address(packet, addr);
            logi("Identity resolving failed for %s\n\n", bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
            sm_event_identity_resolving_succeeded_get_identity_address(packet, addr);
            logi("Identity resolved: type %u address %s\n",
                 sm_event_identity_resolving_succeeded_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_PAIRING_STARTED:
            logi("SM_EVENT_PAIRING_STARTED\n");
            break;
        case SM_EVENT_IDENTITY_CREATED:
            sm_event_identity_created_get_identity_address(packet, addr);
            logi("Identity created: type %u address %s\n", sm_event_identity_created_get_identity_addr_type(packet),
                 bd_addr_to_str(addr));
            break;
        case SM_EVENT_REENCRYPTION_STARTED:
            sm_event_reencryption_complete_get_address(packet, addr);
            logi("Bonding information exists for addr type %u, identity addr %s -> start re-encryption\n",
                 sm_event_reencryption_started_get_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_REENCRYPTION_COMPLETE:
            switch (sm_event_reencryption_complete_get_status(packet)) {
                case ERROR_CODE_SUCCESS:
                    logi("Re-encryption complete, success\n");
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    logi("Re-encryption failed, timeout\n");
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    logi("Re-encryption failed, disconnected\n");
                    break;
                case ERROR_CODE_PIN_OR_KEY_MISSING:
                    logi("Re-encryption failed, bonding information missing\n\n");
                    logi("Assuming remote lost bonding information\n");
                    logi("Deleting local bonding information and start new pairing...\n");
                    sm_event_reencryption_complete_get_address(packet, addr);
                    type = sm_event_reencryption_started_get_addr_type(packet);
                    gap_delete_bonding(type, addr);
                    sm_request_pairing(sm_event_reencryption_complete_get_handle(packet));
                    break;
                default:
                    break;
            }
            break;
        case SM_EVENT_PAIRING_COMPLETE:
            sm_event_pairing_complete_get_address(packet, addr);
            device = uni_hid_device_get_instance_for_address(addr);
            if (!device) {
                loge("SM_EVENT_PAIRING_COMPLETE: Invalid device\n");
                break;
            }

            status = sm_event_pairing_complete_get_status(packet);
            switch (status) {
                case ERROR_CODE_SUCCESS:
                    logi("Pairing complete, success\n");
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    logi("Pairing failed, timeout\n");
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    logi("Pairing failed, disconnected\n");
                    break;
                case ERROR_CODE_AUTHENTICATION_FAILURE:
                    logi("Pairing failed, reason = %u\n", sm_event_pairing_complete_get_reason(packet));
                    break;
                default:
                    loge("Unkown paring status: %#x\n", status);
                    break;
            }

            if (status == ERROR_CODE_SUCCESS)
                return;

            // TODO: Double check
            // Do not disconnect. Sometimes it appears as "failure" although
            // the connection as Ok (???)
            // hog_disconnect(device->conn.handle);
            break;

        default:
            loge("Unkown SM packet type: %#x\n", type);
            break;
    }
}

void uni_ble_on_hci_event_le_meta(const uint8_t* packet, uint16_t size) {
    uni_hid_device_t* device;
    hci_con_handle_t con_handle;
    bd_addr_t event_addr;
    uint8_t subevent;

    ARG_UNUSED(size);

    subevent = hci_event_le_meta_get_subevent_code(packet);

    switch (subevent) {
        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
            hci_subevent_le_connection_complete_get_peer_address(packet, event_addr);
            device = uni_hid_device_get_instance_for_address(event_addr);
            if (!device) {
                loge("uni_ble_on_connection_complete: Device not found for addr: %s\n", bd_addr_to_str(event_addr));
                break;
            }
            con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
            logi("Using con_handle: %#x\n", con_handle);

            uni_hid_device_set_connection_handle(device, con_handle);
            sm_request_pairing(con_handle);

            // Resume scanning
            // gap_start_scan();
            break;

        case HCI_SUBEVENT_LE_ADVERTISING_REPORT:
            // Safely ignore it, we handle the GAP advertising report instead
            break;

        case HCI_SUBEVENT_LE_READ_REMOTE_FEATURES_COMPLETE:
            // Ignore it
            break;

        default:
            logi("Unsupported LE_META sub-event: %#x\n", subevent);
            break;
    }
}

void uni_ble_on_hci_event_encryption_change(const uint8_t* packet, uint16_t size) {
    uni_hid_device_t* device;
    hci_con_handle_t con_handle;
    uint8_t status;

    ARG_UNUSED(size);

    // Might be called from BR/EDR connections.
    // Only handle BLE in this function.
    con_handle = hci_event_encryption_change_get_connection_handle(packet);
    if (gap_get_connection_type(con_handle) != GAP_CONNECTION_LE)
        return;

    device = uni_hid_device_get_instance_for_connection_handle(con_handle);
    if (!device) {
        loge("uni_ble_on_encryption_change: Device not found for connection handle: 0x%04x\n", con_handle);
        return;
    }
    // This event is also triggered by Classic, and might crash the stack.
    // Real case: Connect a Wii , disconnect it, and try re-connection
    if (device->conn.protocol != UNI_BT_CONN_PROTOCOL_BLE)
        // Abort on non BLE connections
        return;

    logi("Connection encrypted: %u\n", hci_event_encryption_change_get_encryption_enabled(packet));
    if (hci_event_encryption_change_get_encryption_enabled(packet) == 0) {
        logi("Encryption failed -> abort\n");
        hog_disconnect(con_handle);
        return;
    }

    status = device_information_service_client_query(con_handle, device_information_packet_handler);
    if (status != ERROR_CODE_SUCCESS) {
        loge("Failed to set device information client: %#x\n", status);
    }
}

void uni_ble_on_gap_event_advertising_report(const uint8_t* packet, uint16_t size) {
    bd_addr_t addr;
    bd_addr_type_t addr_type;
    uint16_t appearance;
    char name[64];

    appearance = 0;
    name[0] = 0;

    ARG_UNUSED(size);

    gap_event_advertising_report_get_address(packet, addr);
    adv_event_get_data(packet, &appearance, name);

    if (appearance != UNI_BT_HID_APPEARANCE_GAMEPAD && appearance != UNI_BT_HID_APPEARANCE_JOYSTICK &&
        appearance != UNI_BT_HID_APPEARANCE_MOUSE)
        return;

    addr_type = gap_event_advertising_report_get_address_type(packet);
    if (uni_hid_device_get_instance_for_address(addr)) {
        // Ignore, address already found
        return;
    }

    logi("Found, connect to device with %s address %s ...\n", addr_type == 0 ? "public" : "random",
         bd_addr_to_str(addr));

    uni_hid_device_t* d = uni_hid_device_create(addr);
    if (!d) {
        loge("Error: no more available device slots\n");
        return;
    }

    // FIXME: Using CODs to make it compatible with legacy BR/EDR code.
    switch (appearance) {
        case UNI_BT_HID_APPEARANCE_MOUSE:
            uni_hid_device_set_cod(d, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE);
            logi("Device '%s' identified as Mouse\n", name);
            break;
        case UNI_BT_HID_APPEARANCE_JOYSTICK:
            uni_hid_device_set_cod(d, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_JOYSTICK);
            logi("Device '%s' identified as Joystick\n", name);
            break;
        case UNI_BT_HID_APPEARANCE_GAMEPAD:
            uni_hid_device_set_cod(d, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD);
            logi("Device '%s' identified as Gamepad\n", name);
            break;
        default:
            loge("Unsupported appearance: %#x\n", appearance);
            break;
    }
    uni_bt_conn_set_protocol(&d->conn, UNI_BT_CONN_PROTOCOL_BLE);
    uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_DEVICE_DISCOVERED);
    uni_hid_device_set_name(d, name);

    hog_connect(addr, addr_type);
}

void uni_ble_delete_bonded_keys(void) {
    bd_addr_t entry_address;
    int i;

    if (!uni_ble_is_enabled())
        return;

    logi("Deleting stored BLE link keys:\n");

    for (i = 0; i < le_device_db_max_count(); i++) {
        int entry_address_type = (int)BD_ADDR_TYPE_UNKNOWN;
        le_device_db_info(i, &entry_address_type, entry_address, NULL);

        // skip unused entries
        if (entry_address_type == (int)BD_ADDR_TYPE_UNKNOWN)
            continue;

        logi("%s - type %u\n", bd_addr_to_str(entry_address), (int)entry_address_type);
        gap_delete_bonding((bd_addr_type_t)entry_address_type, entry_address);
    }
    logi(".\n");
}

void uni_ble_setup(void) {
    // register for events from Security Manager
    sm_event_callback_registration.callback = &sm_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // Setup LE device db
    le_device_db_init();

    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

    // TL;DR:
    // Enable Secure connection, disable bonding

    // Legacy paring, Just Works in ESP32
    // - Stadia: Ok
    // - MS mouse: Ok
    // - Xbox 3 buttons: flaky, fails to connect or connects
    // - Xbox 2 buttons: flaky, fails to connect or connects
    // sm_set_authentication_requirements(0);

    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);

    // Secure connection + NO bonding in ESP32:
    // - Stadia: Ok
    // - MS mouse: Ok
    // - Xbox 3 buttons: Ok
    // - Xbox 2 buttons: fails to connect
    // sm_set_secure_connections_only_mode(true);
    // gap_set_secure_connections_only_mode(true);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION);

    // Secure connection + bonding in ESP32:
    // - Stadia: Ok
    // - MS mouse: Ok... but disconnects after 10 seconds
    // - Xbox 3 buttons: fails to connect
    // - Xbox 2 buttons: fails to connect
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);

    // libusb works with mostly any configuration

    gatt_client_init();
    hids_client_init(hid_descriptor_storage, sizeof(hid_descriptor_storage));
    scan_parameters_service_client_init();
    device_information_service_client_init();

    gap_set_scan_parameters(0 /* type: passive */, 48 /* interval */, 48 /* window */);
}

void uni_ble_scan_start(void) {
    if (!uni_ble_is_enabled())
        return;

    gap_start_scan();
    logi("BLE scan -> 1\n");
    is_scanning = true;
}

void uni_ble_scan_stop(void) {
    if (!uni_ble_is_enabled())
        return;

    gap_stop_scan();
    logi("BLE scan -> 0\n");
    is_scanning = false;
}

void uni_ble_disconnect(hci_con_handle_t conn_handle) {
    hog_disconnect(conn_handle);
}

void uni_ble_set_enabled(bool enabled) {
    // Called from different Task. Don't call btstack functions.
    uni_property_value_t val;

    val.u8 = enabled;

    uni_property_set(UNI_PROPERTY_KEY_BLE_ENABLED, UNI_PROPERTY_TYPE_U8, val);
}

bool uni_ble_is_enabled() {
    uni_property_value_t val;
    uni_property_value_t def;

#ifdef CONFIG_BLUEPAD32_ENABLE_BLE_BY_DEFAULT
    def.u8 = 1;
#else
    def.u8 = 0;
#endif  // CONFIG_BLUEPAD32_ENABLE_BLE_BY_DEFAULT
    val = uni_property_get(UNI_PROPERTY_KEY_BLE_ENABLED, UNI_PROPERTY_TYPE_U8, def);
    return val.u8;
}