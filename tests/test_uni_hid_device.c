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

#include "bt/uni_bt_bredr.h"
#include "platform/uni_platform.h"
#include "uni_hid_device.h"
#include "uni_property.h"

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
    return UNI_ERROR_SUCCESS;
}

static void my_on_device_connected(uni_hid_device_t* d) {}

static void my_on_device_disconnected(uni_hid_device_t* d) {}

static uni_error_t my_on_device_ready(uni_hid_device_t* d) {
    return UNI_ERROR_SUCCESS;
}

static void my_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {}

static const uni_property_t* my_get_property(uni_property_idx_t idx) {
    return NULL;
}

static void my_on_oob_event(uni_platform_oob_event_t event, void* data) {}

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

// Tests

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

int main(int argc, char** argv) {
    // Initialize btstack run loop
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());

    // Basic setup
    uni_hid_device_setup();

    test_create_device();
    test_device_properties();
    test_bredr_setup_ssp_and_encryption_key_size();
    test_posix_shared_tlv_link_key_and_property_coexistence();

    printf("All tests passed!\n");
    return 0;
}
