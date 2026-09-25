// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Ricardo Quesada
// http://retro.moe/unijoysticle2
//
// Unit and regression test suite for Bluepad32 core infrastructure:
// - `uni_circular_buffer` capacity, wrap-around, and null/length guards.
// - `uni_bt_allowlist` CRUD, POSIX TLV string persistence, and `uni_property` guards.
// - `uni_hid_device` pool lifecycle, virtual child linking, and BTstack timer teardown.

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ble/le_device_db.h>
#include <ble/le_device_db_tlv.h>
#include <btstack.h>
#include <btstack_memory.h>
#include <btstack_run_loop.h>
#include <btstack_run_loop_posix.h>
#include <btstack_tlv.h>
#include <btstack_tlv_posix.h>
#include <classic/btstack_link_key_db_tlv.h>
#include <hci.h>
#include <hci_transport.h>
#include <l2cap.h>

#include "bt/uni_bt.h"
#include "bt/uni_bt_allowlist.h"
#include "bt/uni_bt_bredr.h"
#include "bt/uni_bt_defines.h"
#include "platform/uni_platform.h"
#include "sdkconfig.h"
#include "uni_circular_buffer.h"
#include "uni_hid_device.h"
#include "uni_property.h"
#include "uni_virtual_device.h"

static void dummy_transport_register_packet_handler(void (*handler)(uint8_t packet_type,
                                                                    uint8_t* packet,
                                                                    uint16_t size)) {
    (void)handler;
}

static const hci_transport_t dummy_transport = {
    .name = "dummy",
    .register_packet_handler = dummy_transport_register_packet_handler,
};

// Minimal Platform Implementation
static uni_error_t my_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    (void)addr;
    (void)name;
    (void)cod;
    (void)rssi;
    return UNI_ERROR_SUCCESS;
}

static void my_on_device_connected(uni_hid_device_t* d) {
    (void)d;
}

static void my_on_device_disconnected(uni_hid_device_t* d) {
    (void)d;
}

static uni_error_t my_on_device_ready(uni_hid_device_t* d) {
    (void)d;
    return UNI_ERROR_SUCCESS;
}

static void my_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    (void)d;
    (void)ctl;
}

static const uni_property_t* my_get_property(uni_property_idx_t idx) {
    (void)idx;
    return NULL;
}

static void my_on_oob_event(uni_platform_oob_event_t event, void* data) {
    (void)event;
    (void)data;
}

static struct uni_platform my_platform = {
    .name = "Test Platform",
    .init = NULL,
    .on_init_complete = NULL,
    .on_device_discovered = my_on_device_discovered,
    .on_device_connected = my_on_device_connected,
    .on_device_disconnected = my_on_device_disconnected,
    .on_device_ready = my_on_device_ready,
    .on_controller_data = my_on_controller_data,
    .get_property = my_get_property,
    .on_oob_event = my_on_oob_event,
};

struct uni_platform* uni_get_platform(void) {
    return &my_platform;
}

// ============================================================================
// uni_circular_buffer Tests
// ============================================================================

static void test_circular_buffer_initial_and_reset_state(void) {
    printf("Testing uni_circular_buffer initial empty and reset state...\n");
    uni_circular_buffer_t buf = {0};
    int16_t cid = 0;
    void* data = NULL;
    int len = -1;

    assert(uni_circular_buffer_is_empty(&buf) == 1);
    assert(uni_circular_buffer_is_full(&buf) == 0);
    assert(uni_circular_buffer_get(&buf, &cid, &data, &len) == UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY);

    // Null buffer safety checks
    assert(uni_circular_buffer_is_empty(NULL) == 1);
    assert(uni_circular_buffer_is_full(NULL) == 0);
    assert(uni_circular_buffer_get(NULL, &cid, &data, &len) == UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY);
    uni_circular_buffer_reset(NULL);

    uint8_t sample[4] = {1, 2, 3, 4};
    assert(uni_circular_buffer_put(&buf, 0x10, sample, sizeof(sample)) == UNI_CIRCULAR_BUFFER_ERROR_OK);
    assert(uni_circular_buffer_is_empty(&buf) == 0);

    uni_circular_buffer_reset(&buf);
    assert(uni_circular_buffer_is_empty(&buf) == 1);
    assert(uni_circular_buffer_is_full(&buf) == 0);
    printf("PASS\n");
}

