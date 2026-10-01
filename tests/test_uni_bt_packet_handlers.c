// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ricardo Quesada
// http://retro.moe/unijoysticle2

/**
 * @file test_uni_bt_packet_handlers.c
 * @brief Layer 2 BTstack Event, GAP, L2CAP, SDP, and BLE Packet Handler Contract Test Suite.
 *
 * Exercises Bluepad32's Bluetooth state machines and protocol event handlers in-process:
 *  1. `TEST(bt_setup_state_machine_full_walk)`: Full walk of `uni_bt_setup()` through all 7
 *     setup steps (`SETUP_STATE_READY`), command-credit stall handling, re-invocation reset
 *     (`setup_fn_idx = 0`), and `on_init_complete == NULL` safety check.
 *  2. `TEST(bt_gap_inquiry_result_with_rssi_and_cod_filter)`: Synthesizes `GAP_EVENT_INQUIRY_RESULT`
 *     (valid Gamepad CoD + EIR VID/PID/Name vs unsupported Computer CoD vs weak RSSI) and
 *     `HCI_EVENT_PIN_CODE_REQUEST` (Gamepad reversed local BD_ADDR PIN vs Keyboard "0000" PIN).
 *  3. `TEST(bt_l2cap_channel_opened_psm_routing_and_error_cleanup)`: Synthesizes
 *     `L2CAP_EVENT_CHANNEL_OPENED` (`PSM_HID_CONTROL = 0x0011`, `PSM_HID_INTERRUPT = 0x0013`)
 *     and non-zero status error cleanup.
 *  4. `TEST(bt_l2cap_incoming_connection_accept_and_decline)`: Synthesizes
 *     `L2CAP_EVENT_INCOMING_CONNECTION` (accepting supported device and declining when
 *     allowlist or incoming-connection policy rejects).
 *  5. `TEST(bt_sdp_pid_and_hid_query_result_chunks_and_truncation)`: Uses
 *     `uni_bt_sdp_set_device_for_test()`, `uni_handle_sdp_pid_query_result()`, and
 *     `uni_handle_sdp_hid_query_result()` to feed multi-chunk DES (`0x0201` VID, `0x0202` PID,
 *     `0x0206` HID descriptor list) and an adversarial `> 512`-byte oversized HID descriptor
 *     stream verifying truncation/rejection without overflow.
 *  6. `TEST(bt_le_adv_report_64byte_name_overflow_regression)`: Synthesizes
 *     `GAP_EVENT_ADVERTISING_REPORT` with Appearance `0x03C4` (Gamepad), non-HID Appearance
 *     `0x0040` (Phone), and a 100-byte oversized `BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME`
 *     verifying the 63-byte clamp (`strlen(name) == 63`, `name[63] == '\0'`).
 *  7. `TEST(bt_bredr_l2cap_data_packet_strips_header_and_routes)`: Synthesizes `L2CAP_DATA_PACKET`
 *     starting with `0xa1` (`HID_MESSAGE_TYPE_DATA | HID_REPORT_TYPE_INPUT`) on the HID Interrupt
 *     CID and verifies `&packet[1], size - 1` is routed to `uni_hid_parse_input_report()`.
 *  8. `TEST(bt_disconnect_cleans_up_device)`: Synthesizes `HCI_EVENT_DISCONNECTION_COMPLETE`
 *     and verifies device cleanup and `on_device_disconnected` callback invocation.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ble/att_db.h>
#include <ble/le_device_db_tlv.h>
#include <bluetooth_data_types.h>
#include <btstack.h>
#include <btstack_memory.h>
#include <btstack_run_loop.h>
#include <btstack_run_loop_base.h>
#include <btstack_run_loop_posix.h>
#include <btstack_tlv.h>
#include <classic/sdp_util.h>
#include <hci.h>
#include <hci_transport.h>
#include <l2cap.h>

#include "bt/uni_bt.h"
#include "bt/uni_bt_allowlist.h"
#include "bt/uni_bt_bredr.h"
#include "bt/uni_bt_conn.h"
#include "bt/uni_bt_defines.h"
#include "bt/uni_bt_le.h"
#include "bt/uni_bt_sdp.h"
#include "bt/uni_bt_service.h"
#include "bt/uni_bt_setup.h"
#include "controller/uni_controller.h"
#include "controller/uni_controller_type.h"
#include "controller/uni_gamepad.h"
#include "platform/uni_platform.h"
#include "sdkconfig.h"
#include "test_check.h"
#include "uni_common.h"
#include "uni_config.h"
#include "uni_error.h"
#include "uni_hid_device.h"
#include "uni_log.h"
#include "uni_property.h"
#include "uni_virtual_device.h"

// Silence verbose protocol logs during unit tests unless debugging.
static bool g_silence_logs = true;

void uni_logv(const char* fmt, va_list args) {
    if (g_silence_logs) {
        return;
    }
    vfprintf(stdout, fmt, args);
}

// ============================================================================
// Mock HCI Transport & Mock Platform Vtable
// ============================================================================

static int g_transport_open_count = 0;
static int g_transport_send_count = 0;
static uint16_t g_last_hci_cmd_opcode = 0;
static uint16_t g_pending_crypto_opcode = 0;
static uint8_t g_last_hci_cmd_buf[260];
static int g_last_hci_cmd_size = 0;
static uint8_t g_last_acl_buf[260];
static int g_last_acl_size = 0;
static bool g_auto_replenish_credits = true;
static void (*g_transport_packet_handler)(uint8_t packet_type, uint8_t* packet, uint16_t size) = NULL;

static void dummy_transport_init(const void* transport_config) {
    ARG_UNUSED(transport_config);
}

static int dummy_transport_open(void) {
    g_transport_open_count++;
    return 0;
}

static int dummy_transport_close(void) {
    return 0;
}

static void dummy_transport_register_packet_handler(void (*handler)(uint8_t packet_type,
                                                                    uint8_t* packet,
                                                                    uint16_t size)) {
    g_transport_packet_handler = handler;
}

static int dummy_transport_can_send_packet_now(uint8_t packet_type) {
    ARG_UNUSED(packet_type);
    return 1;
}

static int dummy_transport_send_packet(uint8_t packet_type, uint8_t* packet, int size) {
    g_transport_send_count++;
    if (packet_type == HCI_COMMAND_DATA_PACKET && size >= 2) {
        g_last_hci_cmd_opcode = little_endian_read_16(packet, 0);
        if (g_last_hci_cmd_opcode == 0x2018 || g_last_hci_cmd_opcode == 0x2017) {
            g_pending_crypto_opcode = g_last_hci_cmd_opcode;
        }
        g_last_hci_cmd_size = (size < (int)sizeof(g_last_hci_cmd_buf)) ? size : (int)sizeof(g_last_hci_cmd_buf);
        memcpy(g_last_hci_cmd_buf, packet, (size_t)g_last_hci_cmd_size);
    } else if (packet_type == HCI_ACL_DATA_PACKET && size > 0) {
        g_last_acl_size = (size < (int)sizeof(g_last_acl_buf)) ? size : (int)sizeof(g_last_acl_buf);
        memcpy(g_last_acl_buf, packet, (size_t)g_last_acl_size);
    }
    // Release BTstack's outgoing packet buffer lock and replenish HCI command credits
    // so synchronous unit tests can issue consecutive HCI commands without spinning
    // the POSIX run loop.
    if (hci_get_stack()) {
        hci_get_stack()->hci_packet_buffer_reserved = false;
        if (g_auto_replenish_credits) {
            hci_get_stack()->num_cmd_packets = 255;
        }
    }
    return 0;
}

static const hci_transport_t g_dummy_transport = {
    .name = "dummy_bt_packet_test",
    .init = dummy_transport_init,
    .open = dummy_transport_open,
    .close = dummy_transport_close,
    .register_packet_handler = dummy_transport_register_packet_handler,
    .can_send_packet_now = dummy_transport_can_send_packet_now,
    .send_packet = dummy_transport_send_packet,
};

// Mock Platform Tracking Counters
static int g_init_complete_count = 0;
static int g_discovered_count = 0;
static int g_connected_count = 0;
static int g_disconnected_count = 0;
static int g_ready_count = 0;
static int g_controller_data_count = 0;
static int g_oob_event_count = 0;
static uni_error_t g_discover_return_value = UNI_ERROR_SUCCESS;
static uni_error_t g_ready_return_value = UNI_ERROR_SUCCESS;

static void mock_platform_init(int argc, const char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
}

static void mock_platform_on_init_complete(void) {
    g_init_complete_count++;
}

static uni_error_t mock_platform_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    (void)addr;
    ARG_UNUSED(name);
    ARG_UNUSED(cod);
    ARG_UNUSED(rssi);
    g_discovered_count++;
    return g_discover_return_value;
}

static void mock_platform_on_device_connected(uni_hid_device_t* d) {
    ARG_UNUSED(d);
    g_connected_count++;
}

static void mock_platform_on_device_disconnected(uni_hid_device_t* d) {
    ARG_UNUSED(d);
    g_disconnected_count++;
}

static uni_error_t mock_platform_on_device_ready(uni_hid_device_t* d) {
    ARG_UNUSED(d);
    g_ready_count++;
    return g_ready_return_value;
}

static void mock_platform_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    ARG_UNUSED(d);
    ARG_UNUSED(ctl);
    g_controller_data_count++;
}

static void mock_platform_on_oob_event(uni_platform_oob_event_t event, void* data) {
    ARG_UNUSED(event);
    ARG_UNUSED(data);
    g_oob_event_count++;
}

static const uni_property_t* mock_platform_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

static struct uni_platform g_mock_platform = {
    .name = "test_bt_packet_handlers_mock",
    .init = mock_platform_init,
    .on_init_complete = mock_platform_on_init_complete,
    .on_device_discovered = mock_platform_on_device_discovered,
    .on_device_connected = mock_platform_on_device_connected,
    .on_device_disconnected = mock_platform_on_device_disconnected,
    .on_device_ready = mock_platform_on_device_ready,
    .on_controller_data = mock_platform_on_controller_data,
    .on_oob_event = mock_platform_on_oob_event,
    .get_property = mock_platform_get_property,
};

struct uni_platform* uni_get_platform(void) {
    return &g_mock_platform;
}

// ============================================================================
// Packet Construction & Reset Helpers
// ============================================================================

/**
 * @brief Replenish BTstack HCI command credits and release outgoing packet buffer reservation.
 */
static void replenish_hci_cmd_credits(void) {
    if (hci_get_stack()) {
        hci_get_stack()->hci_packet_buffer_reserved = false;
        hci_get_stack()->num_cmd_packets = 255;
    }
}

/**
 * @brief Store a `bd_addr_t` in little-endian (reversed) byte order as expected by BTstack event getters.
 */
static void put_bd_addr_reversed(uint8_t* dst, const bd_addr_t addr) {
    reverse_bd_addr(addr, dst);
}

/**
 * @brief Reset mock counters, allowlist, and HID device table between tests.
 */
static void reset_test_fixture(void) {
    btstack_run_loop_base_timers = NULL;
    uni_hid_device_setup();
    uni_bt_allowlist_remove_all();
    uni_bt_allowlist_set_enabled(false);
    uni_bt_allow_incoming_connections(true);
    uni_bt_sdp_set_device_for_test(NULL);

    g_mock_platform.on_init_complete = mock_platform_on_init_complete;
    g_discover_return_value = UNI_ERROR_SUCCESS;
    g_ready_return_value = UNI_ERROR_SUCCESS;
    g_auto_replenish_credits = true;
    replenish_hci_cmd_credits();

    g_init_complete_count = 0;
    g_discovered_count = 0;
    g_connected_count = 0;
    g_disconnected_count = 0;
    g_ready_count = 0;
    g_controller_data_count = 0;
    g_oob_event_count = 0;
    g_last_hci_cmd_opcode = 0;
}

// ============================================================================
// 1. TEST(bt_setup_state_machine_full_walk)
// ============================================================================

