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
 * Copyright (C) 2022 Ricardo Quesada
 * Unijoysticle additions based on the following BlueKitchen's test/example
 * files:
 *   - hid_host_test.c
 *   - hid_device.c
 *   - gap_inquire.c
 *   - hid_device_test.c
 */

#include "uni_bt_sdp.h"

#include <btstack.h>
#include <inttypes.h>

#include "uni_bluetooth.h"
#include "uni_common.h"
#include "uni_log.h"

#define MAX_ATTRIBUTE_VALUE_SIZE 512  // Apparently PS4 has a 470-bytes report

// Some old devices like "ThinkGeek 8-bitty Game Controller" takes a lot of time to respond
// to SDP queries.
#define SDP_QUERY_TIMEOUT_MS 13000
_Static_assert(SDP_QUERY_TIMEOUT_MS < HID_DEVICE_CONNECTION_TIMEOUT_MS, "Timeout too big");

static uint8_t sdp_attribute_value[MAX_ATTRIBUTE_VALUE_SIZE];
static const unsigned int sdp_attribute_value_buffer_size = MAX_ATTRIBUTE_VALUE_SIZE;
static uni_hid_device_t* sdp_device = NULL;
static btstack_timer_source_t sdp_query_timer;

static void handle_sdp_hid_query_result(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);
static void handle_sdp_pid_query_result(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size);
static void sdp_query_timeout(btstack_timer_source_t* ts);

// SDP Server
static uint8_t device_id_sdp_service_buffer[100];

// HID results: HID descriptor, PSM interrupt, PSM control, etc.
static void handle_sdp_hid_query_result(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    ARG_UNUSED(packet_type);
    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    des_iterator_t attribute_list_it;
    des_iterator_t additional_des_it;
    uint8_t* des_element;
    uint8_t* element;

    if (sdp_device == NULL) {
        loge("ERROR: handle_sdp_hid_query_result. SDP device = NULL\n");
        return;
    }

    switch (hci_event_packet_get_type(packet)) {
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= sdp_attribute_value_buffer_size) {
                sdp_attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] =
                    sdp_event_query_attribute_byte_get_data(packet);
                if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet) + 1) ==
                    sdp_event_query_attribute_byte_get_attribute_length(packet)) {
                    switch (sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                        case BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST:
                            for (des_iterator_init(&attribute_list_it, sdp_attribute_value);
                                 des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES)
                                    continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                for (des_iterator_init(&additional_des_it, des_element);
                                     des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {
                                    if (des_iterator_get_type(&additional_des_it) != DE_STRING)
                                        continue;
                                    element = des_iterator_get_element(&additional_des_it);
                                    const uint8_t* descriptor = de_get_string(element);
                                    int descriptor_len = de_get_data_size(element);
                                    logi("SDP HID Descriptor (%d):\n", descriptor_len);
                                    uni_hid_device_set_hid_descriptor(sdp_device, descriptor, descriptor_len);
                                    printf_hexdump(descriptor, descriptor_len);
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }
            } else {
                loge("SDP attribute value buffer size exceeded: available %d, required %d\n",
                     sdp_attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
            }
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            uni_bt_sdp_query_end(sdp_device);
            break;
        default:
            break;
    }
}

// Device ID results: Vendor ID, Product ID, Version, etc...
static void handle_sdp_pid_query_result(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    ARG_UNUSED(packet_type);
    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    uint16_t id16;

    if (sdp_device == NULL) {
        loge("ERROR: handle_sdp_pid_query_result. SDP device = NULL\n");
        return;
    }

    switch (hci_event_packet_get_type(packet)) {
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= sdp_attribute_value_buffer_size) {
                sdp_attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] =
                    sdp_event_query_attribute_byte_get_data(packet);
                if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet) + 1) ==
                    sdp_event_query_attribute_byte_get_attribute_length(packet)) {
                    switch (sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                        case BLUETOOTH_ATTRIBUTE_VENDOR_ID:
                            if (de_element_get_uint16(sdp_attribute_value, &id16))
                                uni_hid_device_set_vendor_id(sdp_device, id16);
                            else
                                loge("Error getting vendor id\n");
                            break;

                        case BLUETOOTH_ATTRIBUTE_PRODUCT_ID:
                            if (de_element_get_uint16(sdp_attribute_value, &id16))
                                uni_hid_device_set_product_id(sdp_device, id16);
                            else
                                loge("Error getting product id\n");
                            break;
                        default:
                            break;
                    }
                }
            } else {
                loge("SDP attribute value buffer size exceeded: available %d, required %d\n",
                     sdp_attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
            }
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            logi("Vendor ID: 0x%04x - Product ID: 0x%04x\n", uni_hid_device_get_vendor_id(sdp_device),
                 uni_hid_device_get_product_id(sdp_device));
            uni_hid_device_guess_controller_type_from_pid_vid(sdp_device);
            uni_bt_conn_set_state(&sdp_device->conn, UNI_BT_CONN_STATE_SDP_VENDOR_FETCHED);
            uni_bluetooth_process_fsm(sdp_device);
            break;
        default:
            // TODO: xxx
            logd("TODO: handle_sdp_pid_query_result. switch->default triggered\n");
            break;
    }
}