static void test_circular_buffer_null_and_negative_length_guards(void) {
    printf("Testing uni_circular_buffer null and negative length guards...\n");
    uni_circular_buffer_t buf = {0};
    uint8_t payload[4] = {0xAA, 0xBB, 0xCC, 0xDD};

    assert(uni_circular_buffer_put(NULL, 1, payload, 4) == UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG);
    assert(uni_circular_buffer_put(&buf, 1, NULL, 4) == UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG);
    assert(uni_circular_buffer_put(&buf, 1, payload, -1) == UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG);
    assert(uni_circular_buffer_put(&buf, 1, payload, -128) == UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG);
    assert(uni_circular_buffer_is_empty(&buf) == 1);
    printf("PASS\n");
}

static void test_circular_buffer_zero_length_packet(void) {
    printf("Testing uni_circular_buffer zero-length packet put/get...\n");
    uni_circular_buffer_t buf = {0};
    int16_t cid = 0;
    void* data = NULL;
    int len = -1;

    assert(uni_circular_buffer_put(&buf, 0x42, NULL, 0) == UNI_CIRCULAR_BUFFER_ERROR_OK);
    assert(uni_circular_buffer_is_empty(&buf) == 0);
    assert(uni_circular_buffer_get(&buf, &cid, &data, &len) == UNI_CIRCULAR_BUFFER_ERROR_OK);
    assert(cid == 0x42);
    assert(len == 0);
    assert(uni_circular_buffer_is_empty(&buf) == 1);
    printf("PASS\n");
}

static void test_circular_buffer_exact_capacity_boundary(void) {
    printf("Testing uni_circular_buffer exact 128-byte capacity boundary...\n");
    uni_circular_buffer_t buf = {0};
    uint8_t pkt_128[UNI_CIRCULAR_BUFFER_DATA_SIZE];
    uint8_t pkt_129[UNI_CIRCULAR_BUFFER_DATA_SIZE + 1];
    int16_t cid = 0;
    void* data = NULL;
    int len = 0;

    for (size_t i = 0; i < sizeof(pkt_128); i++) {
        pkt_128[i] = (uint8_t)(i ^ 0x5A);
    }
    memset(pkt_129, 0xFF, sizeof(pkt_129));

    // 129 bytes must be rejected
    assert(uni_circular_buffer_put(&buf, 0x77, pkt_129, (int)sizeof(pkt_129)) ==
           UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG);
    assert(uni_circular_buffer_is_empty(&buf) == 1);

    // Exact 128 bytes must succeed and round-trip identically
    assert(uni_circular_buffer_put(&buf, 0x77, pkt_128, (int)sizeof(pkt_128)) == UNI_CIRCULAR_BUFFER_ERROR_OK);
    assert(uni_circular_buffer_get(&buf, &cid, &data, &len) == UNI_CIRCULAR_BUFFER_ERROR_OK);
    assert(cid == 0x77);
    assert(len == UNI_CIRCULAR_BUFFER_DATA_SIZE);
    assert(memcmp(data, pkt_128, sizeof(pkt_128)) == 0);
    printf("PASS\n");
}

