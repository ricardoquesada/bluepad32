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

/**
 * @file uni_bt_sdp.c
 * @brief Bluetooth Classic (BR/EDR) Service Discovery Protocol (SDP) client and server integration.
 *
 * Manages sequential SDP client queries for newly connected BR/EDR HID devices:
 *  1. Queries PnP Information (`BLUETOOTH_SERVICE_CLASS_PNP_INFORMATION`, `0x1200`) to resolve
 *     Vendor ID (`0x0201`) and Product ID (`0x0202`) and infer the controller type.
 *  2. If the controller requires a HID report descriptor, queries the Human Interface Device
 *     service (`BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE`, `0x1124`) to stream and
 *     parse `BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST` (`0x0206`).
 *
 * Also registers a minimal Device ID SDP server record required by DualShock 4 and DualSense
 * controllers during reconnection.
 */

#include "bt/uni_bt_sdp.h"

#include <btstack.h>
#include <inttypes.h>

#include "sdkconfig.h"

#include "bt/uni_bt.h"
#include "bt/uni_bt_bredr.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_log.h"

#if !UNI_ENABLE_BREDR
#error "BR/EDR is not enabled on this platform"
#endif

#define MAX_ATTRIBUTE_VALUE_SIZE 512  // Apparently PS4 has a 470-bytes report

// Some old devices like "ThinkGeek 8-bitty Game Controller" takes a lot of time to respond
// to SDP queries.
#define SDP_QUERY_TIMEOUT_MS 13000
_Static_assert(SDP_QUERY_TIMEOUT_MS < HID_DEVICE_CONNECTION_TIMEOUT_MS, "Timeout too big");

static uint8_t sdp_attribute_value[MAX_ATTRIBUTE_VALUE_SIZE];
static const unsigned int sdp_attribute_value_buffer_size = MAX_ATTRIBUTE_VALUE_SIZE;
static uni_hid_device_t* sdp_device = NULL;
static btstack_timer_source_t sdp_query_timer;

static void sdp_query_timeout(btstack_timer_source_t* ts);

// SDP Server
static uint8_t device_id_sdp_service_buffer[100];

/**
 * @brief Accumulate a single streamed byte from an `SDP_EVENT_QUERY_ATTRIBUTE_VALUE` packet.
 *
 * BTstack's `sdp_parser_process_byte()` streams each attribute value in two phases:
 *  1. `GET_ATTRIBUTE_VALUE_LENGTH`: While reading the 1-to-5 byte Data Element (DE) header
 *     (`data_offset` in `0 .. header_size - 1`), `sdp_parser_emit_value_byte()` is invoked
 *     before `de_state_size()` computes `sdp_parser_attribute_value_size`. Consequently,
 *     `attr_len` is `0` for all DE header bytes.
 *  2. `GET_ATTRIBUTE_VALUE`: Once the DE header is complete, `attr_len` reports the full
 *     attribute length (`header_size + de_size > 0`) for all payload bytes.
 *
 * @param packet       Pointer to the `SDP_EVENT_QUERY_ATTRIBUTE_VALUE` HCI event packet.
 * @param out_attr_len Output pointer receiving `attr_len` when the final byte is stored.
 * @return `true` when the final byte of the attribute (`data_offset + 1 == attr_len`) has
 *         been stored in `sdp_attribute_value` and is ready to parse; `false` otherwise.
 */
static bool sdp_accumulate_attribute_byte(const uint8_t* packet, uint16_t* out_attr_len) {
    uint16_t attr_len = sdp_event_query_attribute_byte_get_attribute_length(packet);
    uint16_t data_offset = sdp_event_query_attribute_byte_get_data_offset(packet);

    if (attr_len > sdp_attribute_value_buffer_size) {
        loge("SDP attribute value buffer size exceeded: available %u, required %u\n", sdp_attribute_value_buffer_size,
             attr_len);
        return false;
    }

    if (data_offset >= sdp_attribute_value_buffer_size || (attr_len > 0 && data_offset >= attr_len)) {
        loge("SDP attribute value data offset out of bounds: offset %u, attr_len %u\n", data_offset, attr_len);
        return false;
    }

    sdp_attribute_value[data_offset] = sdp_event_query_attribute_byte_get_data(packet);
    *out_attr_len = attr_len;
    return attr_len > 0 && (uint16_t)(data_offset + 1) == attr_len;
}