TEST(bt_setup_state_machine_full_walk) {
    reset_test_fixture();

    // Ensure BLE and BR/EDR are enabled so uni_bt_setup() exercises both setup paths.
    uni_bt_bredr_set_enabled(true);
    uni_bt_le_set_enabled(true);

    int err = uni_bt_setup();
    ASSERT_EQ(UNI_ERROR_SUCCESS, err);
    EXPECT_FALSE(uni_bt_setup_is_ready());
    ASSERT_EQ(0, g_init_complete_count);

    // Verify BTstack hci_stack_t invariants configured by uni_bt_bredr_setup():
    // - SSP auto-accept enabled (BTstack v1.8.2+ regression guard)
    // - Minimum encryption key size set to 7 bytes (Errata 11838)
    // - Connectable enabled, inquiry mode set to RSSI + EIR
    ASSERT_NE(NULL, hci_get_stack());
    EXPECT_EQ(1, hci_get_stack()->ssp_auto_accept);
    EXPECT_EQ(7, hci_get_stack()->gap_required_encyrption_key_size);
    EXPECT_EQ(1, hci_get_stack()->connectable);
    EXPECT_EQ(0, hci_get_stack()->discoverable);
    EXPECT_EQ(INQUIRY_MODE_RSSI_AND_EIR, hci_get_stack()->inquiry_mode);

    // Non-HCI_EVENT_PACKET must be ignored while setup is not ready.
    uint8_t dummy_l2cap[] = {0xa1, 0x01, 0x00};
    uni_bt_packet_handler(L2CAP_DATA_PACKET, 0x0040, dummy_l2cap, sizeof(dummy_l2cap));
    EXPECT_FALSE(uni_bt_setup_is_ready());

    // BTSTACK_EVENT_POWERON_FAILED and HCI_STATE_INITIALIZING must not advance setup_state.
    uint8_t poweron_failed_pkt[] = {BTSTACK_EVENT_POWERON_FAILED, 0};
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, poweron_failed_pkt, sizeof(poweron_failed_pkt));
    EXPECT_FALSE(uni_bt_setup_is_ready());

    uint8_t state_init_pkt[] = {BTSTACK_EVENT_STATE, 1, HCI_STATE_INITIALIZING};
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, state_init_pkt, sizeof(state_init_pkt));
    EXPECT_FALSE(uni_bt_setup_is_ready());

    // Step 1: BTSTACK_EVENT_STATE (HCI_STATE_WORKING) transitions to SETUP_STATE_BLUEPAD32_IN_PROGRESS
    // and executes setup_fns[0] (setup_write_simple_pairing_mode, opcode 0x0c56).
    replenish_hci_cmd_credits();
    uint8_t state_working_pkt[] = {BTSTACK_EVENT_STATE, 1, HCI_STATE_WORKING};
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, state_working_pkt, sizeof(state_working_pkt));
    EXPECT_FALSE(uni_bt_setup_is_ready());
    EXPECT_EQ(0x0c56, g_last_hci_cmd_opcode);

    // Test command-credit stall branch: when num_cmd_packets == 0, HCI_EVENT_COMMAND_COMPLETE
    // must not advance setup_fn_idx or crash.
    g_auto_replenish_credits = false;
    hci_get_stack()->num_cmd_packets = 0;
    uint8_t cmd_complete_pkt[] = {HCI_EVENT_COMMAND_COMPLETE, 4, 1, 0x56, 0x0c, 0x00};
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, cmd_complete_pkt, sizeof(cmd_complete_pkt));
    EXPECT_FALSE(uni_bt_setup_is_ready());
    ASSERT_EQ(0, g_init_complete_count);

    // Restore command credits and walk through all 7 setup/command-complete steps:
    // Step 2 executes setup_fns[1] (setup_set_event_filter, opcode 0x0c05) and transitions
    // to SETUP_STATE_READY; subsequent steps verify post-ready HCI_EVENT_COMMAND_COMPLETE handling.
    g_auto_replenish_credits = true;
    replenish_hci_cmd_credits();
    for (int step = 0; step < 7; step++) {
        replenish_hci_cmd_credits();
        uni_bt_packet_handler(HCI_EVENT_PACKET, 0, cmd_complete_pkt, sizeof(cmd_complete_pkt));
    }
    EXPECT_TRUE(uni_bt_setup_is_ready());
    EXPECT_EQ(1, g_init_complete_count);

    // Re-invocation reset (setup_fn_idx = 0) & NULL on_init_complete safety check:
    g_mock_platform.on_init_complete = NULL;
    err = uni_bt_setup();
    ASSERT_EQ(UNI_ERROR_SUCCESS, err);
    EXPECT_FALSE(uni_bt_setup_is_ready());

    replenish_hci_cmd_credits();
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, state_working_pkt, sizeof(state_working_pkt));
    EXPECT_FALSE(uni_bt_setup_is_ready());

    replenish_hci_cmd_credits();
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, cmd_complete_pkt, sizeof(cmd_complete_pkt));
    EXPECT_TRUE(uni_bt_setup_is_ready());
    // Count remains 1 because on_init_complete was NULL during the second walk.
    EXPECT_EQ(1, g_init_complete_count);

    g_mock_platform.on_init_complete = mock_platform_on_init_complete;
}

// ============================================================================
// 2. TEST(bt_gap_inquiry_result_with_rssi_and_cod_filter)
// ============================================================================

/**
 * @brief Synthesize a BTstack `GAP_EVENT_INQUIRY_RESULT` (0xE1) HCI event packet.
 *
 * Encodes peer BD_ADDR, page scan repetition mode, 24-bit Class of Device (CoD), clock offset,
 * optional RSSI, optional Device ID (VID/PID), and optional EIR name in BTstack's exact getter layout.
 */
static uint16_t build_gap_inquiry_result_pkt(uint8_t* pkt,
                                             const bd_addr_t addr,
                                             uint8_t page_scan_rep_mode,
                                             uint32_t cod,
                                             uint16_t clock_offset,
                                             uint8_t rssi_avail,
                                             uint8_t rssi,
                                             uint8_t dev_id_avail,
                                             uint16_t vid,
                                             uint16_t pid,
                                             const char* name) {
    uint8_t name_len = name ? (uint8_t)strlen(name) : 0;
    pkt[0] = GAP_EVENT_INQUIRY_RESULT;
    pkt[1] = (uint8_t)(25 + name_len);
    put_bd_addr_reversed(&pkt[2], addr);
    pkt[8] = page_scan_rep_mode;
    little_endian_store_24(pkt, 9, cod);
    little_endian_store_16(pkt, 12, clock_offset);
    pkt[14] = rssi_avail;
    pkt[15] = rssi;
    pkt[16] = dev_id_avail;
    little_endian_store_16(pkt, 17, 0x0001);  // vendor_id_source (Bluetooth SIG)
    little_endian_store_16(pkt, 19, vid);
    little_endian_store_16(pkt, 21, pid);
    little_endian_store_16(pkt, 23, 0x0100);  // version
    pkt[25] = (name != NULL) ? 1 : 0;
    pkt[26] = name_len;
    if (name_len > 0) {
        memcpy(&pkt[27], name, name_len);
    }
    return (uint16_t)(27 + name_len);
}

TEST(bt_gap_inquiry_result_with_rssi_and_cod_filter) {
    reset_test_fixture();
    uint8_t pkt[128];

    // Case 1: Unsupported Class of Device (Computer Major CoD = 0x0100) -> Ignored.
    bd_addr_t addr_computer = {0x11, 0x22, 0x33, 0x44, 0x55, 0x01};
    uint16_t pkt_len =
        build_gap_inquiry_result_pkt(pkt, addr_computer, 0x01, 0x0100, 0x1234, 1, 210, 0, 0, 0, "MacBook Pro");
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, pkt, pkt_len);
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(addr_computer));
    EXPECT_EQ(0, g_discovered_count);

    // Case 2: Supported Gamepad CoD but RSSI (100) below UNI_BT_RSSI_THRESHOLD (155) -> Ignored.
    bd_addr_t addr_weak_rssi = {0x11, 0x22, 0x33, 0x44, 0x55, 0x02};
    uint32_t gamepad_cod = UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD;
    pkt_len = build_gap_inquiry_result_pkt(pkt, addr_weak_rssi, 0x01, gamepad_cod, 0x1234, 1, 100, 1, 0x054c, 0x0268,
                                           "PLAYSTATION(R)3 Controller");
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, pkt, pkt_len);
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(addr_weak_rssi));
    EXPECT_EQ(0, g_discovered_count);

    // Case 3: Supported Gamepad CoD (0x0508) with strong RSSI (215) + EIR VID/PID/Name -> Accepted & Created.
    bd_addr_t addr_gamepad = {0x11, 0x22, 0x33, 0x44, 0x55, 0x03};
    pkt_len = build_gap_inquiry_result_pkt(pkt, addr_gamepad, 0x01, gamepad_cod, 0x1234, 1, 215, 1, 0x054c, 0x0268,
                                           "PLAYSTATION(R)3 Controller");
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, pkt, pkt_len);

    uni_hid_device_t* d = uni_hid_device_get_instance_for_address(addr_gamepad);
    ASSERT_NE(NULL, d);
    EXPECT_EQ(1, g_discovered_count);
    EXPECT_EQ(gamepad_cod, d->cod);
    EXPECT_EQ(215, d->conn.rssi);
    EXPECT_EQ(0x01, d->conn.page_scan_repetition_mode);
    EXPECT_EQ(0x1234 | UNI_BT_CLOCK_OFFSET_VALID, d->conn.clock_offset);
    EXPECT_EQ(0, strcmp("PLAYSTATION(R)3 Controller", d->name));
    EXPECT_EQ(CONTROLLER_TYPE_PS3Controller, d->controller_type);
    EXPECT_EQ(SDP_QUERY_NOT_NEEDED, d->sdp_query_type);

    // Case 4: HCI_EVENT_PIN_CODE_REQUEST for Gamepad (6-byte reversed local BD_ADDR PIN)
    // vs Keyboard ("0000" 4-byte PIN).
    hci_get_stack()->gap_pairing_state = 0;
    uint8_t pin_pkt[8];
    pin_pkt[0] = HCI_EVENT_PIN_CODE_REQUEST;
    pin_pkt[1] = 6;
    put_bd_addr_reversed(&pin_pkt[2], addr_gamepad);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, pin_pkt, sizeof(pin_pkt));
    EXPECT_EQ(6, hci_get_stack()->gap_pairing_pin_len);

    hci_get_stack()->gap_pairing_state = 0;
    bd_addr_t addr_kb = {0x11, 0x22, 0x33, 0x44, 0x55, 0x04};
    uni_hid_device_t* d_kb = uni_hid_device_create(addr_kb);
    ASSERT_NE(NULL, d_kb);
    uni_hid_device_set_cod(d_kb, UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_KEYBOARD);
    put_bd_addr_reversed(&pin_pkt[2], addr_kb);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, pin_pkt, sizeof(pin_pkt));
    EXPECT_EQ(4, hci_get_stack()->gap_pairing_pin_len);
    EXPECT_EQ(0, memcmp("0000", hci_get_stack()->gap_pairing_input.gap_pairing_pin, 4));
}

// ============================================================================
// 3. TEST(bt_l2cap_channel_opened_psm_routing_and_error_cleanup)
// ============================================================================

/**
 * @brief Synthesize a 26-byte BTstack `L2CAP_EVENT_CHANNEL_OPENED` (0x70) HCI event packet.
 */
static void build_l2cap_channel_opened_pkt(uint8_t pkt[26],
                                           uint8_t status,
                                           const bd_addr_t addr,
                                           hci_con_handle_t handle,
                                           uint16_t psm,
                                           uint16_t local_cid,
                                           uint16_t remote_cid,
                                           uint8_t incoming) {
    memset(pkt, 0, 26);
    pkt[0] = L2CAP_EVENT_CHANNEL_OPENED;
    pkt[1] = 24;
    pkt[2] = status;
    put_bd_addr_reversed(&pkt[3], addr);
    little_endian_store_16(pkt, 9, handle);
    little_endian_store_16(pkt, 11, psm);
    little_endian_store_16(pkt, 13, local_cid);
    little_endian_store_16(pkt, 15, remote_cid);
    little_endian_store_16(pkt, 17, 672);     // local_mtu
    little_endian_store_16(pkt, 19, 672);     // remote_mtu
    little_endian_store_16(pkt, 21, 0xffff);  // flush_timeout
    pkt[23] = incoming;
}

TEST(bt_l2cap_channel_opened_psm_routing_and_error_cleanup) {
    reset_test_fixture();
    uint8_t pkt[26];

    // Create a known gamepad device (DualShock 3, SDP_QUERY_NOT_NEEDED).
    bd_addr_t addr = {0x22, 0x33, 0x44, 0x55, 0x66, 0x01};
    uni_hid_device_t* d = uni_hid_device_create(addr);
    ASSERT_NE(NULL, d);
    uni_hid_device_set_name(d, "PLAYSTATION(R)3 Controller");
    EXPECT_TRUE(uni_hid_device_guess_controller_type_from_name(d, d->name));
    d->sdp_query_type = SDP_QUERY_NOT_NEEDED;

    // 1. Open PSM_HID_CONTROL (0x0011) -> routes to d->conn.control_cid = 0x0040.
    build_l2cap_channel_opened_pkt(pkt, 0x00, addr, 0x0031, PSM_HID_CONTROL, 0x0040, 0x0050, 0);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0x0040, pkt, sizeof(pkt));
    EXPECT_EQ(0x0040, d->conn.control_cid);
    EXPECT_FALSE(d->conn.connected);
    EXPECT_EQ(0, g_connected_count);

    // 2. Open PSM_HID_INTERRUPT (0x0013) -> routes to d->conn.interrupt_cid = 0x0041,
    // marks device connected, and transitions FSM to UNI_BT_CONN_STATE_DEVICE_READY.
    build_l2cap_channel_opened_pkt(pkt, 0x00, addr, 0x0031, PSM_HID_INTERRUPT, 0x0041, 0x0051, 0);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0x0041, pkt, sizeof(pkt));
    EXPECT_EQ(0x0041, d->conn.interrupt_cid);
    EXPECT_TRUE(d->conn.connected);
    EXPECT_EQ(1, g_connected_count);
    EXPECT_EQ(1, g_ready_count);
    EXPECT_EQ(UNI_BT_CONN_STATE_DEVICE_READY, uni_bt_conn_get_state(&d->conn));

    // 3. Non-zero status error on L2CAP_EVENT_CHANNEL_OPENED -> disconnects & deletes device.
    bd_addr_t addr_err = {0x22, 0x33, 0x44, 0x55, 0x66, 0x02};
    uni_hid_device_t* d_err = uni_hid_device_create(addr_err);
    ASSERT_NE(NULL, d_err);
    uni_hid_device_connect(d_err);
    int prev_disc = g_disconnected_count;

    build_l2cap_channel_opened_pkt(pkt, L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY, addr_err, 0x0032,
                                   PSM_HID_CONTROL, 0x0042, 0x0052, 0);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0x0042, pkt, sizeof(pkt));
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(addr_err));
    EXPECT_EQ(prev_disc + 1, g_disconnected_count);
}