static void test_circular_buffer_full_queue_and_wrap_around(void) {
    printf("Testing uni_circular_buffer full queue saturation and wrap-around...\n");
    uni_circular_buffer_t buf = {0};
    int16_t cid = 0;
    void* data = NULL;
    int len = 0;

    // Enqueue UNI_CIRCULAR_BUFFER_SIZE - 1 (31) packets
    for (int i = 0; i < UNI_CIRCULAR_BUFFER_SIZE - 1; i++) {
        uint8_t val = (uint8_t)i;
        assert(uni_circular_buffer_put(&buf, (int16_t)(100 + i), &val, 1) == UNI_CIRCULAR_BUFFER_ERROR_OK);
    }
    assert(uni_circular_buffer_is_full(&buf) == 1);

    // 32nd put must return BUFFER_FULL
    uint8_t extra = 0xFF;
    assert(uni_circular_buffer_put(&buf, 999, &extra, 1) == UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL);

    // Dequeue 16 packets
    for (int i = 0; i < 16; i++) {
        assert(uni_circular_buffer_get(&buf, &cid, &data, &len) == UNI_CIRCULAR_BUFFER_ERROR_OK);
        assert(cid == (int16_t)(100 + i));
        assert(len == 1);
        assert(*(uint8_t*)data == (uint8_t)i);
    }
    assert(uni_circular_buffer_is_full(&buf) == 0);

    // Enqueue 16 more packets across the 31 -> 0 ring wrap boundary
    for (int i = 0; i < 16; i++) {
        uint8_t val = (uint8_t)(200 + i);
        assert(uni_circular_buffer_put(&buf, (int16_t)(300 + i), &val, 1) == UNI_CIRCULAR_BUFFER_ERROR_OK);
    }
    assert(uni_circular_buffer_is_full(&buf) == 1);

    // Dequeue remaining 15 from first batch + 16 from second batch
    for (int i = 16; i < UNI_CIRCULAR_BUFFER_SIZE - 1; i++) {
        assert(uni_circular_buffer_get(&buf, &cid, &data, &len) == UNI_CIRCULAR_BUFFER_ERROR_OK);
        assert(cid == (int16_t)(100 + i));
        assert(*(uint8_t*)data == (uint8_t)i);
    }
    for (int i = 0; i < 16; i++) {
        assert(uni_circular_buffer_get(&buf, &cid, &data, &len) == UNI_CIRCULAR_BUFFER_ERROR_OK);
        assert(cid == (int16_t)(300 + i));
        assert(*(uint8_t*)data == (uint8_t)(200 + i));
    }
    assert(uni_circular_buffer_is_empty(&buf) == 1);
    printf("PASS\n");
}

// ============================================================================
// uni_bt_allowlist, uni_bt, & uni_property_posix Tests
// ============================================================================

static void test_allowlist_zero_addr_rejection(void) {
    printf("Testing uni_bt_allowlist zero_addr rejection on add and remove...\n");
    bd_addr_t zero = {0, 0, 0, 0, 0, 0};
    const bd_addr_t* addrs = NULL;
    int total = 0;

    uni_bt_allowlist_remove_all();
    assert(uni_bt_allowlist_add_addr(zero) == false);
    assert(uni_bt_allowlist_remove_addr(zero) == false);

    uni_bt_allowlist_get_all(&addrs, &total);
    for (int i = 0; i < total; i++) {
        assert(bd_addr_cmp(addrs[i], zero) == 0);
    }
    printf("PASS\n");
}

static void test_allowlist_crud_duplicate_and_capacity(void) {
    printf("Testing uni_bt_allowlist CRUD, duplicate prevention, and capacity exhaustion...\n");
    bd_addr_t a1 = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x01};
    bd_addr_t a2 = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x02};
    bd_addr_t a3 = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x03};
    bd_addr_t a4 = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x04};
    bd_addr_t a5 = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x05};

    uni_bt_allowlist_remove_all();

    assert(uni_bt_allowlist_add_addr(a1) == true);
    assert(uni_bt_allowlist_add_addr(a2) == true);
    assert(uni_bt_allowlist_add_addr(a3) == true);
    assert(uni_bt_allowlist_add_addr(a4) == true);

    // Duplicate must be rejected
    assert(uni_bt_allowlist_add_addr(a1) == false);
    // 5th address exceeds CONFIG_BLUEPAD32_MAX_ALLOWLIST (4)
    assert(uni_bt_allowlist_add_addr(a5) == false);

    // Removing a2 frees a slot for a5
    assert(uni_bt_allowlist_remove_addr(a2) == true);
    assert(uni_bt_allowlist_remove_addr(a2) == false);
    assert(uni_bt_allowlist_add_addr(a5) == true);

    uni_bt_allowlist_remove_all();
    printf("PASS\n");
}

static void test_allowlist_enforcement_toggle(void) {
    printf("Testing uni_bt_allowlist enforcement toggle (is_allowed_addr)...\n");
    bd_addr_t a1 = {0xBB, 0x01, 0x02, 0x03, 0x04, 0x01};
    bd_addr_t unknown = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA};

    uni_bt_allowlist_remove_all();
    assert(uni_bt_allowlist_add_addr(a1) == true);

    uni_bt_allowlist_set_enabled(false);
    assert(uni_bt_allowlist_is_enabled() == false);
    assert(uni_bt_allowlist_is_allowed_addr(a1) == true);
    assert(uni_bt_allowlist_is_allowed_addr(unknown) == true);

    uni_bt_allowlist_set_enabled(true);
    assert(uni_bt_allowlist_is_enabled() == true);
    assert(uni_bt_allowlist_is_allowed_addr(a1) == true);
    assert(uni_bt_allowlist_is_allowed_addr(unknown) == false);

    uni_bt_allowlist_set_enabled(false);
    uni_bt_allowlist_remove_all();
    printf("PASS\n");
}