/**
 * @brief Initialize a Data Element Sequence (DES) iterator with strict bounds validation.
 *
 * Provides a version-agnostic replacement for `des_iterator_init_with_len()` (which was
 * added in BTstack 1.8.2 and is absent from the BTstack 1.6.2 release bundled in `pico-sdk`).
 * In BTstack 1.6.2, `des_iterator_init()` sets `it->length = de_get_len(element)` without
 * checking buffer bounds. This helper first verifies that `element` is non-empty and its
 * total encoded byte length matches `element_size` via `de_get_len_safe()` (available since
 * BTstack 1.1) before delegating to `des_iterator_init()`.
 *
 * @param it           Pointer to the `des_iterator_t` to initialize.
 * @param element      Pointer to the candidate `DE_DES` buffer.
 * @param element_size Total available byte length of `element` (guaranteed `<= 512`, fitting
 *                     within both BTstack 1.6.2's `uint16_t` and 1.8.2+'s `uint32_t` fields).
 * @return `true` if `element` is a valid `DE_DES` whose encoded length equals `element_size`;
 *         `false` if `element` is `NULL`, empty, truncated, has trailing bytes, or is not `DE_DES`.
 */
static bool sdp_des_iterator_init_safe(des_iterator_t* it, uint8_t* element, uint32_t element_size) {
    // Reject NULL or zero-length buffers before calling de_get_len_safe(): de_get_len_safe(buf, 0)
    // returns 0, which would otherwise satisfy `0 == element_size` and fall through to
    // des_iterator_init(), dereferencing element[0] out of bounds.
    if (element == NULL || element_size == 0) {
        return false;
    }
    // Require exact length match: rejects both truncated elements (de_get_len_safe() == 0)
    // and elements followed by unparsed trailing bytes (de_get_len_safe() < element_size).
    if (de_get_len_safe(element, element_size) != element_size) {
        return false;
    }
    return des_iterator_init(it, element);
}

/**
 * @brief Return the bounds-checked byte length of the current element in a DES iterator.
 *
 * Provides a version-agnostic replacement for `des_iterator_get_element_len()` (added in
 * BTstack 1.8.2 and absent from Pico SDK's BTstack 1.6.2). In BTstack 1.6.2,
 * `des_iterator_has_more()` only checks `it->pos < it->length`, and `des_iterator_get_type()`,
 * `des_iterator_get_element()`, and `des_iterator_next()` index `&it->element[it->pos]`
 * without verifying that the child element fits within the remaining `it->length - it->pos`
 * bytes.
 *
 * @note Accesses `it->pos`, `it->length`, and `&it->element[it->pos]` directly rather than
 *       calling BTstack's `des_iterator_*` getters because those C APIs take a non-`const`
 *       `des_iterator_t*` and would violate `-Werror=discarded-qualifiers`.
 *
 * @param it Pointer to the active `des_iterator_t`.
 * @return Validated byte length of the current child element, or `0` if the iterator is
 *         exhausted or the current child element header/payload exceeds `it->length - it->pos`.
 */
static uint32_t sdp_des_iterator_get_element_len_safe(const des_iterator_t* it) {
    if (it->pos >= it->length) {
        return 0;
    }
    return de_get_len_safe(&it->element[it->pos], it->length - it->pos);
}

/**
 * @brief Parse a completed `BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST` (`0x0206`) Data Element Sequence.
 *
 * Expects a nested `DE_DES -> DE_DES -> (DE_UINT8, DE_STRING)` structure where the inner
 * `DE_STRING` holds the raw HID report descriptor. Validates both outer and inner sequences
 * with `sdp_des_iterator_init_safe()` and checks each child element's bounds at the start of
 * every loop iteration via `sdp_des_iterator_get_element_len_safe()` for compatibility with
 * both Pico SDK's BTstack 1.6.2 and BTstack 1.8.2+.
 *
 * @param attr_len Total encoded byte length of the attribute in `sdp_attribute_value`.
 */