// ============================================================================
// 4. TEST(bt_l2cap_incoming_connection_accept_and_decline)
// ============================================================================

/**
 * @brief Synthesize a 16-byte BTstack `L2CAP_EVENT_INCOMING_CONNECTION` (0x72) HCI event packet.
 */
static void build_l2cap_incoming_conn_pkt(uint8_t pkt[16],
                                          const bd_addr_t addr,
                                          hci_con_handle_t handle,
                                          uint16_t psm,
                                          uint16_t local_cid,
                                          uint16_t remote_cid) {
    memset(pkt, 0, 16);
    pkt[0] = L2CAP_EVENT_INCOMING_CONNECTION;
    pkt[1] = 14;
    put_bd_addr_reversed(&pkt[2], addr);
    little_endian_store_16(pkt, 8, handle);
    little_endian_store_16(pkt, 10, psm);
    little_endian_store_16(pkt, 12, local_cid);
    little_endian_store_16(pkt, 14, remote_cid);
}

TEST(bt_l2cap_incoming_connection_accept_and_decline) {
    reset_test_fixture();
    uint8_t pkt[16];

    bd_addr_t allowed_addr = {0x33, 0x44, 0x55, 0x66, 0x77, 0x01};
    bd_addr_t blocked_addr = {0x33, 0x44, 0x55, 0x66, 0x77, 0x99};

    // Enable allowlist with only `allowed_addr` registered.
    EXPECT_TRUE(uni_bt_allowlist_add_addr(allowed_addr));
    uni_bt_allowlist_set_enabled(true);

    // 1. Incoming connection from `blocked_addr` (not in allowlist) -> Declined, no device created.
    build_l2cap_incoming_conn_pkt(pkt, blocked_addr, 0x0050, PSM_HID_CONTROL, 0x0060, 0x0070);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0x0060, pkt, sizeof(pkt));
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(blocked_addr));

    // 2. Incoming PSM_HID_CONTROL connection from `allowed_addr` -> Accepted, device created & marked incoming.
    build_l2cap_incoming_conn_pkt(pkt, allowed_addr, 0x0051, PSM_HID_CONTROL, 0x0061, 0x0071);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0x0061, pkt, sizeof(pkt));
    uni_hid_device_t* d = uni_hid_device_get_instance_for_address(allowed_addr);
    ASSERT_NE(NULL, d);
    EXPECT_TRUE(uni_hid_device_is_incoming(d));
    EXPECT_EQ(0x0051, d->conn.handle);
    EXPECT_EQ(0x0061, d->conn.control_cid);

    // 3. Incoming PSM_HID_INTERRUPT connection from `allowed_addr` -> Accepted, interrupt_cid assigned.
    build_l2cap_incoming_conn_pkt(pkt, allowed_addr, 0x0051, PSM_HID_INTERRUPT, 0x0062, 0x0072);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0x0062, pkt, sizeof(pkt));
    EXPECT_EQ(0x0062, d->conn.interrupt_cid);

    // 4. Incoming connections globally disabled (`uni_bt_allow_incoming_connections(false)`) -> Declined.
    uni_bt_allowlist_set_enabled(false);
    uni_bt_allow_incoming_connections(false);
    bd_addr_t addr_disabled = {0x33, 0x44, 0x55, 0x66, 0x77, 0x02};
    build_l2cap_incoming_conn_pkt(pkt, addr_disabled, 0x0052, PSM_HID_CONTROL, 0x0063, 0x0073);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0x0063, pkt, sizeof(pkt));
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(addr_disabled));
    uni_bt_allow_incoming_connections(true);
}

// ============================================================================
// 5. TEST(bt_sdp_pid_and_hid_query_result_chunks_and_truncation)
// ============================================================================

/**
 * @brief Stream a multi-byte SDP Data Element byte-by-byte via `SDP_EVENT_QUERY_ATTRIBUTE_VALUE` (0x93).
 */
static void stream_sdp_attribute_bytes(void (*handler)(uint8_t, uint16_t, uint8_t*, uint16_t),
                                       uint16_t record_id,
                                       uint16_t attribute_id,
                                       const uint8_t* data,
                                       uint16_t attribute_len) {
    uint8_t pkt[11];
    pkt[0] = SDP_EVENT_QUERY_ATTRIBUTE_VALUE;
    pkt[1] = 9;
    little_endian_store_16(pkt, 2, record_id);
    little_endian_store_16(pkt, 4, attribute_id);
    little_endian_store_16(pkt, 6, attribute_len);
    for (uint16_t offset = 0; offset < attribute_len; offset++) {
        little_endian_store_16(pkt, 8, offset);
        pkt[10] = data[offset];
        handler(HCI_EVENT_PACKET, 0, pkt, sizeof(pkt));
    }
}

TEST(bt_sdp_pid_and_hid_query_result_chunks_and_truncation) {
    reset_test_fixture();

    bd_addr_t addr = {0x44, 0x55, 0x66, 0x77, 0x88, 0x01};
    uni_hid_device_t* d = uni_hid_device_create(addr);
    ASSERT_NE(NULL, d);
    // Mark as incoming so SDP completion does not attempt to open a live L2CAP channel.
    uni_hid_device_set_incoming(d, true);
    uni_bt_sdp_set_device_for_test(d);

    // 1. Stream BLUETOOTH_ATTRIBUTE_VENDOR_ID (0x0201) = 0x054c (Sony) and
    //    BLUETOOTH_ATTRIBUTE_PRODUCT_ID (0x0202) = 0x09cc (DualShock 4 v2) as 3-byte DE_UINT16 elements.
    const uint8_t de_vid_sony[] = {0x09, 0x05, 0x4c};
    const uint8_t de_pid_ds4[] = {0x09, 0x09, 0xcc};
    stream_sdp_attribute_bytes(uni_handle_sdp_pid_query_result, 1, BLUETOOTH_ATTRIBUTE_VENDOR_ID, de_vid_sony,
                               sizeof(de_vid_sony));
    stream_sdp_attribute_bytes(uni_handle_sdp_pid_query_result, 1, BLUETOOTH_ATTRIBUTE_PRODUCT_ID, de_pid_ds4,
                               sizeof(de_pid_ds4));
    EXPECT_EQ(0x054c, uni_hid_device_get_vendor_id(d));
    EXPECT_EQ(0x09cc, uni_hid_device_get_product_id(d));

    // Complete the PnP ID query; verify controller_type is resolved to PS4Controller.
    uint8_t sdp_complete_pkt[] = {SDP_EVENT_QUERY_COMPLETE, 1, 0x00};
    uni_handle_sdp_pid_query_result(HCI_EVENT_PACKET, 0, sdp_complete_pkt, sizeof(sdp_complete_pkt));
    EXPECT_EQ(CONTROLLER_TYPE_PS4Controller, d->controller_type);

    // 2. Stream BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST (0x0206) as a nested DES -> DES -> DE_STRING.
    static const uint8_t sample_hid_desc[] = {
        0x05, 0x01,        // Usage Page (Generic Desktop)
        0x09, 0x05,        // Usage (Game Pad)
        0xa1, 0x01,        // Collection (Application)
        0x09, 0x30,        //   Usage (X)
        0x09, 0x31,        //   Usage (Y)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xff, 0x00,  // Logical Maximum (255)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x02,        //   Report Count (2)
        0x81, 0x02,        //   Input (Data,Var,Abs)
        0xc0               // End Collection
    };
    uint8_t des_buf[64];
    de_create_sequence(des_buf);
    uint8_t* sub_seq = de_push_sequence(des_buf);
    de_add_number(sub_seq, DE_UINT, DE_SIZE_8, 0x22);  // Report Descriptor type
    de_add_data(sub_seq, DE_STRING, sizeof(sample_hid_desc), (uint8_t*)sample_hid_desc);
    de_pop_sequence(des_buf, sub_seq);
    uint16_t des_len = (uint16_t)de_get_len(des_buf);

    uni_bt_sdp_set_device_for_test(d);
    stream_sdp_attribute_bytes(uni_handle_sdp_hid_query_result, 1, BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST, des_buf,
                               des_len);
    EXPECT_EQ((int)sizeof(sample_hid_desc), d->hid_descriptor_len);
    EXPECT_EQ(0, memcmp(d->hid_descriptor, sample_hid_desc, sizeof(sample_hid_desc)));

    // 3. Adversarial > 512-byte (600-byte) SDP attribute value stream:
    //    Verify uni_handle_sdp_hid_query_result and uni_handle_sdp_pid_query_result reject
    //    oversized attribute_length (> MAX_ATTRIBUTE_VALUE_SIZE == 512) without buffer overflow,
    //    and verify uni_hid_device_set_hid_descriptor truncates > 512-byte descriptors to 512 bytes.
    uint8_t* oversized_stream = (uint8_t*)malloc(600);
    ASSERT_NE(NULL, oversized_stream);
    memset(oversized_stream, 0xaa, 600);

    stream_sdp_attribute_bytes(uni_handle_sdp_hid_query_result, 2, BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST,
                               oversized_stream, 600);
    stream_sdp_attribute_bytes(uni_handle_sdp_pid_query_result, 2, BLUETOOTH_ATTRIBUTE_VENDOR_ID, oversized_stream,
                               600);
    // Previous valid HID descriptor and VID must remain intact.
    EXPECT_EQ((int)sizeof(sample_hid_desc), d->hid_descriptor_len);
    EXPECT_EQ(0x054c, uni_hid_device_get_vendor_id(d));

    // Direct descriptor setter truncation check (> 512 bytes clamped to HID_MAX_DESCRIPTOR_LEN == 512).
    uni_hid_device_set_hid_descriptor(d, oversized_stream, 600);
    EXPECT_EQ(HID_MAX_DESCRIPTOR_LEN, d->hid_descriptor_len);
    free(oversized_stream);

    uni_handle_sdp_hid_query_result(HCI_EVENT_PACKET, 0, sdp_complete_pkt, sizeof(sdp_complete_pkt));
    EXPECT_EQ(UNI_BT_CONN_STATE_DEVICE_READY, uni_bt_conn_get_state(&d->conn));
}

// ============================================================================
// 6. TEST(bt_le_adv_report_64byte_name_overflow_regression)
// ============================================================================

/**
 * @brief Synthesize a BTstack `GAP_EVENT_ADVERTISING_REPORT` (0xE2) packet with Appearance and Complete Local Name AD
 * structures.
 */
static uint16_t build_le_adv_report_pkt(uint8_t* pkt,
                                        const bd_addr_t addr,
                                        uint8_t rssi,
                                        uint16_t appearance,
                                        const char* name_bytes,
                                        uint8_t name_len) {
    uint8_t ad_pos = 0;
    uint8_t* ad_data = &pkt[12];

    // AD Structure 1: Appearance (0x19)
    ad_data[ad_pos++] = 3;
    ad_data[ad_pos++] = BLUETOOTH_DATA_TYPE_APPEARANCE;
    little_endian_store_16(ad_data, ad_pos, appearance);
    ad_pos = (uint8_t)(ad_pos + 2);

    // AD Structure 2: Complete Local Name (0x09)
    if (name_len > 0 && name_bytes != NULL) {
        ad_data[ad_pos++] = (uint8_t)(1 + name_len);
        ad_data[ad_pos++] = BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME;
        memcpy(&ad_data[ad_pos], name_bytes, name_len);
        ad_pos = (uint8_t)(ad_pos + name_len);
    }

    pkt[0] = GAP_EVENT_ADVERTISING_REPORT;
    pkt[1] = (uint8_t)(10 + ad_pos);
    pkt[2] = 0x00;  // advertising_event_type
    pkt[3] = BD_ADDR_TYPE_LE_PUBLIC;
    put_bd_addr_reversed(&pkt[4], addr);
    pkt[10] = rssi;
    pkt[11] = ad_pos;
    return (uint16_t)(12 + ad_pos);
}