static void test_allowlist_tlv_string_persistence_roundtrip(void) {
    printf("Testing uni_bt_allowlist POSIX TLV string property and boot round-trip...\n");
    bd_addr_t a1 = {0x12, 0x34, 0x56, 0x78, 0x9A, 0x01};
    bd_addr_t a2 = {0x12, 0x34, 0x56, 0x78, 0x9A, 0x02};
    bd_addr_t a3 = {0x12, 0x34, 0x56, 0x78, 0x9A, 0x03};
    bd_addr_t a4 = {0x12, 0x34, 0x56, 0x78, 0x9A, 0x04};

    uni_bt_allowlist_remove_all();
    assert(uni_bt_allowlist_add_addr(a1) == true);
    assert(uni_bt_allowlist_add_addr(a2) == true);
    assert(uni_bt_allowlist_add_addr(a3) == true);
    uni_bt_allowlist_set_enabled(true);

    // Re-initialize from POSIX TLV storage: verifies update_allowlist_from_property()
    // does NOT overwrite TLV on iteration 0 and preserves all 3 addresses.
    uni_bt_allowlist_init();

    assert(uni_bt_allowlist_is_enabled() == true);
    assert(uni_bt_allowlist_is_allowed_addr(a1) == true);
    assert(uni_bt_allowlist_is_allowed_addr(a2) == true);
    assert(uni_bt_allowlist_is_allowed_addr(a3) == true);
    assert(uni_bt_allowlist_is_allowed_addr(a4) == false);

    // Clean up TLV state and verify empty round-trip
    uni_bt_allowlist_remove_all();
    uni_bt_allowlist_set_enabled(false);
    uni_bt_allowlist_init();
    assert(uni_bt_allowlist_is_enabled() == false);
    uni_bt_allowlist_set_enabled(true);
    assert(uni_bt_allowlist_is_allowed_addr(a1) == false);
    uni_bt_allowlist_set_enabled(false);
    printf("PASS\n");
}

static void test_gap_security_level_u8_union(void) {
    printf("Testing GAP security level u8 vs u32 union regression...\n");
    uni_bt_set_gap_security_level(0);
    assert(uni_bt_get_gap_security_level() == 0);

    uni_bt_set_gap_security_level(2);
    int level = uni_bt_get_gap_security_level();
    assert(level == 2);
    printf("PASS\n");
}

static void test_property_types_and_guards(void) {
    printf("Testing uni_property string/float guards and read-only enforcement...\n");
    // Read-only property must ignore writes
    uni_property_value_t ro_attempt = {.str = "hacked-version"};
    uni_property_set(UNI_PROPERTY_IDX_VERSION, ro_attempt);
    uni_property_value_t ver = uni_property_get(UNI_PROPERTY_IDX_VERSION);
    assert(ver.str != NULL);
    assert(strcmp(ver.str, "hacked-version") != 0);

    // Null string and overly long string (> 128 bytes) must be safely rejected
    uni_property_value_t valid_str = {.str = "01:02:03:04:05:06,"};
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, valid_str);
    uni_property_value_t null_str = {.str = NULL};
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, null_str);
    uni_property_value_t read_back = uni_property_get(UNI_PROPERTY_IDX_ALLOWLIST_LIST);
    assert(read_back.str != NULL);
    assert(strcmp(read_back.str, "01:02:03:04:05:06,") == 0);

    // Reset allowlist string
    uni_property_value_t empty_str = {.str = ""};
    uni_property_set(UNI_PROPERTY_IDX_ALLOWLIST_LIST, empty_str);
    printf("PASS\n");
}

static void test_property_null_platform_guard(void) {
    printf("Testing uni_property null platform and out-of-range index safety...\n");
    my_platform.get_property = NULL;
    uni_property_value_t val = uni_property_get((uni_property_idx_t)(UNI_PROPERTY_IDX_LAST + 5));
    assert(val.u32 == 0);
    my_platform.get_property = my_get_property;
    printf("PASS\n");
}

// ============================================================================
// uni_hid_device Lifecycle, Virtual Devices, Timers, & CoD Tests
// ============================================================================

