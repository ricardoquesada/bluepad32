/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2019 Ricardo Quesada

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
****************************************************************************/

#include "uni_hid_device.h"

#include <stdbool.h>
#include <sys/time.h>

#include "sdkconfig.h"
#include "uni_ble.h"
#include "uni_bt_defines.h"
#include "uni_circular_buffer.h"
#include "uni_config.h"
#include "uni_controller.h"
#include "uni_gamepad.h"
#include "uni_hid_device_vendors.h"
#include "uni_hid_parser_8bitdo.h"
#include "uni_hid_parser_android.h"
#include "uni_hid_parser_ds3.h"
#include "uni_hid_parser_ds4.h"
#include "uni_hid_parser_ds5.h"
#include "uni_hid_parser_generic.h"
#include "uni_hid_parser_icade.h"
#include "uni_hid_parser_mouse.h"
#include "uni_hid_parser_nimbus.h"
#include "uni_hid_parser_ouya.h"
#include "uni_hid_parser_smarttvremote.h"
#include "uni_hid_parser_switch.h"
#include "uni_hid_parser_wii.h"
#include "uni_hid_parser_xboxone.h"
#include "uni_log.h"
#include "uni_platform.h"

enum {
    FLAGS_HAS_COD = (1 << 8),
    FLAGS_HAS_NAME = (1 << 9),
    FLAGS_HAS_HID_DESCRIPTOR = (1 << 10),
    FLAGS_HAS_VENDOR_ID = (1 << 11),
    FLAGS_HAS_PRODUCT_ID = (1 << 12),
    FLAGS_HAS_CONTROLLER_TYPE = (1 << 13),
};

#define MISC_BUTTON_DELAY_MS 200

static uni_hid_device_t g_devices[CONFIG_BLUEPAD32_MAX_DEVICES];
static const bd_addr_t zero_addr = {0, 0, 0, 0, 0, 0};

static void process_misc_button_system(uni_hid_device_t* d);
static void process_misc_button_home(uni_hid_device_t* d);
static void misc_button_enable_callback(btstack_timer_source_t* ts);
static void device_connection_timeout(btstack_timer_source_t* ts);
static void start_connection_timeout(uni_hid_device_t* d);

void uni_hid_device_setup(void) {
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++)
        uni_hid_device_init(&g_devices[i]);
}

uni_hid_device_t* uni_hid_device_create(bd_addr_t address) {
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        if (bd_addr_cmp(g_devices[i].conn.btaddr, zero_addr) == 0) {
            logi("Creating device: %s (idx=%d)\n", bd_addr_to_str(address), i);

            memset(&g_devices[i], 0, sizeof(g_devices[i]));
            memcpy(g_devices[i].conn.btaddr, address, 6);

            // Delete device if it doesn't have a connection
            start_connection_timeout(&g_devices[i]);
            return &g_devices[i];
        }
    }
    return NULL;
}

void uni_hid_device_init(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("Invalid device\n");
        return;
    }
    memset(d, 0, sizeof(*d));
    d->hids_cid = 0xffff;

    uni_bt_conn_init(&d->conn);
}

uni_hid_device_t* uni_hid_device_get_instance_for_address(bd_addr_t addr) {
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        if (bd_addr_cmp(addr, g_devices[i].conn.btaddr) == 0) {
            return &g_devices[i];
        }
    }
    return NULL;
}

uni_hid_device_t* uni_hid_device_get_instance_for_cid(uint16_t cid) {
    if (cid == 0)
        return NULL;
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        if (g_devices[i].conn.interrupt_cid == cid || g_devices[i].conn.control_cid == cid)
            return &g_devices[i];
    }
    return NULL;
}

uni_hid_device_t* uni_hid_device_get_instance_for_hids_cid(uint16_t cid) {
    if (cid == 0)
        return NULL;
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        if (g_devices[i].hids_cid == cid)
            return &g_devices[i];
    }
    return NULL;
}