TEST(bt_le_adv_report_64byte_name_overflow_regression) {
    reset_test_fixture();
    uint8_t pkt[256];

    char oversized_name[100];
    memset(oversized_name, 'X', sizeof(oversized_name));

    // 1. Non-HID Appearance 0x0040 (Phone) with 100-byte name -> Clamped safely, then ignored by filter.
    bd_addr_t addr_phone = {0x55, 0x66, 0x77, 0x88, 0x99, 0x01};
    uint16_t pkt_len = build_le_adv_report_pkt(pkt, addr_phone, 210, 0x0040, oversized_name, 100);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, pkt, pkt_len);
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(addr_phone));

    // 2. Gamepad Appearance 0x03C4 (UNI_BT_HID_APPEARANCE_GAMEPAD) with 100-byte oversized name ->
    //    Verifies get_advertisement_data() clamps local name to 63 bytes + null terminator without
    //    overflowing `char name[64]` on the stack.
    bd_addr_t addr_ble_pad = {0x55, 0x66, 0x77, 0x88, 0x99, 0x02};
    pkt_len = build_le_adv_report_pkt(pkt, addr_ble_pad, 210, UNI_BT_HID_APPEARANCE_GAMEPAD, oversized_name, 100);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, pkt, pkt_len);

    uni_hid_device_t* d = uni_hid_device_get_instance_for_address(addr_ble_pad);
    ASSERT_NE(NULL, d);
    EXPECT_EQ(63, (int)strlen(d->name));
    EXPECT_EQ('\0', d->name[63]);
    for (int i = 0; i < 63; i++) {
        EXPECT_EQ('X', d->name[i]);
    }
    EXPECT_EQ(UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_GAMEPAD, d->cod);
    EXPECT_EQ(UNI_BT_CONN_PROTOCOL_BLE, d->conn.protocol);
}

// ============================================================================
// 7. TEST(bt_bredr_l2cap_data_packet_strips_header_and_routes)
// ============================================================================

TEST(bt_bredr_l2cap_data_packet_strips_header_and_routes) {
    reset_test_fixture();

    // Create a DualShock 3 device (0x054c:0x0268) with control_cid = 0x0040 and interrupt_cid = 0x0041.
    bd_addr_t addr = {0x66, 0x77, 0x88, 0x99, 0xaa, 0x01};
    uni_hid_device_t* d = uni_hid_device_create(addr);
    ASSERT_NE(NULL, d);
    d->conn.control_cid = 0x0040;
    d->conn.interrupt_cid = 0x0041;
    uni_hid_device_set_vendor_id(d, 0x054c);
    uni_hid_device_set_product_id(d, 0x0268);
    uni_hid_device_guess_controller_type_from_pid_vid(d);
    uni_hid_device_connect(d);
    EXPECT_TRUE(uni_hid_device_set_ready_complete(d));

    // 1. Short packet (< 2 bytes) and non-0xa1 header on interrupt_cid must be ignored.
    uint8_t short_pkt[] = {0xa1};
    uni_bt_packet_handler(L2CAP_DATA_PACKET, 0x0041, short_pkt, sizeof(short_pkt));
    EXPECT_EQ(0, g_controller_data_count);

    uint8_t bad_hdr_pkt[50];
    memset(bad_hdr_pkt, 0, sizeof(bad_hdr_pkt));
    bad_hdr_pkt[0] = 0xa2;  // Not 0xa1 (DATA | INPUT)
    bad_hdr_pkt[1] = 0x01;
    uni_bt_packet_handler(L2CAP_DATA_PACKET, 0x0041, bad_hdr_pkt, sizeof(bad_hdr_pkt));
    EXPECT_EQ(0, g_controller_data_count);

    // 2. Valid L2CAP_DATA_PACKET starting with 0xa1 followed by a 49-byte DualShock 3 report (0x01):
    //    pkt[0] = 0xa1 (stripped by uni_bt_bredr_on_l2cap_data_packet)
    //    report[0] (pkt[1]) = 0x01 (report_id)
    //    report[2] (pkt[3]) = 0x10 (D-pad Up)
    //    report[3] (pkt[4]) = 0x40 (Cross -> BUTTON_A)
    //    report[6] (pkt[7]) = 0xff (left stick X -> +511)
    //    report[7] (pkt[8]) = 0x80 (left stick Y -> 0)
    //    report[8] (pkt[9]) = 0x80 (right stick X -> 0)
    //    report[9] (pkt[10]) = 0x80 (right stick Y -> 0)
    //    report[19] (pkt[20]) = 0xff (R2 analog -> throttle 1023)
    //    report[30] (pkt[31]) = 0x05 (battery full)
    uint8_t l2cap_ds3_pkt[50];
    memset(l2cap_ds3_pkt, 0, sizeof(l2cap_ds3_pkt));
    l2cap_ds3_pkt[0] = 0xa1;
    l2cap_ds3_pkt[1] = 0x01;
    l2cap_ds3_pkt[3] = 0x10;
    l2cap_ds3_pkt[4] = 0x40;
    l2cap_ds3_pkt[7] = 0xff;
    l2cap_ds3_pkt[8] = 0x80;
    l2cap_ds3_pkt[9] = 0x80;
    l2cap_ds3_pkt[10] = 0x80;
    l2cap_ds3_pkt[20] = 0xff;
    l2cap_ds3_pkt[31] = 0x05;

    uni_bt_packet_handler(L2CAP_DATA_PACKET, 0x0041, l2cap_ds3_pkt, sizeof(l2cap_ds3_pkt));
    EXPECT_EQ(1, g_controller_data_count);
    EXPECT_EQ(DPAD_UP, d->controller.gamepad.dpad);
    EXPECT_TRUE((d->controller.gamepad.buttons & BUTTON_A) != 0);
    EXPECT_EQ(512, d->controller.gamepad.axis_x);
    EXPECT_EQ(1020, d->controller.gamepad.throttle);
}

// ============================================================================
// 8. TEST(bt_disconnect_cleans_up_device)
// ============================================================================

TEST(bt_disconnect_cleans_up_device) {
    reset_test_fixture();

    bd_addr_t addr = {0x77, 0x88, 0x99, 0xaa, 0xbb, 0x01};
    hci_con_handle_t handle = 0x0088;

    uni_hid_device_t* d = uni_hid_device_create(addr);
    ASSERT_NE(NULL, d);
    uni_hid_device_set_connection_handle(d, handle);
    uni_hid_device_connect(d);
    EXPECT_TRUE(uni_hid_device_set_ready_complete(d));

    ASSERT_EQ(d, uni_hid_device_get_instance_for_connection_handle(handle));
    ASSERT_EQ(d, uni_hid_device_get_instance_for_address(addr));
    EXPECT_EQ(1, g_connected_count);
    EXPECT_EQ(0, g_disconnected_count);

    // Synthesize HCI_EVENT_DISCONNECTION_COMPLETE (0x05) for `handle = 0x0088`.
    uint8_t disc_pkt[6];
    disc_pkt[0] = HCI_EVENT_DISCONNECTION_COMPLETE;
    disc_pkt[1] = 4;
    disc_pkt[2] = 0x00;  // status = ERROR_CODE_SUCCESS
    little_endian_store_16(disc_pkt, 3, handle);
    disc_pkt[5] = 0x13;  // reason = Remote User Terminated Connection

    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, disc_pkt, sizeof(disc_pkt));

    // Verify device slot was disconnected, notified to platform, and deleted.
    EXPECT_EQ(1, g_disconnected_count);
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_connection_handle(handle));
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(addr));
}

// ============================================================================
// 9. TEST(bt_sdp_query_abort_on_disconnect_and_failure_b5)
// ============================================================================

TEST(bt_sdp_query_abort_on_disconnect_and_failure_b5) {
    reset_test_fixture();
    sdp_client_deinit();
    l2cap_init();

    // Case 1: Mid-SDP device disconnect clears sdp_device and removes sdp_query_timer.
    bd_addr_t addr1 = {0x88, 0x99, 0xaa, 0xbb, 0xcc, 0x01};
    uni_hid_device_t* d1 = uni_hid_device_create(addr1);
    ASSERT_NE(NULL, d1);
    uni_hid_device_set_connection_handle(d1, 0x0091);

    uni_bt_sdp_query_start(d1);
    // sdp_query_timer is now armed in btstack_run_loop_base_timers and sdp_device == d1.
    EXPECT_NE(NULL, btstack_run_loop_base_timers);
    EXPECT_EQ(UNI_BT_CONN_STATE_SDP_VENDOR_REQUESTED, uni_bt_conn_get_state(&d1->conn));

    // Case 2: Concurrent second device rejection does NOT abort d1's active SDP query.
    bd_addr_t addr_concurrent = {0x88, 0x99, 0xaa, 0xbb, 0xcc, 0x02};
    uni_hid_device_t* d_concurrent = uni_hid_device_create(addr_concurrent);
    ASSERT_NE(NULL, d_concurrent);
    uni_bt_sdp_query_start(d_concurrent);
    // d_concurrent was rejected and deleted, but d1's SDP query and timer remain active!
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(addr_concurrent));
    EXPECT_EQ(d1, uni_hid_device_get_instance_for_address(addr1));
    EXPECT_NE(NULL, btstack_run_loop_base_timers);

    // Verify d1 is still the active sdp_device by streaming a VID attribute to it.
    const uint8_t de_vid_sony[] = {0x09, 0x05, 0x4c};
    stream_sdp_attribute_bytes(uni_handle_sdp_pid_query_result, 1, BLUETOOTH_ATTRIBUTE_VENDOR_ID, de_vid_sony,
                               sizeof(de_vid_sony));
    EXPECT_EQ(0x054c, uni_hid_device_get_vendor_id(d1));

    // Now disconnect d1 mid-SDP via HCI_EVENT_DISCONNECTION_COMPLETE:
    // uni_hid_device_delete(d1) must invoke uni_bt_sdp_query_abort(d1), disarming sdp_query_timer
    // and clearing sdp_device = NULL.
    uint8_t disc_pkt[6] = {HCI_EVENT_DISCONNECTION_COMPLETE, 4, 0x00, 0x91, 0x00, 0x13};
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, disc_pkt, sizeof(disc_pkt));
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(addr1));
    EXPECT_EQ(NULL, btstack_run_loop_base_timers);

    // Case 3: Early sdp_client_query_uuid16() failure (SDP_QUERY_BUSY != 0) in
    // uni_bt_sdp_query_start_vid_pid() and uni_bt_sdp_query_start_hid_descriptor().
    // Note: sdp_client is still in W4_CONNECT from d1's query above, so sdp_client_ready() is false!
    EXPECT_FALSE(sdp_client_ready());

    bd_addr_t addr_busy_vid = {0x88, 0x99, 0xaa, 0xbb, 0xcc, 0x03};
    uni_hid_device_t* d_busy_vid = uni_hid_device_create(addr_busy_vid);
    ASSERT_NE(NULL, d_busy_vid);
    // uni_bt_sdp_query_start() arms sdp_query_timer, sets sdp_device = d_busy_vid, and calls
    // uni_bt_sdp_query_start_vid_pid(), which gets SDP_QUERY_BUSY and must abort & delete d_busy_vid.
    uni_bt_sdp_query_start(d_busy_vid);
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(addr_busy_vid));
    EXPECT_EQ(NULL, btstack_run_loop_base_timers);

    // Also test uni_bt_sdp_query_start_hid_descriptor() when sdp_client_query_uuid16() returns SDP_QUERY_BUSY:
    bd_addr_t addr_busy_hid = {0x88, 0x99, 0xaa, 0xbb, 0xcc, 0x04};
    uni_hid_device_t* d_busy_hid = uni_hid_device_create(addr_busy_hid);
    ASSERT_NE(NULL, d_busy_hid);
    uni_hid_device_guess_controller_type_from_pid_vid(
        d_busy_hid);  // Populates generic parse_usage -> requires HID descriptor
    EXPECT_TRUE(uni_hid_device_does_require_hid_descriptor(d_busy_hid));
    uni_bt_sdp_set_device_for_test(d_busy_hid);
    uni_bt_sdp_query_start_hid_descriptor(d_busy_hid);
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(addr_busy_hid));
    EXPECT_EQ(NULL, btstack_run_loop_base_timers);

    // Reset BTstack SDP client state and verify a new device d2 can start an SDP query cleanly
    // without being blocked by a stale sdp_device pointer.
    sdp_client_deinit();
    l2cap_init();
    bd_addr_t addr2 = {0x88, 0x99, 0xaa, 0xbb, 0xcc, 0x05};
    uni_hid_device_t* d2 = uni_hid_device_create(addr2);
    ASSERT_NE(NULL, d2);
    uni_bt_sdp_query_start(d2);
    EXPECT_EQ(d2, uni_hid_device_get_instance_for_address(addr2));
    EXPECT_NE(NULL, btstack_run_loop_base_timers);

    uni_hid_device_delete(d2);
    EXPECT_EQ(NULL, btstack_run_loop_base_timers);
    sdp_client_deinit();
    l2cap_init();
}