static void test_create_device(void) {
    printf("Testing uni_hid_device_create...\n");
    bd_addr_t addr = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uni_hid_device_t* d = uni_hid_device_create(addr);
    assert(d != NULL);
    assert(memcmp(d->conn.btaddr, addr, 6) == 0);

    uni_hid_device_t* d2 = uni_hid_device_get_instance_for_address(addr);
    assert(d == d2);

    uni_hid_device_delete(d);

    uni_hid_device_t* d3 = uni_hid_device_get_instance_for_address(addr);
    assert(d3 == NULL);
    printf("PASS\n");
}

static void test_device_properties(void) {
    printf("Testing device properties...\n");
    bd_addr_t addr = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uni_hid_device_t* d = uni_hid_device_create(addr);

    uni_hid_device_set_cod(d, 0x123456);
    assert(d->cod == 0x123456);

    uni_hid_device_set_vendor_id(d, 0xAAAA);
    assert(uni_hid_device_get_vendor_id(d) == 0xAAAA);

    uni_hid_device_set_product_id(d, 0xBBBB);
    assert(uni_hid_device_get_product_id(d) == 0xBBBB);

    uni_hid_device_set_name(d, "Test Device");
    assert(strcmp(d->name, "Test Device") == 0);
    assert(uni_hid_device_has_name(d));

    uni_hid_device_delete(d);
    printf("PASS\n");
}

// Verifies that uni_bt_bredr_setup() overrides BTstack v1.8.2+'s default SSP auto-accept (0 -> 1, D1)
// and minimum encryption key size (16 -> 7 bytes, D2), and configures GAP Security Level 2.
static void test_bredr_setup_ssp_and_encryption_key_size(void) {
    printf("Testing uni_bt_bredr_setup SSP auto-accept and encryption key size...\n");

    // Preconditions:
    // 1. uni_property_init() initializes the Posix TLV singleton queried by uni_bt_get_gap_security_level().
    // 2. hci_init() allocates hci_stack (requires a non-NULL transport with register_packet_handler).
    // 3. l2cap_init() prepares L2CAP before uni_bt_bredr_setup() registers HID PSMs and initializes SDP.
    uni_property_init();
    hci_init(&dummy_transport, NULL);
    l2cap_init();

    // Verify BTstack v1.8.2+ defaults prior to uni_bt_bredr_setup().
    hci_stack_t* stack = hci_get_stack();
    assert(stack != NULL);
    assert(stack->ssp_auto_accept == 0);
    assert(stack->gap_required_encyrption_key_size == 16);

    uni_bt_bredr_setup();

    // Verify D1 (SSP auto-accept enabled) and D2 (7-byte minimum encryption key size restored).
    assert(stack->ssp_auto_accept == 1);
    assert(stack->gap_required_encyrption_key_size == 7);
    assert(gap_get_security_level() == LEVEL_2);
    assert(stack->connectable == 1);
    assert(stack->discoverable == 0);
    assert(stack->new_page_scan_type == PAGE_SCAN_MODE_INTERLACED);
    assert(stack->inquiry_mode == INQUIRY_MODE_RSSI_AND_EIR);

    printf("PASS\n");
}