static void sdp_query_timeout(btstack_timer_source_t* ts) {
    loge("<------- sdp_query_timeout()\n");
    uni_hid_device_t* d = btstack_run_loop_get_timer_context(ts);
    if (!sdp_device) {
        loge("sdp_query_timeout: unexpeced, sdp_device should not be NULL\n");
        return;
    }
    if (d != sdp_device) {
        loge("sdp_query_timeout: unexpected device values, they should be equal, got: %s != %s",
             bd_addr_to_str(d->conn.btaddr), bd_addr_to_str(sdp_device->conn.btaddr));
        return;
    }

    logi("Failed to query SDP for %s, timeout\n", bd_addr_to_str(d->conn.btaddr));
    sdp_device = NULL;
}

// Public functions

void uni_bt_sdp_query_start(uni_hid_device_t* d) {
    loge("-----------> sdp_query_start()\n");
    // Needed for the SDP query since it only supports one SDP query at the time.
    if (sdp_device != NULL) {
        logi("Another SDP query is in progress (%s), disconnecting...\n", bd_addr_to_str(sdp_device->conn.btaddr));
        uni_hid_device_disconnect(d);
        uni_hid_device_delete(d);
        /* 'd'' is destroyed after this call, don't use it */
        return;
    }

    sdp_device = d;
    btstack_run_loop_set_timer_context(&sdp_query_timer, d);
    btstack_run_loop_set_timer_handler(&sdp_query_timer, &sdp_query_timeout);
    btstack_run_loop_set_timer(&sdp_query_timer, SDP_QUERY_TIMEOUT_MS);
    btstack_run_loop_add_timer(&sdp_query_timer);

    uni_bt_sdp_query_start_vid_pid(d);
}

void uni_bt_sdp_query_end(uni_hid_device_t* d) {
    loge("<----------- sdp_query_end()\n");
    uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_SDP_HID_DESCRIPTOR_FETCHED);
    sdp_device = NULL;
    btstack_run_loop_remove_timer(&sdp_query_timer);
    uni_bluetooth_process_fsm(d);
}

void uni_bt_sdp_query_start_vid_pid(uni_hid_device_t* d) {
    logi("Starting SDP VID/PID query for %s\n", bd_addr_to_str(d->conn.btaddr));

    uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_SDP_VENDOR_REQUESTED);
    uint8_t status =
        sdp_client_query_uuid16(&handle_sdp_pid_query_result, d->conn.btaddr, BLUETOOTH_SERVICE_CLASS_PNP_INFORMATION);
    if (status != 0) {
        loge("Failed to perform SDP VID/PID query\n");
        uni_hid_device_disconnect(d);
        uni_hid_device_delete(d);
        /* 'd' is destroyed after this call, don't use it */
        return;
    }
}

void uni_bt_sdp_query_start_hid_descriptor(uni_hid_device_t* d) {
    if (!uni_hid_device_does_require_hid_descriptor(d)) {
        logi("Device %s does not need a HID descriptor, skipping query.\n", bd_addr_to_str(d->conn.btaddr));
        uni_bt_sdp_query_end(d);
        return;
    }

    logi("Starting SDP HID-descriptor query for %s\n", bd_addr_to_str(d->conn.btaddr));

    // Needed for the SDP query since it only supports one SDP query at the time.
    if (sdp_device == NULL) {
        logi("...but sdp_vendor was not set, aborting query for %s\n", bd_addr_to_str(d->conn.btaddr));
        return;
    }

    uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_SDP_HID_DESCRIPTOR_REQUESTED);
    uint8_t status = sdp_client_query_uuid16(&handle_sdp_hid_query_result, d->conn.btaddr,
                                             BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
    if (status != 0) {
        loge("Failed to perform SDP query for %s. Removing it...\n", bd_addr_to_str(d->conn.btaddr));
        btstack_run_loop_remove_timer(&sdp_query_timer);
        uni_hid_device_disconnect(d);
        uni_hid_device_delete(d);
        /* 'd'' is destroyed after this call, don't use it */
    }
}

void uni_bt_sdp_server_init() {
    // Only initialize the SDP record. Just needed for DualShock/DualSense to have
    // a successful reconnect.
    sdp_init();

    device_id_create_sdp_record(device_id_sdp_service_buffer, 0x10003, DEVICE_ID_VENDOR_ID_SOURCE_BLUETOOTH,
                                BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, 1, 1);
    logi("Device ID SDP service record size: %u\n", de_get_len((uint8_t*)device_id_sdp_service_buffer));
    sdp_register_service(device_id_sdp_service_buffer);
}