// ============================================================================
// 10. TEST(bt_le_pnp_id_att_error_status_guard_b6)
// ============================================================================

TEST(bt_le_pnp_id_att_error_status_guard_b6) {
    reset_test_fixture();

    bd_addr_t addr = {0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0x01};
    hci_con_handle_t con_handle = 0x0045;
    uni_hid_device_t* d = uni_hid_device_create(addr);
    ASSERT_NE(NULL, d);
    uni_hid_device_set_connection_handle(d, con_handle);
    ASSERT_EQ(0, uni_hid_device_get_vendor_id(d));
    ASSERT_EQ(0, uni_hid_device_get_product_id(d));

    // Construct a 13-byte GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_PNP_ID (0x08) packet:
    //   [0] = HCI_EVENT_GATTSERVICE_META (0xEA)
    //   [1] = 11 (payload length)
    //   [2] = GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_PNP_ID (0x08)
    //   [3..4] = con_handle (little-endian)
    //   [5] = att_status
    //   [6] = vendor_source_id
    //   [7..8] = vendor_id (little-endian)
    //   [9..10] = product_id (little-endian)
    //   [11..12] = product_version (little-endian)
    uint8_t pnp_pkt[13];
    memset(pnp_pkt, 0, sizeof(pnp_pkt));
    pnp_pkt[0] = HCI_EVENT_GATTSERVICE_META;
    pnp_pkt[1] = sizeof(pnp_pkt) - 2;
    pnp_pkt[2] = GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_PNP_ID;
    little_endian_store_16(pnp_pkt, 3, con_handle);
    pnp_pkt[5] = ATT_ERROR_ATTRIBUTE_NOT_FOUND;  // 0x0A != ATT_ERROR_SUCCESS
    pnp_pkt[6] = 0x02;                           // USB Implementers Forum
    little_endian_store_16(pnp_pkt, 7, 0xDEAD);  // Garbage vendor_id
    little_endian_store_16(pnp_pkt, 9, 0xBEEF);  // Garbage product_id
    little_endian_store_16(pnp_pkt, 11, 0x0100);

    // 1. Dispatch with non-zero ATT error status: vendor_id and product_id MUST remain 0.
    uni_bt_le_on_hci_event_gattservice_meta(pnp_pkt, sizeof(pnp_pkt));
    EXPECT_EQ(0, uni_hid_device_get_vendor_id(d));
    EXPECT_EQ(0, uni_hid_device_get_product_id(d));

    // 2. Dispatch with ATT_ERROR_SUCCESS (0x00) and valid VID=0x045e, PID=0x0b13:
    pnp_pkt[5] = ATT_ERROR_SUCCESS;
    little_endian_store_16(pnp_pkt, 7, 0x045e);
    little_endian_store_16(pnp_pkt, 9, 0x0b13);
    uni_bt_le_on_hci_event_gattservice_meta(pnp_pkt, sizeof(pnp_pkt));
    EXPECT_EQ(0x045e, uni_hid_device_get_vendor_id(d));
    EXPECT_EQ(0x0b13, uni_hid_device_get_product_id(d));
}

// ============================================================================
// 11. TEST(bt_le_setup_legacy_pairing_steam_controller_regression)
// ============================================================================

static void pump_btstack_crypto_and_sm(void) {
    // Pump any pending HCI_OPCODE_HCI_LE_RAND (0x2018) or HCI_OPCODE_HCI_LE_ENCRYPT (0x2017)
    // commands emitted by btstack_crypto / SM, and advance SM's state machine via its 0ms timer.
    for (int step = 0; step < 32; step++) {
        replenish_hci_cmd_credits();
        btstack_run_loop_base_process_timers(btstack_run_loop_get_time_ms() + 1);
        if (g_pending_crypto_opcode == 0x2018) {
            // HCI_OPCODE_HCI_LE_RAND -> Command Complete with 8 random bytes at offset 6
            g_pending_crypto_opcode = 0;
            uint8_t cc_rand[14] = {
                HCI_EVENT_COMMAND_COMPLETE,
                12,
                1,
                0x18,
                0x20,
                ERROR_CODE_SUCCESS,
                (uint8_t)(0x11 + step),
                0x22,
                0x33,
                0x44,
                0x55,
                0x66,
                0x77,
                0x88,
            };
            g_transport_packet_handler(HCI_EVENT_PACKET, cc_rand, sizeof(cc_rand));
        } else if (g_pending_crypto_opcode == 0x2017) {
            // HCI_OPCODE_HCI_LE_ENCRYPT -> Command Complete with 16 encrypted bytes at offset 6
            g_pending_crypto_opcode = 0;
            uint8_t cc_enc[22] = {
                HCI_EVENT_COMMAND_COMPLETE,
                20,
                1,
                0x17,
                0x20,
                ERROR_CODE_SUCCESS,
                0xaa,
                0xbb,
                0xcc,
                0xdd,
                0xee,
                0xff,
                0x00,
                0x11,
                0x22,
                0x33,
                0x44,
                0x55,
                0x66,
                0x77,
                0x88,
                0x99,
            };
            g_transport_packet_handler(HCI_EVENT_PACKET, cc_enc, sizeof(cc_enc));
        } else {
            btstack_run_loop_base_process_timers(btstack_run_loop_get_time_ms() + 1);
            if (g_pending_crypto_opcode == 0) {
                break;
            }
        }
    }
}