uni_hid_device_t* uni_hid_device_get_instance_for_connection_handle(hci_con_handle_t handle) {
    if (handle == UNI_BT_CONN_HANDLE_INVALID)
        return NULL;
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        if (g_devices[i].conn.handle == handle) {
            return &g_devices[i];
        }
    }
    return NULL;
}

uni_hid_device_t* uni_hid_device_get_instance_with_predicate(uni_hid_device_predicate_t predicate, void* data) {
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        // Only "ready" devices are propagated
        if (uni_bt_conn_get_state(&g_devices[i].conn) != UNI_BT_CONN_STATE_DEVICE_READY)
            continue;
        if (predicate(&g_devices[i], data))
            return &g_devices[i];
    }
    return NULL;
}

uni_hid_device_t* uni_hid_device_get_instance_for_idx(int idx) {
    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
        return NULL;
    return &g_devices[idx];
}

int uni_hid_device_get_idx_for_instance(uni_hid_device_t* d) {
    int idx = (d - &g_devices[0]) / sizeof(g_devices[0]);

    if (idx < 0 || idx >= CONFIG_BLUEPAD32_MAX_DEVICES)
        return -1;
    return idx;
}

uni_hid_device_t* uni_hid_device_get_first_device_with_state(uni_bt_conn_state_t state) {
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        if ((bd_addr_cmp(g_devices[i].conn.btaddr, zero_addr) != 0) &&
            uni_bt_conn_get_state(&g_devices[i].conn) == state)
            return &g_devices[i];
    }
    return NULL;
}

void uni_hid_device_set_ready(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("ERROR: Invalid NULL device\n");
        return;
    }

    if (uni_bt_conn_get_state(&d->conn) == UNI_BT_CONN_STATE_DEVICE_PENDING_READY) {
        loge("uni_hid_device_set_ready(): Error, device already in 'ready-pending' state, skipping\n");
        return;
    }

    uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_DEVICE_PENDING_READY);

    // Each "parser" is responsible to call uni_hid_device_set_ready() once the
    // "parser" is ready.
    if (d->report_parser.setup)
        d->report_parser.setup(d);
    else {
        // If parser.setup() is not present, it is safe to assume that the setup is complete
        uni_hid_device_set_ready_complete(d);
    }
}

void uni_hid_device_set_ready_complete(uni_hid_device_t* d) {
    // Called once the "parser" is ready.
    if (d == NULL) {
        loge("ERROR: Invalid NULL device\n");
        return;
    }

    if (uni_bt_conn_get_state(&d->conn) == UNI_BT_CONN_STATE_DEVICE_READY) {
        loge("uni_hid_device_set_ready_complete(): Error, device already in 'ready-complete' state, skipping\n");
        return;
    }

    logi("Device setup (%s) is complete\n", bd_addr_to_str(d->conn.btaddr));

    // Remove the timer once the connection was established.
    btstack_run_loop_remove_timer(&d->connection_timer);

    // Platform is able to decline a gamepad, should it needs it.
    if (uni_get_platform()->on_device_ready(d) == 0) {
        uni_bt_conn_set_state(&d->conn, UNI_BT_CONN_STATE_DEVICE_READY);
    } else {
        loge("Platform declined the gamepad, deleting it");
        uni_hid_device_disconnect(d);
        uni_hid_device_delete(d);
        /* 'd' is destroyed after this call, don't use it */
    }
}

void uni_hid_device_request_inquire(void) {
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        // retry remote name request
        if (uni_bt_conn_get_state(&g_devices[i].conn) == UNI_BT_CONN_STATE_REMOTE_NAME_INQUIRED)
            uni_bt_conn_set_state(&g_devices[i].conn, UNI_BT_CONN_STATE_REMOTE_NAME_REQUEST);
    }
}

void uni_hid_device_on_connected(uni_hid_device_t* d, bool connected) {
    if (d == NULL) {
        log_error("ERROR: Invalid device\n");
        return;
    }

    if (connected) {
        // connected
        uni_get_platform()->on_device_connected(d);
    } else {
        // disconnected
        uni_get_platform()->on_device_disconnected(d);
    }
}

