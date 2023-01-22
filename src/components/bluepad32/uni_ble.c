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
 * Unijoysticle additions based on the following BlueKitchen's test/example
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
#include "uni_common.h"
#include "uni_config.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_log.h"

void uni_ble_device_information_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    /* LISTING_PAUSE */
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    uint8_t code;
    uint8_t status;
    uint8_t att_status;
    hci_con_handle_t con_handle;
    uni_hid_device_t* device;

    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) {
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
                    sm_request_pairing(con_handle);
                    break;
                default:
                    logi("Device Information service client connection failed, err 0x%02x.\n", status);
                    gap_disconnect(con_handle);
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

void uni_ble_hids_client_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    ARG_UNUSED(packet_type);
    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    uint8_t status;
    uint16_t hids_cid;
    uni_hid_device_t* device;

    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) {
        return;
    }

    switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
        case GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED:
            status = gattservice_subevent_hid_service_connected_get_status(packet);
            logi("GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED, status=0x%02x\n", status);
            switch (status) {
                case ERROR_CODE_SUCCESS:
                    logi("HID service client connected, found %d services\n",
                         gattservice_subevent_hid_service_connected_get_num_instances(packet));

                    // store device as bonded
                    // XXX: todo
                    logi("Ready - please start typing or mousing..\n");
                    break;
                default:
                    loge("HID service client connection failed, err 0x%02x.\n", status);
                    break;
            }
            break;

        case GATTSERVICE_SUBEVENT_HID_REPORT:
            logi("GATTSERVICE_SUBEVENT_HID_REPORT\n");
            hids_cid = gattservice_subevent_hid_report_get_hids_cid(packet);
            device = uni_hid_device_get_instance_for_hids_cid(hids_cid);
            if (!device) {
                loge("GATT HID REPORT: Could not find valid HID device\n");
            }

            printf_hexdump(gattservice_subevent_hid_report_get_report(packet),
                           gattservice_subevent_hid_report_get_report_len(packet));

            // uni_hid_parse_input_report(device, gattservice_subevent_hid_report_get_report(packet),
            // gattservice_subevent_hid_report_get_report_len(packet)); uni_hid_device_process_controller(device);
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

/* HCI packet handler
 *
 * text The SM packet handler receives Security Manager Events required for
 * pairing. It also receives events generated during Identity Resolving see
 * Listing SMPacketHandler.
 */
void uni_ble_sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    ARG_UNUSED(channel);
    ARG_UNUSED(size);
    bd_addr_t addr;
    uni_hid_device_t* device;
    uint8_t status;
    uint8_t type;

    if (packet_type != HCI_EVENT_PACKET)
        return;

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
            logi("SM_EVENT_IDENTITY_RESOLVING_FAILED\n");
            break;
        case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
            logi("SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED\n");
            break;
        case SM_EVENT_PAIRING_STARTED:
            logi("SM_EVENT_PAIRING_STARTED\n");
            break;
        case SM_EVENT_PAIRING_COMPLETE:
            sm_event_pairing_complete_get_address(packet, addr);
            device = uni_hid_device_get_instance_for_address(addr);
            if (!device) {
                loge("SM_EVENT_PAIRING_COMPLETE: Invalid device");
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
            break;
        default:
            loge("Unkown SM packet type: %#x\n", type);
            break;
    }
}