TEST(bt_le_setup_legacy_pairing_steam_controller_regression) {
    reset_test_fixture();
    ASSERT_NE(NULL, g_transport_packet_handler);

    // Configure LE Device DB TLV with the active Posix TLV instance so SM identity resolution can query it.
    const btstack_tlv_t* tlv_impl = NULL;
    void* tlv_context_ptr = NULL;
    btstack_tlv_get_instance(&tlv_impl, &tlv_context_ptr);
    ASSERT_NE(NULL, tlv_impl);
    le_device_db_tlv_configure(tlv_impl, tlv_context_ptr);

    // Run uni_bt_le_setup() to configure SM (`sm_set_secure_connections_only_mode(false)`,
    // `sm_set_encryption_key_size_range(7, 16)`, `sm_set_authentication_requirements(SM_AUTHREQ_BONDING)`).
    uni_bt_le_setup();

    // Reset both btstack_crypto and SM internal state by emitting HCI_STATE_HALTING,
    // then transition to HCI_STATE_WORKING and pump the initial ER/IR/DKG/ECC key generation.
    hci_stack_t* stack = hci_get_stack();
    ASSERT_NE(NULL, stack);
    stack->state = HCI_STATE_HALTING;
    hci_emit_state();

    stack->state = HCI_STATE_WORKING;
    stack->acl_packets_total_num = 16;
    stack->le_acl_packets_total_num = 16;
    stack->acl_data_packet_length = 1024;
    stack->le_data_packets_length = 251;
    replenish_hci_cmd_credits();
    g_last_hci_cmd_opcode = 0;
    g_pending_crypto_opcode = 0;
    hci_emit_state();
    pump_btstack_crypto_and_sm();

    // Create a BLE device representing the 2015 Steam Controller (random address C1:92:66:E9:B3:6A).
    bd_addr_t steam_addr = {0xC1, 0x92, 0x66, 0xE9, 0xB3, 0x6A};
    hci_con_handle_t con_handle = 0x004c;
    uni_hid_device_t* d = uni_hid_device_create(steam_addr);
    ASSERT_NE(NULL, d);
    uni_bt_conn_set_protocol(&d->conn, UNI_BT_CONN_PROTOCOL_BLE);

    // 1. Inject HCI_EVENT_LE_META / HCI_SUBEVENT_LE_CONNECTION_COMPLETE (21 bytes) into the HCI transport.
    //    hci.c creates the connection, emits GAP_SUBEVENT_LE_CONNECTION_COMPLETE to SM, and emits
    //    HCI_SUBEVENT_LE_CONNECTION_COMPLETE to uni_bt_le_on_hci_event_le_meta(), which calls
    //    sm_request_pairing(0x004c).
    memset(g_last_acl_buf, 0, sizeof(g_last_acl_buf));
    g_last_acl_size = 0;
    g_last_hci_cmd_opcode = 0;

    uint8_t le_conn_evt[21];
    memset(le_conn_evt, 0, sizeof(le_conn_evt));
    le_conn_evt[0] = HCI_EVENT_LE_META;
    le_conn_evt[1] = 19;
    le_conn_evt[2] = HCI_SUBEVENT_LE_CONNECTION_COMPLETE;
    le_conn_evt[3] = ERROR_CODE_SUCCESS;
    little_endian_store_16(le_conn_evt, 4, con_handle);
    le_conn_evt[6] = HCI_ROLE_MASTER;
    le_conn_evt[7] = BD_ADDR_TYPE_LE_RANDOM;
    put_bd_addr_reversed(&le_conn_evt[8], steam_addr);
    little_endian_store_16(le_conn_evt, 14, 0x0018);  // conn_interval
    little_endian_store_16(le_conn_evt, 16, 0x0000);  // conn_latency
    little_endian_store_16(le_conn_evt, 18, 0x0048);  // supervision_timeout
    le_conn_evt[20] = 0x05;                           // master_clock_accuracy

    g_transport_packet_handler(HCI_EVENT_PACKET, le_conn_evt, sizeof(le_conn_evt));
    EXPECT_EQ(con_handle, d->conn.handle);
    pump_btstack_crypto_and_sm();

    // Verify that SM sent an SMP Pairing Request (0x01) on L2CAP CID 0x0006 with
    // AuthReq == SM_AUTHREQ_BONDING (0x01), NOT forced to 0x29 by sm_sc_only_mode!
    ASSERT_EQ(15, g_last_acl_size);
    EXPECT_EQ(0x0006, little_endian_read_16(g_last_acl_buf, 6));  // L2CAP CID = SMP
    EXPECT_EQ(0x01, g_last_acl_buf[8]);                           // Opcode = Pairing Request (0x01)
    EXPECT_EQ(IO_CAPABILITY_NO_INPUT_NO_OUTPUT, g_last_acl_buf[9]);
    EXPECT_EQ(SM_AUTHREQ_BONDING, g_last_acl_buf[11]);  // AuthReq = Bonding (0x01), SC bit NOT forced
    EXPECT_EQ(16, g_last_acl_buf[12]);                  // Max Encryption Key Size = 16

    // Clear the ACL packet slot counter on the connection so SM can transmit the next PDU if needed.
    hci_connection_t* hci_con = hci_connection_for_handle(con_handle);
    ASSERT_NE(NULL, hci_con);
    hci_con->num_packets_sent = 0;
    memset(g_last_acl_buf, 0, sizeof(g_last_acl_buf));
    g_last_acl_size = 0;
    g_last_hci_cmd_opcode = 0;

    // 2. Inject the exact Steam Controller (2015) SMP Pairing Response (Frame 1370 from hci_dump.pklg):
    //    Opcode = 0x02 (Pairing Response), IO = 0x03 (NoInputNoOutput), OOB = 0x00,
    //    AuthReq = 0x01 (Bonding only, Secure Connections = 0 -> LE Legacy Pairing),
    //    MaxKeySize = 16 (0x10), InitiatorKeyDist = 0x02 (IRK), ResponderKeyDist = 0x03 (LTK | IRK).
    uint8_t steam_pairing_rsp_acl[15] = {
        0x4c, 0x20,  // Handle 0x004c | PB=10
        0x0b, 0x00,  // ACL length = 11
        0x07, 0x00,  // L2CAP length = 7
        0x06, 0x00,  // L2CAP CID = 0x0006 (SMP)
        0x02,        // SMP Opcode: Pairing Response (0x02)
        0x03,        // IO Capability: No Input, No Output (0x03)
        0x00,        // OOB Data Flag: Not Present (0x00)
        0x01,        // AuthReq: Bonding (0x01) - LE Legacy Pairing (SC = 0)
        0x10,        // Max Encryption Key Size: 16
        0x02,        // Initiator Key Distribution: IRK (0x02)
        0x03,        // Responder Key Distribution: LTK | IRK (0x03)
    };
    g_transport_packet_handler(HCI_ACL_DATA_PACKET, steam_pairing_rsp_acl, sizeof(steam_pairing_rsp_acl));
    pump_btstack_crypto_and_sm();

    // Verify that SM did NOT reject the Steam Controller with SMP Pairing Failed (0x05) /
    // SM_REASON_AUTHENTHICATION_REQUIREMENTS (0x03), and instead proceeded to Phase 2 Legacy Pairing
    // by sending SMP Pairing Confirm (0x03, 17-byte SMP PDU = 25-byte ACL frame)!
    ASSERT_EQ(25, g_last_acl_size);
    EXPECT_EQ(0x0006, little_endian_read_16(g_last_acl_buf, 6));  // L2CAP CID = SMP
    EXPECT_EQ(0x03, g_last_acl_buf[8]);                           // Opcode = Pairing Confirm (0x03)

    // 3. Exercise the Steam Controller GATT setup state machine (`uni_hid_parser_steam_setup` ->
    //    `uni_steam_handle_gatt_client_event`) on non-zero `con_handle = 0x004c`.
    //    Regression test for `uni_steam_handle_gatt_client_event()` previously using `channel`
    //    (which `gatt_client.c:emit_event_new()` always passes as `0`) instead of extracting
    //    `con_handle` from the GATT event payload (`gatt_event_*_get_handle(packet)`).
    hci_con->num_packets_sent = 0;
    memset(g_last_acl_buf, 0, sizeof(g_last_acl_buf));
    g_last_acl_size = 0;

    uni_hid_device_set_vendor_id(d, 0x28de);
    uni_hid_device_set_product_id(d, 0x1106);
    uni_hid_device_guess_controller_type_from_pid_vid(d);
    EXPECT_EQ(CONTROLLER_TYPE_SteamController, d->controller_type);
    uni_hid_device_connect(d);
    uni_hid_device_set_ready(d);
    EXPECT_EQ(UNI_BT_CONN_STATE_DEVICE_PENDING_READY, uni_bt_conn_get_state(&d->conn));

    // Step 3a: On the first GATT query, `gatt_client` sends ATT_EXCHANGE_MTU_REQUEST (0x02, 11 bytes);
    // reply with ATT_EXCHANGE_MTU_RESPONSE (0x03, MTU = 23) so `gatt_client` proceeds to send
    // ATT_FIND_BY_TYPE_VALUE_REQUEST (0x06, 31 bytes) for the 128-bit Steam service UUID.
    ASSERT_EQ(11, g_last_acl_size);
    EXPECT_EQ(0x0004, little_endian_read_16(g_last_acl_buf, 6));  // L2CAP CID = ATT
    EXPECT_EQ(0x02, g_last_acl_buf[8]);                           // ATT_EXCHANGE_MTU_REQUEST

    hci_con->num_packets_sent = 0;
    memset(g_last_acl_buf, 0, sizeof(g_last_acl_buf));
    g_last_acl_size = 0;
    uint8_t att_mtu_rsp[11] = {
        0x4c, 0x20,  // Handle 0x004c | PB=10
        0x07, 0x00,  // ACL length = 7
        0x03, 0x00,  // L2CAP length = 3
        0x04, 0x00,  // L2CAP CID = 0x0004 (ATT)
        0x03,        // ATT_EXCHANGE_MTU_RESPONSE (0x03)
        0x17, 0x00,  // Server Rx MTU = 23
    };
    g_transport_packet_handler(HCI_ACL_DATA_PACKET, att_mtu_rsp, sizeof(att_mtu_rsp));

    ASSERT_EQ(31, g_last_acl_size);
    EXPECT_EQ(0x0004, little_endian_read_16(g_last_acl_buf, 6));  // L2CAP CID = ATT
    EXPECT_EQ(0x06, g_last_acl_buf[8]);                           // ATT_FIND_BY_TYPE_VALUE_REQUEST

    // Inject Frame 630 from hci_dump.pklg: ATT_FIND_BY_TYPE_VALUE_RESPONSE (0x07) with
    // service handle range 0x0027..0xffff on con_handle = 0x004c.
    hci_con->num_packets_sent = 0;
    memset(g_last_acl_buf, 0, sizeof(g_last_acl_buf));
    g_last_acl_size = 0;
    uint8_t att_find_by_type_rsp[13] = {
        0x4c, 0x20,  // Handle 0x004c | PB=10
        0x09, 0x00,  // ACL length = 9
        0x05, 0x00,  // L2CAP length = 5
        0x04, 0x00,  // L2CAP CID = 0x0004 (ATT)
        0x07,        // ATT_FIND_BY_TYPE_VALUE_RESPONSE (0x07)
        0x27, 0x00,  // Found Attribute Handle = 0x0027
        0xff, 0xff,  // Group End Handle = 0xffff
    };
    g_transport_packet_handler(HCI_ACL_DATA_PACKET, att_find_by_type_rsp, sizeof(att_find_by_type_rsp));

    // Step 3b: Verify `uni_steam_handle_gatt_client_event()` resolved `con_handle = 0x004c`
    // (NOT `channel = 0`) and issued ATT_READ_BY_TYPE_REQUEST (0x08) for characteristic discovery
    // in range 0x0027..0xffff.
    ASSERT_EQ(15, g_last_acl_size);
    EXPECT_EQ(0x0004, little_endian_read_16(g_last_acl_buf, 6));  // L2CAP CID = ATT
    EXPECT_EQ(0x08, g_last_acl_buf[8]);                           // ATT_READ_BY_TYPE_REQUEST
    EXPECT_EQ(0x0027, little_endian_read_16(g_last_acl_buf, 9));
    EXPECT_EQ(0xffff, little_endian_read_16(g_last_acl_buf, 11));

    // Inject ATT_READ_BY_TYPE_RESPONSE (0x09) reporting the Steam report characteristic
    // (100F6C34-1735-4313-B402-38567131E5F3) at declaration handle 0x002a, properties 0x0a,
    // value handle 0x002b.
    hci_con->num_packets_sent = 0;
    memset(g_last_acl_buf, 0, sizeof(g_last_acl_buf));
    g_last_acl_size = 0;
    uint8_t att_read_by_type_rsp[31] = {
        0x4c,
        0x20,  // Handle 0x004c | PB=10
        0x1b,
        0x00,  // ACL length = 27
        0x17,
        0x00,  // L2CAP length = 23
        0x04,
        0x00,  // L2CAP CID = 0x0004 (ATT)
        0x09,  // ATT_READ_BY_TYPE_RESPONSE (0x09)
        21,    // Length of each attribute handle-value pair (2 + 1 + 2 + 16 = 21)
        0x2a,
        0x00,  // Characteristic declaration handle = 0x002a
        0x0a,  // Properties = Read | Write
        0x2b,
        0x00,  // Characteristic value handle = 0x002b
        // 128-bit UUID 100F6C34-1735-4313-B402-38567131E5F3 in little-endian ATT wire order:
        0xf3,
        0xe5,
        0x31,
        0x71,
        0x56,
        0x38,
        0x02,
        0xb4,
        0x13,
        0x43,
        0x35,
        0x17,
        0x34,
        0x6c,
        0x0f,
        0x10,
    };
    g_transport_packet_handler(HCI_ACL_DATA_PACKET, att_read_by_type_rsp, sizeof(att_read_by_type_rsp));

    // gatt_client sends another ATT_READ_BY_TYPE_REQUEST starting at 0x002c; complete it with
    // ATT_ERROR_RESPONSE (ATT_ERROR_ATTRIBUTE_NOT_FOUND = 0x0a).
    EXPECT_EQ(0x08, g_last_acl_buf[8]);
    hci_con->num_packets_sent = 0;
    memset(g_last_acl_buf, 0, sizeof(g_last_acl_buf));
    g_last_acl_size = 0;
    uint8_t att_err_not_found[13] = {
        0x4c, 0x20,  // Handle 0x004c | PB=10
        0x09, 0x00,  // ACL length = 9
        0x05, 0x00,  // L2CAP length = 5
        0x04, 0x00,  // L2CAP CID = 0x0004 (ATT)
        0x01,        // ATT_ERROR_RESPONSE (0x01)
        0x08,        // Request Opcode In Error = ATT_READ_BY_TYPE_REQUEST (0x08)
        0x2c, 0x00,  // Handle In Error = 0x002c
        0x0a,        // Error Code = ATT_ERROR_ATTRIBUTE_NOT_FOUND (0x0a)
    };
    g_transport_packet_handler(HCI_ACL_DATA_PACKET, att_err_not_found, sizeof(att_err_not_found));

    // Step 3c: Verify `uni_steam_handle_gatt_client_event()` wrote `cmd_clear_mappings` (0xc0, 0x81, 0x01)
    // via ATT_WRITE_REQUEST (0x12) to characteristic value handle 0x002b on `con_handle = 0x004c`.
    ASSERT_EQ(14, g_last_acl_size);
    EXPECT_EQ(0x0004, little_endian_read_16(g_last_acl_buf, 6));  // L2CAP CID = ATT
    EXPECT_EQ(0x12, g_last_acl_buf[8]);                           // ATT_WRITE_REQUEST
    EXPECT_EQ(0x002b, little_endian_read_16(g_last_acl_buf, 9));  // Value Handle = 0x002b
    EXPECT_EQ(0xc0, g_last_acl_buf[11]);
    EXPECT_EQ(0x81, g_last_acl_buf[12]);  // STEAM_CMD_CLEAR_MAPPINGS
    EXPECT_EQ(0x01, g_last_acl_buf[13]);

    // Inject ATT_WRITE_RESPONSE (0x13) to complete `STATE_QUERY_CLEAR_MAPPINGS`.
    hci_con->num_packets_sent = 0;
    memset(g_last_acl_buf, 0, sizeof(g_last_acl_buf));
    g_last_acl_size = 0;
    uint8_t att_write_rsp[9] = {
        0x4c, 0x20,  // Handle 0x004c | PB=10
        0x05, 0x00,  // ACL length = 5
        0x01, 0x00,  // L2CAP length = 1
        0x04, 0x00,  // L2CAP CID = 0x0004 (ATT)
        0x13,        // ATT_WRITE_RESPONSE (0x13)
    };
    g_transport_packet_handler(HCI_ACL_DATA_PACKET, att_write_rsp, sizeof(att_write_rsp));

    // Step 3d: Verify `uni_steam_handle_gatt_client_event()` wrote `cmd_disable_lizard` (18 bytes)
    // via ATT_WRITE_REQUEST (0x12) to characteristic value handle 0x002b on `con_handle = 0x004c`.
    ASSERT_EQ(29, g_last_acl_size);
    EXPECT_EQ(0x0004, little_endian_read_16(g_last_acl_buf, 6));  // L2CAP CID = ATT
    EXPECT_EQ(0x12, g_last_acl_buf[8]);                           // ATT_WRITE_REQUEST
    EXPECT_EQ(0x002b, little_endian_read_16(g_last_acl_buf, 9));  // Value Handle = 0x002b
    EXPECT_EQ(0xc0, g_last_acl_buf[11]);
    EXPECT_EQ(0x87, g_last_acl_buf[12]);  // STEAM_CMD_WRITE_REGISTER
    EXPECT_EQ(0x0f, g_last_acl_buf[13]);

    // Inject second ATT_WRITE_RESPONSE (0x13) to complete `STATE_QUERY_DISABLE_LIZARD` and verify
    // `uni_hid_device_set_ready_complete(d)` transitions the Steam Controller to `DEVICE_READY`.
    hci_con->num_packets_sent = 0;
    g_transport_packet_handler(HCI_ACL_DATA_PACKET, att_write_rsp, sizeof(att_write_rsp));
    EXPECT_EQ(UNI_BT_CONN_STATE_DEVICE_READY, uni_bt_conn_get_state(&d->conn));
    EXPECT_EQ(1, g_ready_count);

    // Clean up connection via HCI_EVENT_DISCONNECTION_COMPLETE.
    uint8_t disc_evt[6] = {HCI_EVENT_DISCONNECTION_COMPLETE, 4, 0x00, 0x4c, 0x00, 0x08};
    g_transport_packet_handler(HCI_EVENT_PACKET, disc_evt, sizeof(disc_evt));
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(steam_addr));
}

// ============================================================================
// 12. TEST(bredr_l2cap_data_packet_bounds_and_orphan_channel)
// ============================================================================

static int g_mock_input_report_count = 0;
static int g_mock_feature_report_count = 0;
static uint8_t g_mock_last_report_id = 0;
static uint16_t g_mock_last_report_len = 0;

static void mock_parse_input_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    ARG_UNUSED(d);
    g_mock_input_report_count++;
    if (report && len > 0) {
        g_mock_last_report_id = report[0];
    }
    g_mock_last_report_len = len;
}

static void mock_parse_feature_report(struct uni_hid_device_s* d, const uint8_t* report, uint16_t len) {
    ARG_UNUSED(d);
    g_mock_feature_report_count++;
    if (report && len > 0) {
        g_mock_last_report_id = report[0];
    }
    g_mock_last_report_len = len;
}