void uni_hid_device_set_cod(uni_hid_device_t* d, uint32_t cod) {
    if (d == NULL) {
        log_error("ERROR: Invalid device\n");
        return;
    }

    d->cod = cod;
    if (cod == 0)
        d->flags &= ~FLAGS_HAS_COD;
    else
        d->flags |= FLAGS_HAS_COD;
}

bool uni_hid_device_is_cod_supported(uint32_t cod) {
    const uint32_t minor_cod = cod & UNI_BT_COD_MINOR_MASK;

    // Peripherals: joysticks, mice, gamepads and keyboard are accepted.
    if ((cod & UNI_BT_COD_MAJOR_MASK) == UNI_BT_COD_MAJOR_PERIPHERAL) {
        // Device is a peripheral: keyboard, mouse, joystick, gamepad, etc.
        // We only care about joysticks, gamepads & mice. But some gamepads,
        // specially cheap ones are advertised as keyboards.
        // FIXME: Which gamepads are advertised as keyboard?
        return !!(minor_cod & (UNI_BT_COD_MINOR_MICE | UNI_BT_COD_MINOR_KEYBOARD | UNI_BT_COD_MINOR_GAMEPAD |
                               UNI_BT_COD_MINOR_JOYSTICK));
    }

    // Hack for Amazon Fire TV remote control: CoD: 0x00400408 (Audio + Telephony Hands free)
    // FIXME: This is no longer true, since a "set-event-filter" is being called accepting only
    // MAJOR_PERIPHERAL devices.
    // Acceptable trade-off: The Amazon Stick is no longer working. On the other hand we have
    // muuuuuch cleaner logs, and easier to analyze hci dumps.
    if ((cod & UNI_BT_COD_MAJOR_MASK) == UNI_BT_COD_MAJOR_AUDIO_VIDEO) {
        return (cod == 0x400408);
    }
    return false;
}

void uni_hid_device_set_incoming(uni_hid_device_t* d, bool incoming) {
    if (d == NULL) {
        log_error("ERROR: Invalid device\n");
        return;
    }

    d->conn.incoming = incoming;
}

bool uni_hid_device_is_incoming(uni_hid_device_t* d) {
    return d->conn.incoming;
}

void uni_hid_device_set_name(uni_hid_device_t* d, const char* name) {
    if (d == NULL) {
        log_error("ERROR: Invalid device\n");
        return;
    }
    if (name == NULL) {
        log_error("Invalid name");
        return;
    }

    strncpy(d->name, name, sizeof(d->name) - 1);
    d->name[sizeof(d->name) - 1] = 0;

    d->flags |= FLAGS_HAS_NAME;
}

bool uni_hid_device_has_name(uni_hid_device_t* d) {
    if (d == NULL) {
        log_error("ERROR: Invalid device\n");
        return false;
    }

    return !!(d->flags & FLAGS_HAS_NAME);
}

void uni_hid_device_set_hid_descriptor(uni_hid_device_t* d, const uint8_t* descriptor, int len) {
    if (d == NULL) {
        log_error("ERROR: Invalid device\n");
        return;
    }

    int min = btstack_min(HID_MAX_DESCRIPTOR_LEN, len);
    memcpy(d->hid_descriptor, descriptor, len);
    d->hid_descriptor_len = min;
    d->flags |= FLAGS_HAS_HID_DESCRIPTOR;
}

bool uni_hid_device_has_hid_descriptor(uni_hid_device_t* d) {
    if (d == NULL) {
        log_error("ERROR: Invalid device\n");
        return false;
    }

    return !!(d->flags & FLAGS_HAS_HID_DESCRIPTOR);
}

void uni_hid_device_set_product_id(uni_hid_device_t* d, uint16_t product_id) {
    d->product_id = product_id;
    d->flags |= FLAGS_HAS_PRODUCT_ID;
}

uint16_t uni_hid_device_get_product_id(uni_hid_device_t* d) {
    return d->product_id;
}