// Verifies that passing the shared Posix TLV singleton context (tlv_context_ptr) initialized by
// uni_property_init() to btstack_link_key_db_tlv_get_instance() and le_device_db_tlv_configure() (D4)
// persists BR/EDR link keys to disk without colliding with Bluepad32 'B' 'P' '3' property tags.
static void test_posix_shared_tlv_link_key_and_property_coexistence(void) {
    printf("Testing Posix shared TLV link key DB and property coexistence...\n");

    uni_property_init();

    const btstack_tlv_t* tlv_impl = NULL;
    btstack_tlv_posix_t* tlv_context_ptr = NULL;
    btstack_tlv_get_instance(&tlv_impl, (void**)&tlv_context_ptr);
    assert(tlv_impl != NULL);
    assert(tlv_context_ptr != NULL);
    assert(tlv_context_ptr->file != NULL);
    assert(tlv_context_ptr->db_path != NULL);
    assert(strcmp(tlv_context_ptr->db_path, "/tmp/bp32_property.tvl") == 0);

    uni_property_value_t orig_val = uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED);
    uni_property_set(UNI_PROPERTY_IDX_BLE_ENABLED, (uni_property_value_t){.boolean = false});
    assert(uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED).boolean == false);

    const btstack_link_key_db_t* link_key_db = btstack_link_key_db_tlv_get_instance(tlv_impl, tlv_context_ptr);
    assert(link_key_db != NULL);
    hci_set_link_key_db(link_key_db);
    le_device_db_tlv_configure(tlv_impl, tlv_context_ptr);

    fflush(tlv_context_ptr->file);
    long pos_before = ftell(tlv_context_ptr->file);
    assert(pos_before > 0);

    bd_addr_t test_addr = {0xA0, 0xAB, 0x51, 0x99, 0xA8, 0x8E};
    link_key_t test_key = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
                           0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x01};
    link_key_db->put_link_key(test_addr, test_key, COMBINATION_KEY);

    link_key_t out_key = {0};
    link_key_type_t out_type = INVALID_LINK_KEY;
    int found = link_key_db->get_link_key(test_addr, out_key, &out_type);
    assert(found == 1);
    assert(out_type == COMBINATION_KEY);
    assert(memcmp(out_key, test_key, sizeof(link_key_t)) == 0);

    // Note (EC-4): Do NOT call btstack_tlv_posix_deinit() mid-test, because it sets the file-scope
    // static flag btstack_tlv_posix_read_only = true inside btstack_tlv_posix.c and disables all
    // subsequent TLV writes in the process. Instead, verify on-disk persistence via fflush() + ftell().
    fflush(tlv_context_ptr->file);
    long pos_after = ftell(tlv_context_ptr->file);
    assert(pos_after > pos_before);

    assert(uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED).boolean == false);

    link_key_db->delete_link_key(test_addr);
    assert(link_key_db->get_link_key(test_addr, out_key, &out_type) == 0);
    assert(uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED).boolean == false);

    uni_property_set(UNI_PROPERTY_IDX_BLE_ENABLED, orig_val);
    assert(uni_property_get(UNI_PROPERTY_IDX_BLE_ENABLED).boolean == orig_val.boolean);

    printf("PASS\n");
}

static void test_virtual_child_creation_and_direct_delete(void) {
    printf("Testing virtual child creation and direct child deletion...\n");
    uni_virtual_device_set_enabled(true);
    assert(uni_virtual_device_is_enabled() == true);

    bd_addr_t addr = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26};
    uni_hid_device_t* parent = uni_hid_device_create(addr);
    assert(parent != NULL);

    uni_hid_device_t* child = uni_hid_device_create_virtual(parent);
    assert(child != NULL);
    assert(parent->child == child);
    assert(child->parent == parent);
    assert(uni_hid_device_is_virtual_device(child) == true);
    assert(uni_hid_device_is_virtual_device(parent) == false);
    assert(child->hids_cid == 0xffff);
    assert(uni_hid_device_get_instance_for_address(addr) == parent);

    // Deleting child directly MUST unlink parent->child
    uni_hid_device_delete(child);
    assert(parent->child == NULL);

    // Subsequent parent deletion must succeed without double-free
    uni_hid_device_delete(parent);
    assert(uni_hid_device_get_instance_for_address(addr) == NULL);
    printf("PASS\n");
}

static void test_parent_delete_cascades_to_virtual_child(void) {
    printf("Testing parent deletion cascading to virtual child...\n");
    uni_virtual_device_set_enabled(true);

    bd_addr_t addr = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36};
    uni_hid_device_t* parent = uni_hid_device_create(addr);
    assert(parent != NULL);

    uni_hid_device_t* child = uni_hid_device_create_virtual(parent);
    assert(child != NULL);
    assert(parent->child == child);

    uni_hid_device_delete(parent);
    assert(parent->child == NULL);
    assert(child->parent == NULL);
    assert(uni_hid_device_get_instance_for_address(addr) == NULL);
    printf("PASS\n");
}

static void dummy_timer_handler(btstack_timer_source_t* ts) {
    (void)ts;
}