TEST(bredr_l2cap_data_packet_bounds_and_orphan_channel) {
    reset_test_fixture();
    g_mock_input_report_count = 0;
    g_mock_feature_report_count = 0;
    g_mock_last_report_id = 0;
    g_mock_last_report_len = 0;

    bd_addr_t addr = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0x01};
    uni_hid_device_t* d = uni_hid_device_create(addr);
    ASSERT_NE(NULL, d);
    d->conn.control_cid = 0x0040;
    d->conn.interrupt_cid = 0x0041;
    d->report_parser.parse_input_report = mock_parse_input_report;
    d->report_parser.parse_feature_report = mock_parse_feature_report;
    uni_hid_device_connect(d);
    EXPECT_TRUE(uni_hid_device_set_ready_complete(d));

    // 1. Orphan channel (no matching uni_hid_device_t for CID 0x9999) must be ignored safely.
    uint8_t valid_pkt[3] = {0xa1, 0x01, 0x55};
    uni_bt_bredr_on_l2cap_data_packet(0x9999, valid_pkt, sizeof(valid_pkt));
    EXPECT_EQ(0, g_mock_input_report_count);
    EXPECT_EQ(0, g_mock_feature_report_count);

    // 2. 0-byte and exact 1-byte heap-allocated packets (size < 2) on interrupt_cid and control_cid
    //    must be rejected before reading packet[1] under ASan.
    uint8_t* one_byte_pkt = (uint8_t*)malloc(1);
    ASSERT_NE(NULL, one_byte_pkt);
    one_byte_pkt[0] = 0xa1;
    uni_bt_bredr_on_l2cap_data_packet(0x0041, one_byte_pkt, 0);
    uni_bt_bredr_on_l2cap_data_packet(0x0041, one_byte_pkt, 1);
    one_byte_pkt[0] = 0xa3;
    uni_bt_bredr_on_l2cap_data_packet(0x0040, one_byte_pkt, 0);
    uni_bt_bredr_on_l2cap_data_packet(0x0040, one_byte_pkt, 1);
    free(one_byte_pkt);
    EXPECT_EQ(0, g_mock_input_report_count);
    EXPECT_EQ(0, g_mock_feature_report_count);

    // 3. Valid 3-byte HID INPUT report {0xa1, 0x01, 0x55} on interrupt_cid strips 0xa1 and routes 2 bytes.
    uni_bt_bredr_on_l2cap_data_packet(0x0041, valid_pkt, sizeof(valid_pkt));
    EXPECT_EQ(1, g_mock_input_report_count);
    EXPECT_EQ(0x01, g_mock_last_report_id);
    EXPECT_EQ(2, g_mock_last_report_len);

    // 4. Valid 3-byte HID FEATURE report {0xa3, 0x02, 0x77} on control_cid strips 0xa3 and routes 2 bytes.
    uint8_t valid_feat_pkt[3] = {0xa3, 0x02, 0x77};
    uni_bt_bredr_on_l2cap_data_packet(0x0040, valid_feat_pkt, sizeof(valid_feat_pkt));
    EXPECT_EQ(1, g_mock_feature_report_count);
    EXPECT_EQ(0x02, g_mock_last_report_id);
    EXPECT_EQ(2, g_mock_last_report_len);
}

// ============================================================================
// 13. TEST(le_hogp_truncated_packet_and_discovery_bounds)
// ============================================================================

TEST(le_hogp_truncated_packet_and_discovery_bounds) {
    reset_test_fixture();
    g_mock_input_report_count = 0;

    // 1. GAP_EVENT_ADVERTISING_REPORT with size = 11 (< 12 minimum header size):
    //    Allocate exact 11-byte buffer so ASan traps any OOB read of packet[11] (data_length).
    uint8_t* short_adv = (uint8_t*)malloc(11);
    ASSERT_NE(NULL, short_adv);
    memset(short_adv, 0, 11);
    short_adv[0] = GAP_EVENT_ADVERTISING_REPORT;
    short_adv[1] = 9;
    uni_bt_le_on_gap_event_advertising_report(short_adv, 11);
    free(short_adv);
    EXPECT_EQ(0, g_discovered_count);

    // 2. GAP_EVENT_ADVERTISING_REPORT with mismatched data_length (packet[11] = 40, but packet size = 15):
    //    Allocate exact 15-byte buffer so ASan traps any read past size - 12 (3 bytes of AD payload).
    uint8_t* mismatch_adv = (uint8_t*)malloc(15);
    ASSERT_NE(NULL, mismatch_adv);
    memset(mismatch_adv, 0, 15);
    mismatch_adv[0] = GAP_EVENT_ADVERTISING_REPORT;
    mismatch_adv[1] = 13;
    mismatch_adv[11] = 40;  // Claims 40 bytes of AD data, but only 3 bytes follow
    mismatch_adv[12] = 2;
    mismatch_adv[13] = BLUETOOTH_DATA_TYPE_FLAGS;
    mismatch_adv[14] = 0x06;
    uni_bt_le_on_gap_event_advertising_report(mismatch_adv, 15);
    free(mismatch_adv);
    EXPECT_EQ(0, g_discovered_count);

    // 3. GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_DONE with orphan con_handle (0x0bad):
    uint8_t dis_done_orphan[6] = {
        HCI_EVENT_GATTSERVICE_META, 4, GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_DONE, 0xad, 0x0b, ERROR_CODE_SUCCESS,
    };
    uni_bt_le_on_hci_event_gattservice_meta(dis_done_orphan, sizeof(dis_done_orphan));

    // 4. GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED with orphan hids_cid (0x0bad):
    uint8_t hid_conn_orphan[8] = {
        HCI_EVENT_GATTSERVICE_META,
        6,
        GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED,
        0xad,
        0x0b,  // hids_cid = 0x0bad
        ERROR_CODE_SUCCESS,
        HID_PROTOCOL_MODE_REPORT,
        1,
    };
    uni_bt_le_on_hci_event_gattservice_meta(hid_conn_orphan, sizeof(hid_conn_orphan));

    // 5. GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED with non-zero status (0x0c) on valid device:
    bd_addr_t ble_addr = {0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0x01};
    uni_hid_device_t* d = uni_hid_device_create(ble_addr);
    ASSERT_NE(NULL, d);
    d->hids_cid = 0x0022;
    d->conn.handle = HCI_CON_HANDLE_INVALID;
    d->report_parser.parse_input_report = mock_parse_input_report;
    // Set non-zero hid_descriptor_len so parse_report does not query hids_host descriptor storage.
    uint8_t dummy_desc[4] = {0x05, 0x01, 0x09, 0x05};
    uni_hid_device_set_hid_descriptor(d, dummy_desc, sizeof(dummy_desc));

    uint8_t hid_conn_fail[8] = {
        HCI_EVENT_GATTSERVICE_META,
        6,
        GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED,
        0x22,
        0x00,  // hids_cid = 0x0022
        ERROR_CODE_COMMAND_DISALLOWED,
        HID_PROTOCOL_MODE_REPORT,
        0,
    };
    uni_bt_le_on_hci_event_gattservice_meta(hid_conn_fail, sizeof(hid_conn_fail));
    EXPECT_EQ(0xffff, d->hids_cid);

    // 6. GATTSERVICE_SUBEVENT_HID_REPORT with orphan hids_cid (0x0bad) and with report_len == 0:
    d->hids_cid = 0x0022;
    uint8_t hid_rpt_orphan[10] = {
        HCI_EVENT_GATTSERVICE_META,
        8,
        GATTSERVICE_SUBEVENT_HID_REPORT,
        0xad,
        0x0b,  // hids_cid = 0x0bad
        0x00,  // service_index
        0x01,  // report_id
        0x00,
        0x00,  // report_len = 0
        0x00,
    };
    uni_bt_le_on_hci_event_gattservice_meta(hid_rpt_orphan, sizeof(hid_rpt_orphan));
    EXPECT_EQ(0, g_mock_input_report_count);

    uint8_t hid_rpt_empty[10] = {
        HCI_EVENT_GATTSERVICE_META,
        8,
        GATTSERVICE_SUBEVENT_HID_REPORT,
        0x22,
        0x00,  // hids_cid = 0x0022
        0x00,  // service_index
        0x01,  // report_id
        0x00,
        0x00,  // report_len = 0 (< 1)
        0x00,
    };
    uni_bt_le_on_hci_event_gattservice_meta(hid_rpt_empty, sizeof(hid_rpt_empty));
    EXPECT_EQ(0, g_mock_input_report_count);
}

// ============================================================================
// 14. TEST(bt_service_device_lifecycle_and_att_write_validation)
// ============================================================================

TEST(bt_service_device_lifecycle_and_att_write_validation) {
    reset_test_fixture();

    // Enable the Bluepad32 BLE GATT service (registers profile_data and ATT read/write callbacks).
    uni_bt_service_set_enabled(false);
    EXPECT_FALSE(uni_bt_service_is_enabled());
    uni_bt_service_set_enabled(true);
    EXPECT_TRUE(uni_bt_service_is_enabled());

    bd_addr_t addr = {0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x01};
    uni_hid_device_t* d = uni_hid_device_create(addr);
    ASSERT_NE(NULL, d);
    uni_hid_device_set_vendor_id(d, 0x054c);
    uni_hid_device_set_product_id(d, 0x09cc);
    d->controller_type = CONTROLLER_TYPE_PS4Controller;
    d->controller_subtype = CONTROLLER_SUBTYPE_NONE;
    d->conn.state = UNI_BT_CONN_STATE_L2CAP_INTERRUPT_CONNECTED;
    d->conn.incoming = 1;

    uni_bt_service_on_device_connected(d);
    d->controller_subtype = CONTROLLER_SUBTYPE_WIIMOTE_HORIZONTAL;
    d->conn.state = UNI_BT_CONN_STATE_DEVICE_READY;
    uni_bt_service_on_device_ready(d);

    // Prepare an ATT connection context with MTU = 256 to read compact_devices (handle 0x0011, AC05).
    att_connection_t att_conn;
    memset(&att_conn, 0, sizeof(att_conn));
    att_conn.con_handle = 0x0040;
    att_conn.mtu = 256;
    att_conn.max_mtu = 256;

    uint8_t req_read_devices[3] = {ATT_READ_REQUEST, 0x11, 0x00};
    uint8_t rsp_buf[256];
    memset(rsp_buf, 0, sizeof(rsp_buf));
    uint16_t rsp_len = att_handle_request(&att_conn, req_read_devices, sizeof(req_read_devices), rsp_buf);
    ASSERT_TRUE(rsp_len >= 1 + 16);
    EXPECT_EQ(ATT_READ_RESPONSE, rsp_buf[0]);
    // Verify compact_devices[0] layout: idx (u8), addr (6B), vendor_id (le16), product_id (le16),
    // state (u8), incoming (u8), controller_type (le16), controller_subtype (u8).
    EXPECT_EQ(0, rsp_buf[1]);
    EXPECT_EQ(0, memcmp(&rsp_buf[2], addr, 6));
    EXPECT_EQ(0x054c, little_endian_read_16(rsp_buf, 8));
    EXPECT_EQ(0x09cc, little_endian_read_16(rsp_buf, 10));
    EXPECT_EQ(UNI_BT_CONN_STATE_DEVICE_READY, rsp_buf[12]);
    EXPECT_EQ(1, rsp_buf[13]);
    EXPECT_EQ(CONTROLLER_TYPE_PS4Controller, little_endian_read_16(rsp_buf, 14));
    EXPECT_EQ(CONTROLLER_SUBTYPE_WIIMOTE_HORIZONTAL, rsp_buf[16]);

    // Disconnect device and verify compact_devices[0] is cleared while preserving idx = 0.
    uni_bt_service_on_device_disconnected(d);
    memset(rsp_buf, 0, sizeof(rsp_buf));
    rsp_len = att_handle_request(&att_conn, req_read_devices, sizeof(req_read_devices), rsp_buf);
    ASSERT_TRUE(rsp_len >= 1 + 16);
    EXPECT_EQ(ATT_READ_RESPONSE, rsp_buf[0]);
    EXPECT_EQ(0, rsp_buf[1]);
    EXPECT_EQ(0, little_endian_read_16(rsp_buf, 8));
    EXPECT_EQ(0, little_endian_read_16(rsp_buf, 10));

    // Validate ATT_WRITE_REQUEST error paths in uni_att_write_callback:
    // 1. Short 1-byte write to CCCD handle 0x0014 (AC06 Client Configuration) ->
    //    ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH (0x0d).
    uint8_t req_short_cccd[4] = {ATT_WRITE_REQUEST, 0x14, 0x00, 0x01};
    rsp_len = att_handle_request(&att_conn, req_short_cccd, sizeof(req_short_cccd), rsp_buf);
    ASSERT_EQ(5, rsp_len);
    EXPECT_EQ(ATT_ERROR_RESPONSE, rsp_buf[0]);
    EXPECT_EQ(ATT_WRITE_REQUEST, rsp_buf[1]);
    EXPECT_EQ(0x0014, little_endian_read_16(rsp_buf, 2));
    EXPECT_EQ(ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH, rsp_buf[4]);

    // 2a. Out-of-bounds mappings type write (UNI_GAMEPAD_MAPPINGS_TYPE_COUNT) to AC07 (handle 0x0016) ->
    //     ATT_ERROR_VALUE_NOT_ALLOWED (0x13).
    uint8_t req_oob_map[4] = {ATT_WRITE_REQUEST, 0x16, 0x00, UNI_GAMEPAD_MAPPINGS_TYPE_COUNT};
    rsp_len = att_handle_request(&att_conn, req_oob_map, sizeof(req_oob_map), rsp_buf);
    ASSERT_EQ(5, rsp_len);
    EXPECT_EQ(ATT_ERROR_RESPONSE, rsp_buf[0]);
    EXPECT_EQ(0x0016, little_endian_read_16(rsp_buf, 2));
    EXPECT_EQ(ATT_ERROR_VALUE_NOT_ALLOWED, rsp_buf[4]);

    // 2b. Out-of-bounds device index write (CONFIG_BLUEPAD32_MAX_DEVICES) to AC0B (handle 0x001e) ->
    //     ATT_ERROR_REQUEST_NOT_SUPPORTED (0x06).
    uint8_t req_oob_disc[4] = {ATT_WRITE_REQUEST, 0x1e, 0x00, CONFIG_BLUEPAD32_MAX_DEVICES};
    rsp_len = att_handle_request(&att_conn, req_oob_disc, sizeof(req_oob_disc), rsp_buf);
    ASSERT_EQ(5, rsp_len);
    EXPECT_EQ(ATT_ERROR_RESPONSE, rsp_buf[0]);
    EXPECT_EQ(0x001e, little_endian_read_16(rsp_buf, 2));
    EXPECT_EQ(ATT_ERROR_REQUEST_NOT_SUPPORTED, rsp_buf[4]);

    // 3. Write 0 (false) to AC0C (handle 0x0020, delete Bluetooth keys) ->
    //    ATT_ERROR_REQUEST_NOT_SUPPORTED (0x06).
    uint8_t req_false_del_keys[4] = {ATT_WRITE_REQUEST, 0x20, 0x00, 0x00};
    rsp_len = att_handle_request(&att_conn, req_false_del_keys, sizeof(req_false_del_keys), rsp_buf);
    ASSERT_EQ(5, rsp_len);
    EXPECT_EQ(ATT_ERROR_RESPONSE, rsp_buf[0]);
    EXPECT_EQ(0x0020, little_endian_read_16(rsp_buf, 2));
    EXPECT_EQ(ATT_ERROR_REQUEST_NOT_SUPPORTED, rsp_buf[4]);

    // 4. Valid 1-byte write to AC04 (handle 0x000f, scan for new connections) toggles uni_bt_is_scanning().
    uint8_t req_scan_on[4] = {ATT_WRITE_REQUEST, 0x0f, 0x00, 0x01};
    rsp_len = att_handle_request(&att_conn, req_scan_on, sizeof(req_scan_on), rsp_buf);
    ASSERT_EQ(1, rsp_len);
    EXPECT_EQ(ATT_WRITE_RESPONSE, rsp_buf[0]);
    EXPECT_TRUE(uni_bt_is_scanning());

    uint8_t req_scan_off[4] = {ATT_WRITE_REQUEST, 0x0f, 0x00, 0x00};
    rsp_len = att_handle_request(&att_conn, req_scan_off, sizeof(req_scan_off), rsp_buf);
    ASSERT_EQ(1, rsp_len);
    EXPECT_EQ(ATT_WRITE_RESPONSE, rsp_buf[0]);
    EXPECT_FALSE(uni_bt_is_scanning());

    uni_hid_device_delete(d);
    uni_bt_service_set_enabled(false);
    EXPECT_FALSE(uni_bt_service_is_enabled());
}