void uni_hid_device_set_vendor_id(uni_hid_device_t* d, uint16_t vendor_id) {
    if (vendor_id == 0) {
        loge("Invalid vendor_id = 0%04x\n", vendor_id);
        return;
    }
    d->vendor_id = vendor_id;
    d->flags |= FLAGS_HAS_VENDOR_ID;
}

uint16_t uni_hid_device_get_vendor_id(uni_hid_device_t* d) {
    return d->vendor_id;
}

void uni_hid_device_connect(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("uni_hid_device_connect: invalid hid device: NULL\n");
        return;
    }

    logi("Device %s is connected\n", bd_addr_to_str(d->conn.btaddr));

    // Update connection state
    uni_bt_conn_set_connected(&d->conn, true);
    // Tell platforms connection is ready
    uni_hid_device_on_connected(d, true);
}

void uni_hid_device_disconnect(uni_hid_device_t* d) {
    // Might be called from different states... perhaps the device was already connected.
    // Or perhaps it got disconnected in the middle of a connection.
    bool connected = false;

    if (d == NULL) {
        loge("uni_hid_device_disconnect: invalid hid device: NULL\n");
        return;
    }

    logi("Disconnecting device: %s\n", bd_addr_to_str(d->conn.btaddr));

    connected = d->conn.connected;

    // Cleanup BLE
    uni_ble_disconnect(d->conn.handle);

    // Close possible open connections
    uni_bt_conn_disconnect(&d->conn);

    // Disconnected, so no longer needs the timers
    btstack_run_loop_remove_timer(&d->connection_timer);
    btstack_run_loop_remove_timer(&d->inquiry_remote_name_timer);

    // If it was already connected, tell platforms
    if (connected)
        uni_hid_device_on_connected(d, false);
}

void uni_hid_device_delete(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("uni_hid_device_delete: invalid hid device: NULL\n");
        return;
    }
    logi("Deleting device: %s\n", bd_addr_to_str(d->conn.btaddr));

    // Remove the timer. If it was still running, it will crash if the handler gets called.
    btstack_run_loop_remove_timer(&d->connection_timer);

    uni_hid_device_init(d);
}

void uni_hid_device_dump_device(uni_hid_device_t* d) {
    char* conn_type;
    gap_connection_type_t type;

    type = gap_get_connection_type(d->conn.handle);
    switch (type) {
        case GAP_CONNECTION_ACL:
            conn_type = "ACL";
            break;
        case GAP_CONNECTION_SCO:
            conn_type = "SCO";
            break;
        case GAP_CONNECTION_LE:
            conn_type = "BLE";
            break;
        default:
            conn_type = "Invalid";
            break;
    }

    logi("\tbtaddr: %s\n", bd_addr_to_str(d->conn.btaddr));
    logi("\tbt: handle=%d (%s), ctrl_cid=0x%04x, intr_cid=0x%04x, cod=0x%08x, flags=0x%08x, incoming=%d\n",
         d->conn.handle, conn_type, d->conn.control_cid, d->conn.interrupt_cid, d->cod, d->flags, d->conn.incoming);
    logi("\tmodel: vid=0x%04x, pid=0x%04x, model='%s', name='%s'\n", d->vendor_id, d->product_id,
         uni_gamepad_get_model_name(d->controller_type), d->name);
    if (uni_get_platform()->device_dump)
        uni_get_platform()->device_dump(d);
    if (d->report_parser.device_dump)
        d->report_parser.device_dump(d);
}

void uni_hid_device_dump_all(void) {
    logi("Connected devices:\n");
    for (int i = 0; i < CONFIG_BLUEPAD32_MAX_DEVICES; i++) {
        if (bd_addr_cmp(g_devices[i].conn.btaddr, zero_addr) == 0)
            continue;
        logi("idx=%d:\n", i);
        uni_hid_device_dump_device(&g_devices[i]);
        logi("\n");
    }
}

bool uni_hid_device_guess_controller_type_from_name(uni_hid_device_t* d, const char* name) {
    if (!name)
        return false;

    // Try with the different matchers.
    bool ret = uni_hid_parser_ds3_does_name_match(d, name);
    ret = ret || uni_hid_parser_switch_does_name_match(d, name);
    if (ret) {
        uni_hid_device_guess_controller_type_from_pid_vid(d);
    }
    return ret;
}

