#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <btstack_memory.h>
#include <btstack_run_loop.h>
#include <btstack_run_loop_posix.h>

#include "platform/uni_platform.h"
#include "uni_hid_device.h"

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

int main(int argc, char** argv) {
    // Initialize btstack run loop
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());

    // Basic setup
    uni_hid_device_setup();

    test_create_device();
    test_device_properties();

    printf("All tests passed!\n");
    return 0;
}