static void test_intrusive_timer_removal_on_delete(void) {
    printf("Testing intrusive BTstack timer removal before memset...\n");
    bd_addr_t addr = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
    uni_hid_device_t* d = uni_hid_device_create(addr);
    assert(d != NULL);

    // Register and add inquiry_remote_name_timer and misc_button_delay_timer into run loop
    btstack_run_loop_set_timer_handler(&d->inquiry_remote_name_timer, dummy_timer_handler);
    btstack_run_loop_set_timer(&d->inquiry_remote_name_timer, 1000);
    btstack_run_loop_add_timer(&d->inquiry_remote_name_timer);

    btstack_run_loop_set_timer_handler(&d->misc_button_delay_timer, dummy_timer_handler);
    btstack_run_loop_set_timer(&d->misc_button_delay_timer, 1000);
    btstack_run_loop_add_timer(&d->misc_button_delay_timer);

    // Delete device: must remove all 3 timers before memset(d, 0, sizeof(*d))
    uni_hid_device_delete(d);

    // Verify BTstack run loop timer linked list is uncorrupted by adding and removing a fresh timer
    btstack_timer_source_t verify_timer;
    memset(&verify_timer, 0, sizeof(verify_timer));
    btstack_run_loop_set_timer_handler(&verify_timer, dummy_timer_handler);
    btstack_run_loop_set_timer(&verify_timer, 500);
    btstack_run_loop_add_timer(&verify_timer);
    assert(btstack_run_loop_remove_timer(&verify_timer) == 1);
    printf("PASS\n");
}

static void test_device_pool_exhaustion_and_null_guards(void) {
    printf("Testing device pool exhaustion and null guards...\n");
    uni_hid_device_t* created[CONFIG_BLUEPAD32_MAX_DEVICES] = {0};

    // Null parent guard for create_virtual
    uni_virtual_device_set_enabled(true);
    assert(uni_hid_device_create_virtual(NULL) == NULL);

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        bd_addr_t addr = {0x50, 0x00, 0x00, 0x00, 0x00, (uint8_t)(i + 1)};
        created[i] = uni_hid_device_create(addr);
        assert(created[i] != NULL);
    }

    // Pool is now full: both create and create_virtual must return NULL
    bd_addr_t extra_addr = {0x50, 0x00, 0x00, 0x00, 0x00, 0xFF};
    assert(uni_hid_device_create(extra_addr) == NULL);
    assert(uni_hid_device_create_virtual(created[0]) == NULL);

    // Null HID descriptor with non-zero length must be rejected safely
    uni_hid_device_set_hid_descriptor(created[0], NULL, 10);
    assert(uni_hid_device_has_hid_descriptor(created[0]) == false);
    assert(created[0]->hid_descriptor_len == 0);

    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        uni_hid_device_delete(created[i]);
    }
    printf("PASS\n");
}

static void test_cod_filtering(void) {
    printf("Testing Class of Device (CoD) filtering...\n");
    assert(uni_hid_device_is_cod_supported(UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD) == true);
    assert(uni_hid_device_is_cod_supported(UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_JOYSTICK) == true);
    assert(uni_hid_device_is_cod_supported(UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_KEYBOARD) == true);
    assert(uni_hid_device_is_cod_supported(UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE) == true);
    assert(uni_hid_device_is_cod_supported(0x00400408) == true);

    assert(uni_hid_device_is_cod_supported(0x000000) == false);
    assert(uni_hid_device_is_cod_supported(0x000200) == false);
    assert(uni_hid_device_is_cod_supported(UNI_BT_COD_MAJOR_PERIPHERAL) == false);
    printf("PASS\n");
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Initialize btstack run loop & properties
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    uni_property_init();

    // Basic setup
    uni_hid_device_setup();

    // uni_circular_buffer
    test_circular_buffer_initial_and_reset_state();
    test_circular_buffer_null_and_negative_length_guards();
    test_circular_buffer_zero_length_packet();
    test_circular_buffer_exact_capacity_boundary();
    test_circular_buffer_full_queue_and_wrap_around();

    // uni_bt_allowlist & uni_property_posix
    test_allowlist_zero_addr_rejection();
    test_allowlist_crud_duplicate_and_capacity();
    test_allowlist_enforcement_toggle();
    test_allowlist_tlv_string_persistence_roundtrip();
    test_gap_security_level_u8_union();
    test_property_types_and_guards();
    test_property_null_platform_guard();

    // uni_hid_device
    test_create_device();
    test_device_properties();
    test_bredr_setup_ssp_and_encryption_key_size();
    test_posix_shared_tlv_link_key_and_property_coexistence();
    test_virtual_child_creation_and_direct_delete();
    test_parent_delete_cascades_to_virtual_child();
    test_intrusive_timer_removal_on_delete();
    test_device_pool_exhaustion_and_null_guards();
    test_cod_filtering();

    printf("All uni_hid_device tests passed!\n");
    return 0;
}