void uni_hid_device_guess_controller_type_from_pid_vid(uni_hid_device_t* d) {
    if (uni_hid_device_has_controller_type(d)) {
        logi("device already has a controller type");
        return;
    }
    // Try to guess it from Vendor/Product id.
    uni_controller_type_t type = guess_controller_type(d->vendor_id, d->product_id);

    // If it fails, try to guess it from COD
    if (type == CONTROLLER_TYPE_Unknown) {
        logi("Device (vendor_id=0x%04x, product_id=0x%04x) not found in DB.\n", d->vendor_id, d->product_id);
        if (uni_hid_device_is_mouse(d)) {
            type = CONTROLLER_TYPE_GenericMouse;
        } else if (uni_hid_device_is_keyboard(d)) {
            type = CONTROLLER_TYPE_GenericKeyboard;
        } else {
            // FIXME: Default should be the most popular gamepad device.
            loge("Failed to find gamepad profile for device. Fallback: using Android profile.\n");
            type = CONTROLLER_TYPE_AndroidController;
        }
    }

    // Subtype is still unknown, it will be set by the relevant parse_input_report() func
    d->controller_subtype = CONTROLLER_SUBTYPE_NONE;

    memset(&d->report_parser, 0, sizeof(d->report_parser));

    switch (type) {
        case CONTROLLER_TYPE_iCadeController:
            d->report_parser.setup = uni_hid_parser_icade_setup;
            d->report_parser.parse_usage = uni_hid_parser_icade_parse_usage;
            logi("Device detected as iCade: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_OUYAController:
            d->report_parser.init_report = uni_hid_parser_ouya_init_report;
            d->report_parser.parse_usage = uni_hid_parser_ouya_parse_usage;
            d->report_parser.set_player_leds = uni_hid_parser_ouya_set_player_leds;
            logi("Device detected as OUYA: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_XBoxOneController:
            d->report_parser.setup = uni_hid_parser_xboxone_setup;
            d->report_parser.init_report = uni_hid_parser_xboxone_init_report;
            d->report_parser.parse_usage = uni_hid_parser_xboxone_parse_usage;
            d->report_parser.set_rumble = uni_hid_parser_xboxone_set_rumble;
            logi("Device detected as Xbox Wireless: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_AndroidController:
            d->report_parser.init_report = uni_hid_parser_android_init_report;
            d->report_parser.parse_usage = uni_hid_parser_android_parse_usage;
            d->report_parser.set_player_leds = uni_hid_parser_android_set_player_leds;
            logi("Device detected as Android: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_NimbusController:
            d->report_parser.init_report = uni_hid_parser_nimbus_init_report;
            d->report_parser.parse_usage = uni_hid_parser_nimbus_parse_usage;
            d->report_parser.set_player_leds = uni_hid_parser_nimbus_set_player_leds;
            logi("Device detected as Nimbus: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_SmartTVRemoteController:
            d->report_parser.init_report = uni_hid_parser_smarttvremote_init_report;
            d->report_parser.parse_usage = uni_hid_parser_smarttvremote_parse_usage;
            logi("Device detected as Smart TV remote: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_PS3Controller:
            d->report_parser.setup = uni_hid_parser_ds3_setup;
            d->report_parser.init_report = uni_hid_parser_ds3_init_report;
            d->report_parser.parse_input_report = uni_hid_parser_ds3_parse_input_report;
            d->report_parser.set_player_leds = uni_hid_parser_ds3_set_player_leds;
            d->report_parser.set_rumble = uni_hid_parser_ds3_set_rumble;
            logi("Device detected as DUALSHOCK3: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_PS4Controller:
            d->report_parser.setup = uni_hid_parser_ds4_setup;
            d->report_parser.init_report = uni_hid_parser_ds4_init_report;
            d->report_parser.parse_input_report = uni_hid_parser_ds4_parse_input_report;
            d->report_parser.parse_feature_report = uni_hid_parser_ds4_parse_feature_report;
            d->report_parser.set_lightbar_color = uni_hid_parser_ds4_set_lightbar_color;
            d->report_parser.set_rumble = uni_hid_parser_ds4_set_rumble;
            d->report_parser.device_dump = uni_hid_parser_ds4_device_dump;
            logi("Device detected as DUALSHOCK4: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_PS5Controller:
            d->report_parser.init_report = uni_hid_parser_ds5_init_report;
            d->report_parser.setup = uni_hid_parser_ds5_setup;
            d->report_parser.parse_input_report = uni_hid_parser_ds5_parse_input_report;
            d->report_parser.parse_feature_report = uni_hid_parser_ds5_parse_feature_report;
            d->report_parser.set_player_leds = uni_hid_parser_ds5_set_player_leds;
            d->report_parser.set_lightbar_color = uni_hid_parser_ds5_set_lightbar_color;
            d->report_parser.set_rumble = uni_hid_parser_ds5_set_rumble;
            d->report_parser.device_dump = uni_hid_parser_ds5_device_dump;
            logi("Device detected as DualSense: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_8BitdoController:
            d->report_parser.init_report = uni_hid_parser_8bitdo_init_report;
            d->report_parser.parse_usage = uni_hid_parser_8bitdo_parse_usage;
            logi("Device detected as 8BITDO: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_GenericController:
            d->report_parser.init_report = uni_hid_parser_generic_init_report;
            d->report_parser.parse_usage = uni_hid_parser_generic_parse_usage;
            logi("Device detected as generic: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_WiiController:
            d->report_parser.setup = uni_hid_parser_wii_setup;
            d->report_parser.init_report = uni_hid_parser_wii_init_report;
            d->report_parser.parse_input_report = uni_hid_parser_wii_parse_input_report;
            d->report_parser.set_player_leds = uni_hid_parser_wii_set_player_leds;
            d->report_parser.set_rumble = uni_hid_parser_wii_set_rumble;
            d->report_parser.device_dump = uni_hid_parser_wii_device_dump;
            logi("Device detected as Wii controller: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_SwitchProController:
        case CONTROLLER_TYPE_SwitchJoyConRight:
        case CONTROLLER_TYPE_SwitchJoyConLeft:
            d->report_parser.setup = uni_hid_parser_switch_setup;
            d->report_parser.init_report = uni_hid_parser_switch_init_report;
            d->report_parser.parse_input_report = uni_hid_parser_switch_parse_input_report;
            d->report_parser.set_player_leds = uni_hid_parser_switch_set_player_leds;
            d->report_parser.set_rumble = uni_hid_parser_switch_set_rumble;
            d->report_parser.device_dump = uni_hid_parser_switch_device_dump;
            logi("Device detected as Nintendo Switch Pro controller: 0x%02x\n", type);
            break;
        case CONTROLLER_TYPE_GenericMouse:
            d->report_parser.setup = uni_hid_parser_mouse_setup;
            d->report_parser.parse_input_report = uni_hid_parser_mouse_parse_input_report;
            d->report_parser.init_report = uni_hid_parser_mouse_init_report;
            d->report_parser.parse_usage = uni_hid_parser_mouse_parse_usage;
            d->report_parser.device_dump = uni_hid_parser_mouse_device_dump;
            logi("Device detected as Mouse: 0x%02x\n", type);
            break;
        default:
            d->report_parser.init_report = uni_hid_parser_generic_init_report;
            d->report_parser.parse_usage = uni_hid_parser_generic_parse_usage;
            logi("Device not detected (0x%02x). Using generic driver.\n", type);
            break;
    }

    d->controller_type = type;
    d->flags |= FLAGS_HAS_CONTROLLER_TYPE;
}

bool uni_hid_device_has_controller_type(uni_hid_device_t* d) {
    if (d == NULL) {
        log_error("ERROR: Invalid device\n");
        return false;
    }

    return !!(d->flags & FLAGS_HAS_CONTROLLER_TYPE);
}

void uni_hid_device_set_connection_handle(uni_hid_device_t* d, hci_con_handle_t handle) {
    d->conn.handle = handle;
}

void uni_hid_device_process_controller(uni_hid_device_t* d) {
    uni_gamepad_t gp;
    if (uni_bt_conn_get_state(&d->conn) != UNI_BT_CONN_STATE_DEVICE_READY) {
        return;
    }

    if (d->controller.klass == UNI_CONTROLLER_CLASS_GAMEPAD) {
        gp = uni_gamepad_remap(&d->controller.gamepad);
        d->controller.gamepad = gp;
    }

    if (uni_get_platform()->on_controller_data != NULL)
        uni_get_platform()->on_controller_data(d, &d->controller);
    else if (uni_get_platform()->on_gamepad_data != NULL)
        uni_get_platform()->on_gamepad_data(d, &d->controller.gamepad);

    // FIXME: each backend should decide what to do with misc buttons
    process_misc_button_system(d);
    process_misc_button_home(d);
}

// Try to send the report now. If it can't, queue it and send it in the next
// event loop.
void uni_hid_device_send_report(uni_hid_device_t* d, uint16_t cid, const uint8_t* report, uint16_t len) {
    if (d == NULL) {
        loge("Invalid device\n");
        return;
    }
    if (cid <= 0) {
        loge("Invalid cid: %d\n", cid);
        return;
    }

    if (!report || len <= 0) {
        loge("Invalid report\n");
        return;
    }

    int err = l2cap_send(cid, (uint8_t*)report, len);
    if (err != 0) {
        logd("Could not send report (error=0x%04x). Adding it to queue\n", err);
        if (uni_circular_buffer_put(&d->outgoing_buffer, cid, report, len) != 0) {
            loge("ERROR: ciruclar buffer full. Cannot queue report\n");
        }
    }
    // Even, if it can send the report, trigger a "can send now event" in case
    // a report was queued.
    // TODO: Is this really needed?
    l2cap_request_can_send_now_event(cid);
}

// Sends an interrupt-report. If it can't it will queue it and try again later.
void uni_hid_device_send_intr_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (d == NULL) {
        loge("Invalid device\n");
        return;
    }
    uni_hid_device_send_report(d, d->conn.interrupt_cid, report, len);
}

// Queue a control-report and send it the report in the next event loop.
// It uses the "control" channel.
void uni_hid_device_send_ctrl_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (d == NULL) {
        loge("Invalid device\n");
        return;
    }
    uni_hid_device_send_report(d, d->conn.control_cid, report, len);
}

// Send the reports that are already queued. Uses the "intr" channel.
void uni_hid_device_send_queued_reports(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("Invalid device\n");
        return;
    }

    if (uni_circular_buffer_is_empty(&d->outgoing_buffer)) {
        logd("circular buffer empty?\n");
        return;
    }

    void* data;
    int data_len;
    int16_t cid;
    if (uni_circular_buffer_get(&d->outgoing_buffer, &cid, &data, &data_len) != UNI_CIRCULAR_BUFFER_ERROR_OK) {
        loge("ERROR: could not get buffer from circular buffer.\n");
        return;
    }
    uni_hid_device_send_report(d, cid, data, data_len);
}

bool uni_hid_device_does_require_hid_descriptor(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("uni_hid_device_does_require_hid_descriptor: failed, device is NULL\n");
        return false;
    }

    // If the parser has a "parse_usage" functions, it is safe to assume that a HID descriptor
    // is needed. "parse_usage" cannot work without a HID descriptor.
    return (d->report_parser.parse_usage != NULL);
}

bool uni_hid_device_is_mouse(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("uni_hid_device_is_mouse: failed, device is NULL\n");
        return false;
    }

    uint32_t mouse_cod = UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_MICE;
    return (d->cod & mouse_cod) == mouse_cod;
}

bool uni_hid_device_is_keyboard(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("uni_hid_device_is_keybaord: failed, device is NULL\n");
        return false;
    }
    uint32_t keyboard_cod = UNI_BT_COD_MAJOR_PERIPHERAL | UNI_BT_COD_MINOR_KEYBOARD;
    return (d->cod & keyboard_cod) == keyboard_cod;
}

bool uni_hid_device_is_gamepad(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("uni_hid_device_is_gamepad: failed, device is NULL\n");
        return false;
    }
    // If it a gamepad or a joystick, then we treat it as a gamepad
    uint32_t gamepad_cod = UNI_BT_COD_MINOR_GAMEPAD | UNI_BT_COD_MINOR_JOYSTICK;
    return (d->cod & UNI_BT_COD_MAJOR_PERIPHERAL) && (d->cod & gamepad_cod);
}

// Helpers

static void misc_button_enable_callback(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = btstack_run_loop_get_timer_context(ts);
    d->misc_button_wait_delay &= ~MISC_BUTTON_SYSTEM;
}

// process_mic_button_system swaps joystick port A and B only if there is one device attached.
static void process_misc_button_system(uni_hid_device_t* d) {
    if ((d->controller.gamepad.misc_buttons & MISC_BUTTON_SYSTEM) == 0) {
        // System button released ?
        d->misc_button_wait_release &= ~MISC_BUTTON_SYSTEM;
        return;
    }

    if (d->misc_button_wait_release & MISC_BUTTON_SYSTEM)
        return;

    // Needed only for Nintendo Switch family of controllers.
    // This is because each time you press the "system" button it generates two events
    // automatically:  press button + release button
    // We artificially add a delay.
    bool requires_delay = (d->controller_type == CONTROLLER_TYPE_SwitchProController ||
                           d->controller_type == CONTROLLER_TYPE_SwitchJoyConLeft || CONTROLLER_TYPE_SwitchJoyConRight);

    if (requires_delay && (d->misc_button_wait_delay & MISC_BUTTON_SYSTEM))
        return;

    d->misc_button_wait_release |= MISC_BUTTON_SYSTEM;

    uni_get_platform()->on_oob_event(UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON, d);

    if (requires_delay) {
        d->misc_button_wait_delay |= MISC_BUTTON_SYSTEM;
        btstack_run_loop_set_timer_context(&d->misc_button_delay_timer, d);
        btstack_run_loop_set_timer_handler(&d->misc_button_delay_timer, &misc_button_enable_callback);
        btstack_run_loop_set_timer(&d->misc_button_delay_timer, MISC_BUTTON_DELAY_MS);
        btstack_run_loop_add_timer(&d->misc_button_delay_timer);
    }
}

// process_misc_button_home dumps uni_hid_device debug info in the console.
static void process_misc_button_home(uni_hid_device_t* d) {
    if ((d->controller.gamepad.misc_buttons & MISC_BUTTON_HOME) == 0) {
        // Home button released ? Clear "wait" flag.
        d->misc_button_wait_release &= ~MISC_BUTTON_HOME;
        return;
    }

    // "Wait" flag present? Return.
    if (d->misc_button_wait_release & MISC_BUTTON_HOME)
        return;

    // Update "wait" flag.
    d->misc_button_wait_release |= MISC_BUTTON_HOME;

    uni_hid_device_dump_all();
}

static void device_connection_timeout(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = btstack_run_loop_get_timer_context(ts);

    if (d->conn.state == UNI_BT_CONN_STATE_DEVICE_READY) {
        // Nothing. Device is ready. Good.
        return;
    }
    logi("Device cannot connect in time, deleting:\n");
    uni_hid_device_dump_device(d);

    uni_hid_device_disconnect(d);
    uni_hid_device_delete(d);
    /* 'd'' is destroyed after this call, don't use it */
}

static void start_connection_timeout(uni_hid_device_t* d) {
    btstack_run_loop_set_timer_context(&d->connection_timer, d);
    btstack_run_loop_set_timer_handler(&d->connection_timer, &device_connection_timeout);
    btstack_run_loop_set_timer(&d->connection_timer, HID_DEVICE_CONNECTION_TIMEOUT_MS);
    btstack_run_loop_add_timer(&d->connection_timer);
}