// ============================================================================
// 15. TEST(bt_le_connection_and_hids_failure_resumes_scanning)
// ============================================================================

TEST(bt_le_connection_and_hids_failure_resumes_scanning) {
    reset_test_fixture();

    // Enable BLE scanning synchronously so `is_scanning == true` inside `uni_bt_le.c`.
    uni_bt_le_set_enabled(true);
    uni_bt_start_scanning_and_autoconnect_unsafe();
    EXPECT_TRUE(uni_bt_is_scanning());

    // Sub-test 1: HCI_SUBEVENT_LE_CONNECTION_COMPLETE with error status 0x3e
    // (ERROR_CODE_CONNECTION_FAILED_TO_BE_ESTABLISHED) deletes the device and resumes scanning.
    bd_addr_t peer_addr1 = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    uint8_t adv_pkt[64];
    uint16_t adv_len = build_le_adv_report_pkt(adv_pkt, peer_addr1, 210, UNI_BT_HID_APPEARANCE_GAMEPAD, "BLE Pad 1", 9);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, adv_pkt, adv_len);
    ASSERT_NE(NULL, uni_hid_device_get_instance_for_address(peer_addr1));

    uint8_t le_conn_evt[21];
    memset(le_conn_evt, 0, sizeof(le_conn_evt));
    le_conn_evt[0] = HCI_EVENT_LE_META;
    le_conn_evt[1] = 19;
    le_conn_evt[2] = HCI_SUBEVENT_LE_CONNECTION_COMPLETE;
    le_conn_evt[3] = ERROR_CODE_CONNECTION_FAILED_TO_BE_ESTABLISHED;  // 0x3e
    little_endian_store_16(le_conn_evt, 4, 0x0040);
    le_conn_evt[6] = HCI_ROLE_MASTER;
    le_conn_evt[7] = BD_ADDR_TYPE_LE_PUBLIC;
    put_bd_addr_reversed(&le_conn_evt[8], peer_addr1);

    uni_bt_le_on_hci_event_le_meta(le_conn_evt, sizeof(le_conn_evt));
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(peer_addr1));
    EXPECT_TRUE(uni_bt_is_scanning());

    // Sub-test 2: HCI_SUBEVENT_LE_CONNECTION_COMPLETE for an unknown address AA:BB:CC:DD:EE:99
    // with error status 0x02 and success status 0x00 does not crash and keeps scanning active.
    bd_addr_t unknown_addr = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x99};
    le_conn_evt[3] = 0x02;
    put_bd_addr_reversed(&le_conn_evt[8], unknown_addr);
    uni_bt_le_on_hci_event_le_meta(le_conn_evt, sizeof(le_conn_evt));
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(unknown_addr));
    EXPECT_TRUE(uni_bt_is_scanning());

    le_conn_evt[3] = ERROR_CODE_SUCCESS;
    uni_bt_le_on_hci_event_le_meta(le_conn_evt, sizeof(le_conn_evt));
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(unknown_addr));
    EXPECT_TRUE(uni_bt_is_scanning());

    // Sub-test 3: GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED with non-zero status
    // (ATT_ERROR_INSUFFICIENT_ENCRYPTION = 0x0f) clears hids_cid and resumes scanning.
    bd_addr_t peer_addr2 = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02};
    adv_len = build_le_adv_report_pkt(adv_pkt, peer_addr2, 210, UNI_BT_HID_APPEARANCE_GAMEPAD, "BLE Pad 2", 9);
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, adv_pkt, adv_len);
    uni_hid_device_t* d2 = uni_hid_device_get_instance_for_address(peer_addr2);
    ASSERT_NE(NULL, d2);

    le_conn_evt[3] = ERROR_CODE_SUCCESS;
    little_endian_store_16(le_conn_evt, 4, 0x0041);
    put_bd_addr_reversed(&le_conn_evt[8], peer_addr2);
    uni_bt_le_on_hci_event_le_meta(le_conn_evt, sizeof(le_conn_evt));
    EXPECT_EQ(0x0041, d2->conn.handle);
    d2->hids_cid = 0x0033;

    uint8_t hid_conn_err[8] = {
        HCI_EVENT_GATTSERVICE_META,
        6,
        GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED,
        0x33,
        0x00,                               // hids_cid = 0x0033
        ATT_ERROR_INSUFFICIENT_ENCRYPTION,  // 0x0f != ERROR_CODE_SUCCESS
        HID_PROTOCOL_MODE_REPORT,
        0,
    };
    uni_bt_le_on_hci_event_gattservice_meta(hid_conn_err, sizeof(hid_conn_err));
    EXPECT_EQ(0xffff, d2->hids_cid);
    EXPECT_TRUE(uni_bt_is_scanning());

    // Clean up connection via HCI_EVENT_DISCONNECTION_COMPLETE.
    uint8_t disc_pkt[6] = {HCI_EVENT_DISCONNECTION_COMPLETE, 4, 0x00, 0x41, 0x00, 0x13};
    uni_bt_packet_handler(HCI_EVENT_PACKET, 0, disc_pkt, sizeof(disc_pkt));
    EXPECT_EQ(NULL, uni_hid_device_get_instance_for_address(peer_addr2));
    uni_bt_stop_scanning_unsafe();
}

// ============================================================================
// 16. TEST(bt_sdp_query_attribute_value_oob_data_offset_guard)
// ============================================================================

TEST(bt_sdp_query_attribute_value_oob_data_offset_guard) {
    reset_test_fixture();

    bd_addr_t addr = {0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uni_hid_device_t* d = uni_hid_device_create(addr);
    ASSERT_NE(NULL, d);
    uni_hid_device_set_incoming(d, true);
    uni_bt_sdp_set_device_for_test(d);

    uint8_t pkt[11];
    pkt[0] = SDP_EVENT_QUERY_ATTRIBUTE_VALUE;
    pkt[1] = 9;
    little_endian_store_16(pkt, 2, 1);  // record_id = 1

    // 1. PID query handler (uni_handle_sdp_pid_query_result):
    //    attribute_length = 10 (<= 512), data_offset = 10 (boundary == attribute_length) and 2000 (> 512).
    little_endian_store_16(pkt, 4, BLUETOOTH_ATTRIBUTE_VENDOR_ID);
    little_endian_store_16(pkt, 6, 10);  // attribute_length = 10
    little_endian_store_16(pkt, 8, 10);  // data_offset = 10 (== attribute_length)
    pkt[10] = 0xAA;
    uni_handle_sdp_pid_query_result(HCI_EVENT_PACKET, 0, pkt, sizeof(pkt));

    little_endian_store_16(pkt, 8, 2000);  // data_offset = 2000 (> 512)
    uni_handle_sdp_pid_query_result(HCI_EVENT_PACKET, 0, pkt, sizeof(pkt));
    EXPECT_EQ(0, uni_hid_device_get_vendor_id(d));

    // 2. HID descriptor query handler (uni_handle_sdp_hid_query_result):
    //    attribute_length = 16 (<= 512), data_offset = 16 (== attribute_length) and 512 (== buffer_size).
    little_endian_store_16(pkt, 4, BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST);
    little_endian_store_16(pkt, 6, 16);  // attribute_length = 16
    little_endian_store_16(pkt, 8, 16);  // data_offset = 16 (== attribute_length)
    pkt[10] = 0xBB;
    uni_handle_sdp_hid_query_result(HCI_EVENT_PACKET, 0, pkt, sizeof(pkt));

    little_endian_store_16(pkt, 8, 512);  // data_offset = 512 (== MAX_ATTRIBUTE_VALUE_SIZE)
    uni_handle_sdp_hid_query_result(HCI_EVENT_PACKET, 0, pkt, sizeof(pkt));
    EXPECT_EQ(0, d->hid_descriptor_len);

    // 3. Follow immediately with a valid 3-byte DE_UINT16 attribute sequence (0x054c) to confirm
    //    normal SDP attribute reassembly still functions after rejecting malformed offsets.
    const uint8_t de_vid_sony[] = {0x09, 0x05, 0x4c};
    stream_sdp_attribute_bytes(uni_handle_sdp_pid_query_result, 1, BLUETOOTH_ATTRIBUTE_VENDOR_ID, de_vid_sony,
                               sizeof(de_vid_sony));
    EXPECT_EQ(0x054c, uni_hid_device_get_vendor_id(d));

    uni_bt_sdp_set_device_for_test(NULL);
    uni_hid_device_delete(d);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(void) {
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    uni_property_init();
    hci_init(&g_dummy_transport, NULL);
    l2cap_init();
    uni_hid_device_setup();
    uni_bt_allowlist_init();
    uni_virtual_device_init();

    RUN_TEST(bt_setup_state_machine_full_walk);
    RUN_TEST(bt_gap_inquiry_result_with_rssi_and_cod_filter);
    RUN_TEST(bt_l2cap_channel_opened_psm_routing_and_error_cleanup);
    RUN_TEST(bt_l2cap_incoming_connection_accept_and_decline);
    RUN_TEST(bt_sdp_pid_and_hid_query_result_chunks_and_truncation);
    RUN_TEST(bt_le_adv_report_64byte_name_overflow_regression);
    RUN_TEST(bt_bredr_l2cap_data_packet_strips_header_and_routes);
    RUN_TEST(bt_disconnect_cleans_up_device);
    RUN_TEST(bt_sdp_query_abort_on_disconnect_and_failure_b5);
    RUN_TEST(bt_le_pnp_id_att_error_status_guard_b6);
    RUN_TEST(bt_le_setup_legacy_pairing_steam_controller_regression);
    RUN_TEST(bredr_l2cap_data_packet_bounds_and_orphan_channel);
    RUN_TEST(le_hogp_truncated_packet_and_discovery_bounds);
    RUN_TEST(bt_service_device_lifecycle_and_att_write_validation);
    RUN_TEST(bt_le_connection_and_hids_failure_resumes_scanning);
    RUN_TEST(bt_sdp_query_attribute_value_oob_data_offset_guard);

    return test_summary();
}