static void parse_sdp_hid_descriptor_list(uint16_t attr_len) {
    des_iterator_t attribute_list_it;
    if (!sdp_des_iterator_init_safe(&attribute_list_it, sdp_attribute_value, attr_len)) {
        loge("Invalid SDP HID descriptor list DES (attr_len=%u)\n", attr_len);
        return;
    }

    for (; des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {
        uint32_t des_element_len = sdp_des_iterator_get_element_len_safe(&attribute_list_it);
        if (des_element_len == 0) {
            // Must break (never continue): in BTstack 1.6.2, continue would run the loop
            // increment des_iterator_next(), performing an unbounded de_get_len() read on
            // the truncated child element at attribute_list_it.pos.
            break;
        }
        if (des_iterator_get_type(&attribute_list_it) != DE_DES) {
            continue;
        }
        uint8_t* des_element = des_iterator_get_element(&attribute_list_it);

        des_iterator_t additional_des_it;
        if (!sdp_des_iterator_init_safe(&additional_des_it, des_element, des_element_len)) {
            continue;
        }

        for (; des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {
            if (sdp_des_iterator_get_element_len_safe(&additional_des_it) == 0) {
                // Must break (never continue) to prevent BTstack 1.6.2's des_iterator_next()
                // from reading past the end of the inner DE_DES on a truncated child element.
                break;
            }
            if (des_iterator_get_type(&additional_des_it) != DE_STRING) {
                continue;
            }
            uint8_t* element = des_iterator_get_element(&additional_des_it);
            const uint8_t* descriptor = de_get_string(element);
            if (descriptor == NULL) {
                continue;
            }
            int descriptor_len = (int)de_get_data_size(element);
            logi("SDP HID Descriptor (%d):\n", descriptor_len);
            uni_hid_device_set_hid_descriptor(sdp_device, descriptor, descriptor_len);
            printf_hexdump(descriptor, descriptor_len);
        }
    }
}

/**
 * @brief Parse a completed PnP Information attribute (`BLUETOOTH_ATTRIBUTE_VENDOR_ID` or `PRODUCT_ID`).
 *
 * @param attr_id  SDP attribute identifier (`0x0201` or `0x0202`).
 * @param attr_len Total encoded byte length of the attribute in `sdp_attribute_value`.
 */
static void parse_sdp_pnp_attribute(uint16_t attr_id, uint16_t attr_len) {
    uint16_t id16 = 0;
    bool valid_uint16 = (de_get_len_safe(sdp_attribute_value, attr_len) == attr_len) &&
                        de_element_get_uint16(sdp_attribute_value, &id16);

    switch (attr_id) {
        case BLUETOOTH_ATTRIBUTE_VENDOR_ID:
            if (valid_uint16) {
                uni_hid_device_set_vendor_id(sdp_device, id16);
            } else {
                loge("Error getting vendor id\n");
            }
            break;

        case BLUETOOTH_ATTRIBUTE_PRODUCT_ID:
            if (valid_uint16) {
                uni_hid_device_set_product_id(sdp_device, id16);
            } else {
                loge("Error getting product id\n");
            }
            break;

        default:
            break;
    }
}

// HID results: HID descriptor, PSM interrupt, PSM control, etc.
// Exposed with non-static linkage so Layer 2 packet-handler unit tests can feed
// synthetic SDP_EVENT_QUERY_ATTRIBUTE_VALUE / SDP_EVENT_QUERY_COMPLETE byte streams directly.
void uni_handle_sdp_hid_query_result(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    ARG_UNUSED(packet_type);
    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    if (sdp_device == NULL) {
        loge("ERROR: uni_handle_sdp_hid_query_result. SDP device = NULL\n");
        return;
    }

    switch (hci_event_packet_get_type(packet)) {
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE: {
            uint16_t attr_len = 0;
            if (!sdp_accumulate_attribute_byte(packet, &attr_len)) {
                break;
            }
            if (sdp_event_query_attribute_byte_get_attribute_id(packet) == BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST) {
                parse_sdp_hid_descriptor_list(attr_len);
            }
            break;
        }
        case SDP_EVENT_QUERY_COMPLETE:
            uni_bt_sdp_query_end(sdp_device);
            break;
        default:
            break;
    }
}

// Device ID results: Vendor ID, Product ID, Version, etc.
// Exposed with non-static linkage so Layer 2 packet-handler unit tests can feed
// synthetic SDP_EVENT_QUERY_ATTRIBUTE_VALUE / SDP_EVENT_QUERY_COMPLETE byte streams directly.
void uni_handle_sdp_pid_query_result(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    ARG_UNUSED(packet_type);
    ARG_UNUSED(channel);
    ARG_UNUSED(size);

    if (sdp_device == NULL) {
        loge("ERROR: uni_handle_sdp_pid_query_result. SDP device = NULL\n");
        return;
    }

    switch (hci_event_packet_get_type(packet)) {
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE: {
            uint16_t attr_len = 0;
            if (!sdp_accumulate_attribute_byte(packet, &attr_len)) {
                break;
            }
            parse_sdp_pnp_attribute(sdp_event_query_attribute_byte_get_attribute_id(packet), attr_len);
            break;
        }
        case SDP_EVENT_QUERY_COMPLETE:
            logi("Vendor ID: 0x%04x - Product ID: 0x%04x\n", uni_hid_device_get_vendor_id(sdp_device),
                 uni_hid_device_get_product_id(sdp_device));
            uni_hid_device_guess_controller_type_from_pid_vid(sdp_device);
            uni_bt_conn_set_state(&sdp_device->conn, UNI_BT_CONN_STATE_SDP_VENDOR_FETCHED);
            uni_bt_bredr_process_fsm(sdp_device);
            break;
        default:
            logd("TODO: uni_handle_sdp_pid_query_result. switch->default triggered\n");
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

// Test seam: binds the module-scoped `sdp_device` pointer directly so unit tests can invoke
// `uni_handle_sdp_pid_query_result` and `uni_handle_sdp_hid_query_result` without opening
// a live L2CAP SDP connection.
void uni_bt_sdp_set_device_for_test(uni_hid_device_t* d) {
    sdp_device = d;
}

void uni_bt_sdp_query_start(uni_hid_device_t* d) {
    logi("-----------> sdp_query_start()\n");
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
    logi("<----------- sdp_query_end()\n");
    uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_SDP_HID_DESCRIPTOR_FETCHED);
    sdp_device = NULL;
    btstack_run_loop_remove_timer(&sdp_query_timer);
    uni_bt_bredr_process_fsm(d);
}

void uni_bt_sdp_query_abort(uni_hid_device_t* d) {
    // Only disarm sdp_query_timer and clear sdp_device if the device being deleted/aborted
    // is the one currently running an SDP query. When a concurrent 2nd device is rejected in
    // uni_bt_sdp_query_start(), uni_hid_device_delete(d2) calls this function; checking
    // sdp_device == d prevents d2's teardown from cancelling d1's active SDP query.
    if (d != NULL && sdp_device == d) {
        btstack_run_loop_remove_timer(&sdp_query_timer);
        sdp_device = NULL;
    }
}

void uni_bt_sdp_query_start_vid_pid(uni_hid_device_t* d) {
    logi("Starting SDP VID/PID query for %s\n", bd_addr_to_str(d->conn.btaddr));

    uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_SDP_VENDOR_REQUESTED);
    uint8_t status = sdp_client_query_uuid16(&uni_handle_sdp_pid_query_result, d->conn.btaddr,
                                             BLUETOOTH_SERVICE_CLASS_PNP_INFORMATION);
    if (status != 0) {
        loge("Failed to perform SDP VID/PID query\n");
        uni_bt_sdp_query_abort(d);
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
    uint8_t status = sdp_client_query_uuid16(&uni_handle_sdp_hid_query_result, d->conn.btaddr,
                                             BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
    if (status != 0) {
        loge("Failed to perform SDP query for %s. Removing it...\n", bd_addr_to_str(d->conn.btaddr));
        uni_bt_sdp_query_abort(d);
        uni_hid_device_disconnect(d);
        uni_hid_device_delete(d);
        /* 'd'' is destroyed after this call, don't use it */
    }
}

void uni_bt_sdp_server_init(void) {
    // Only initialize the SDP record. Just needed for DualShock/DualSense to have
    // a successful reconnecting.
    sdp_init();

    device_id_create_sdp_record(device_id_sdp_service_buffer, 0x10003, DEVICE_ID_VENDOR_ID_SOURCE_BLUETOOTH,
                                BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, 1, 1);
    logi("Device ID SDP service record size: %u\n", de_get_len((uint8_t*)device_id_sdp_service_buffer));
    sdp_register_service(device_id_sdp_service_buffer);
}
