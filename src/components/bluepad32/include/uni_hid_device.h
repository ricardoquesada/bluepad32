// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_HID_DEVICE_H
#define UNI_HID_DEVICE_H

/****************************************************************************
 * @file
 * @brief Manages the HID devices.
 *
 * This file is responsible for creating, deleting, finding and updating HID
 * devices.
 *
 * A HID device can be a gamepad, a mouse, a keyboard, or any other HID-compliant
 * device.
 *
 * A HID device can be "physical" or "virtual". A physical device is a real
 * Bluetooth device. A virtual device is one that is created programmatically.
 * For example, a DualShock4 controller is a physical device that exposes a
 * gamepad and a mouse. The gamepad is the main device, and the mouse is a
 * virtual device.
 ****************************************************************************/

#include <btstack.h>
#include <stdbool.h>
#include <stdint.h>

#include "bt/uni_bt_conn.h"
#include "controller/uni_controller.h"
#include "controller/uni_controller_type.h"
#include "parser/uni_hid_parser.h"
#include "uni_circular_buffer.h"
#include "uni_error.h"

#define HID_MAX_NAME_LEN 240              ///< Max HID device name length.
#define HID_MAX_DESCRIPTOR_LEN 512        ///< Max HID descriptor length.
#define HID_DEVICE_MAX_PARSER_DATA 256    ///< Max size for parser-specific data.
#define HID_DEVICE_MAX_PLATFORM_DATA 256  ///< Max size for platform-specific data.

/**
 * @brief Allow HID devices that don't expose the Device Information Service (DIS, 0x180A).
 *
 * Set to 1 to accept devices without DIS and continue to HID.
 * Set to 0 to reject devices without DIS.
 */
#define UNI_HID_DEVICE_ALLOW_NO_DIS 1

/**
 * @brief HID_DEVICE_CONNECTION_TIMEOUT_MS includes the time from when the device is created until it is ready.
 */
#define HID_DEVICE_CONNECTION_TIMEOUT_MS 20000

/**
 * @brief Different SDP query types.
 *
 * Depending on the controller, the SDP query should be done before or after
 * the L2CAP connection.
 */
typedef enum {
    SDP_QUERY_AFTER_CONNECT,  /**< If not set, this is the default one. */
    SDP_QUERY_BEFORE_CONNECT, /**< Special case for DualShock4 1st generation. */
    SDP_QUERY_NOT_NEEDED,     /**< Because the Controller type was inferred by other means. */
} uni_sdp_query_type_t;

/**
 * @brief Represents a HID device.
 *
 * A HID device can be a gamepad, a mouse, a keyboard, or any other HID-compliant
 * device.
 *
 * A HID device can be "physical" or "virtual". A physical device is a real
 * Bluetooth device. A virtual device is one that is created programmatically.
 * For example, a DualShock4 controller is a physical device that exposes a
 * gamepad and a mouse. The gamepad is the main device, and the mouse is a
 * virtual device.
 */
struct uni_hid_device_s {
    uint32_t cod;                 ///< Class of Device.
    uint16_t vendor_id;           ///< Vendor ID.
    uint16_t product_id;          ///< Product ID.
    char name[HID_MAX_NAME_LEN];  ///< Device name.

    uint32_t flags;  ///< Device flags.

    /**
     * @brief Will abort connection if the connection was not established after timeout.
     */
    btstack_timer_source_t connection_timer;
    /**
     * @brief Max amount of time to wait to get the device name.
     */
    btstack_timer_source_t inquiry_remote_name_timer;

    // SDP
    uint8_t hid_descriptor[HID_MAX_DESCRIPTOR_LEN];  ///< HID descriptor.
    uint16_t hid_descriptor_len;                     ///< HID descriptor length.
    /**
     * @brief When to perform the SDP query.
     *
     * DualShock4 1st gen requires to do the SDP query before l2cap connect,
     * otherwise it won't work.
     * And Nintendo Switch Pro gamepad requires to do the SDP query after l2cap
     * connect, so we use this variable to determine when to do the SDP query.
     * TODO: Actually this is not entirely true since it works Ok when using
     * Unijoysticle + BTstack + libusb in Linux. The correct thing to do is to
     * debug the Linux connection and see what packets are sent before the
     * connection.
     */
    uni_sdp_query_type_t sdp_query_type;

    // Channels
    uint16_t hids_cid;  ///< BLE only: HID service channel ID.

    // TODO: Create a union of gamepad/mouse/keyboard structs
    // At the moment "mouse" reuses gamepad struct, but it is a hack.
    uni_controller_type_t controller_type;        ///< type of controller. E.g: DualShock4, Switch, etc.
    uni_controller_subtype_t controller_subtype;  ///< sub-type of controller attached, used for Wii mostly
    uni_controller_t controller;                  ///< Controller data (gamepad, mouse, etc.)

    uni_report_parser_t report_parser;  ///< Function used to parse the HID reports.

    uint32_t misc_button_wait_release;  ///< Buttons that need to be released before triggering the action again.
    uint32_t misc_button_wait_delay;    ///< Buttons that need to wait for a delay before triggering the action again.
    /**
     * @brief Needed for Nintendo Switch family of controllers.
     */
    btstack_timer_source_t misc_button_delay_timer;

    /**
     * @brief Circular buffer that contains the outgoing packets that couldn't be sent
     * immediately.
     */
    uni_circular_buffer_t outgoing_buffer;

    /**
     * @brief Bytes reserved to controller's parser instances.
     * E.g.: The Wii driver uses it for the state machine.
     */
    uint8_t parser_data[HID_DEVICE_MAX_PARSER_DATA];

    /**
     * @brief Bytes reserved to different platforms.
     * E.g.: C64 or Airlift might use it to store different values.
     */
    uint8_t platform_data[HID_DEVICE_MAX_PLATFORM_DATA];

    uni_bt_conn_t conn;  ///< Bluetooth connection info.

    /**
     * @brief Link to parent device. Used only when the device is a "virtual child".
     * Safe to assume that when parent != NULL, then it is a "virtual" device.
     * For example, the mouse implemented by DualShock4 has the "gamepad" as parent.
     */
    struct uni_hid_device_s* parent;
    /**
     * @brief When a physical controller has a child, like a "virtual device"
     * For example, DualShock4 has the "mouse" as a child.
     */
    struct uni_hid_device_s* child;
};
typedef struct uni_hid_device_s uni_hid_device_t;

/**
 * @brief Callback function used as in get_instance_with_predicate
 */
typedef uint8_t (*uni_hid_device_predicate_t)(uni_hid_device_t* d, void* data);

/**
 * @brief Setups the HID device module.
 *
 * Should be called only once.
 */
void uni_hid_device_setup(void);

/**
 * @brief Creates a new HID device.
 *
 * @param address The Bluetooth address of the device.
 * @return A new HID device, or NULL if it could not be created.
 */
uni_hid_device_t* uni_hid_device_create(bd_addr_t address);

/**
 * @brief Creates a new virtual HID device.
 *
 * Used for controllers that implement two input devices like DualShock4, which is a gamepad and a mouse
 * at the same time. The mouse will be the "virtual" device in this case.
 *
 * @param parent The parent device.
 * @return A new virtual HID device, or NULL if it could not be created.
 */
uni_hid_device_t* uni_hid_device_create_virtual(uni_hid_device_t* parent);

// Don't add any other get_instance_for_XXX function.
// Instead use: get_instance_with_predicate()
/**
 * @brief Returns the HID device associated with the given Bluetooth address.
 * @param addr The Bluetooth address.
 * @return The HID device, or NULL if not found.
 */
uni_hid_device_t* uni_hid_device_get_instance_for_address(bd_addr_t addr);

/**
 * @brief Returns the HID device associated with the given L2CAP CID.
 * @param cid The L2CAP CID.
 * @return The HID device, or NULL if not found.
 */
uni_hid_device_t* uni_hid_device_get_instance_for_cid(uint16_t cid);

/**
 * @brief Returns the HID device associated with the given HID service CID (BLE only).
 * @param cid The HID service CID.
 * @return The HID device, or NULL if not found.
 */
uni_hid_device_t* uni_hid_device_get_instance_for_hids_cid(uint16_t cid);

/**
 * @brief Returns the HID device associated with the given HCI connection handle.
 * @param handle The HCI connection handle.
 * @return The HID device, or NULL if not found.
 */
uni_hid_device_t* uni_hid_device_get_instance_for_connection_handle(hci_con_handle_t handle);

/**
 * @brief Returns the first HID device that is in the given state.
 * @param state The connection state.
 * @return The HID device, or NULL if not found.
 */
uni_hid_device_t* uni_hid_device_get_first_device_with_state(uni_bt_conn_state_t state);

/**
 * @brief Returns the HID device that matches the given predicate.
 * @param predicate The predicate function.
 * @param data The data to pass to the predicate function.
 * @return The HID device, or NULL if not found.
 */
uni_hid_device_t* uni_hid_device_get_instance_with_predicate(uni_hid_device_predicate_t predicate, void* data);

/**
 * @brief Returns the HID device at the given index.
 * @param idx The index.
 * @return The HID device, or NULL if not found.
 */
uni_hid_device_t* uni_hid_device_get_instance_for_idx(int32_t idx);

/**
 * @brief Returns the index of the given HID device.
 * @param d The HID device.
 * @return The index, or -1 if not found.
 */
int32_t uni_hid_device_get_idx_for_instance(const uni_hid_device_t* d);

/**
 * @brief Initializes a HID device.
 * @param d The HID device to initialize.
 */
void uni_hid_device_init(uni_hid_device_t* d);

/**
 * @brief Sets the HID device as ready.
 * @param d The HID device.
 */
void uni_hid_device_set_ready(uni_hid_device_t* d);

/**
 * @brief To be called when the device is ready.
 *
 * It notifies the platform that the device is ready.
 *
 * @param d The HID device.
 * @return true if the platform accepted the connection, false otherwise.
 */
bool uni_hid_device_set_ready_complete(uni_hid_device_t* d);

/**
 * @brief Requests to start a new inquiry.
 */
void uni_hid_device_request_inquire(void);

/**
 * @brief To be called when the device connection status changes.
 * @param d The HID device.
 * @param connected true if connected, false if disconnected.
 */
void uni_hid_device_on_connected(uni_hid_device_t* d, bool connected);

/**
 * @brief Connects to the given HID device.
 * @param d The HID device to connect to.
 */
void uni_hid_device_connect(uni_hid_device_t* d);

/** Restart the connect-to-ready watchdog (e.g. long GATT setup). */
void uni_hid_device_kick_connection_timeout(uni_hid_device_t* d);
void uni_hid_device_disconnect(uni_hid_device_t* d);

/**
 * @brief Deletes the given HID device.
 * @param d The HID device to delete.
 */
void uni_hid_device_delete(uni_hid_device_t* d);

/**
 * @brief Sets the Class of Device (CoD) for the given HID device.
 * @param d The HID device.
 * @param cod The Class of Device.
 */
void uni_hid_device_set_cod(uni_hid_device_t* d, uint32_t cod);

/**
 * @brief Returns whether the given Class of Device (CoD) is supported.
 * @param cod The Class of Device.
 * @return true if supported, false otherwise.
 */
bool uni_hid_device_is_cod_supported(uint32_t cod);

/**
 * @brief A new device has been discovered while scanning.
 * @param addr the Bluetooth address
 * @param name could be NULL, could be zero-length, or might contain the name.
 * @param cod Class of Device. See "uni_bt_defines.h" for possible values.
 * @param rssi Received Signal Strength Indicator (RSSI) measured in dBms. The higher (255) the better.
 * @returns UNI_ERROR_SUCCESS if a connection to the device should be established.
 */
uni_error_t uni_hid_device_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi);

/**
 * @brief Sets the HID descriptor for the given HID device.
 * @param d The HID device.
 * @param descriptor The HID descriptor.
 * @param len The length of the HID descriptor.
 */
void uni_hid_device_set_hid_descriptor(uni_hid_device_t* d, const uint8_t* descriptor, uint16_t len);

/**
 * @brief Returns whether the given HID device has a HID descriptor.
 * @param d The HID device.
 * @return true if it has a HID descriptor, false otherwise.
 */
bool uni_hid_device_has_hid_descriptor(const uni_hid_device_t* d);

/**
 * @brief Sets whether the connection is incoming or outgoing.
 * @param d The HID device.
 * @param incoming true if incoming, false if outgoing.
 */
void uni_hid_device_set_incoming(uni_hid_device_t* d, bool incoming);

/**
 * @brief Returns whether the connection is incoming or outgoing.
 * @param d The HID device.
 * @return true if incoming, false if outgoing.
 */
bool uni_hid_device_is_incoming(const uni_hid_device_t* d);

/**
 * @brief Sets the name for the given HID device.
 * @param d The HID device.
 * @param name The name.
 */
void uni_hid_device_set_name(uni_hid_device_t* d, const char* name);

/**
 * @brief Returns whether the given HID device has a name.
 * @param d The HID device.
 * @return true if it has a name, false otherwise.
 */
bool uni_hid_device_has_name(const uni_hid_device_t* d);

/**
 * @brief Sets the product ID for the given HID device.
 * @param d The HID device.
 * @param product_id The product ID.
 */
void uni_hid_device_set_product_id(uni_hid_device_t* d, uint16_t product_id);

/**
 * @brief Returns the product ID of the given HID device.
 * @param d The HID device.
 * @return The product ID.
 */
uint16_t uni_hid_device_get_product_id(const uni_hid_device_t* d);

/**
 * @brief Sets the vendor ID for the given HID device.
 * @param d The HID device.
 * @param vendor_id The vendor ID.
 */
void uni_hid_device_set_vendor_id(uni_hid_device_t* d, uint16_t vendor_id);

/**
 * @brief Returns the vendor ID of the given HID device.
 * @param d The HID device.
 * @return The vendor ID.
 */
uint16_t uni_hid_device_get_vendor_id(const uni_hid_device_t* d);

/**
 * @brief Dumps the state of the given HID device to the console.
 * @param d The HID device.
 */
void uni_hid_device_dump_device(uni_hid_device_t* d);

/**
 * @brief Dumps the state of all HID devices to the console.
 */
void uni_hid_device_dump_all(void);

/**
 * @brief Guesses the controller type from the device name.
 * @param d The HID device.
 * @param name The device name.
 * @return true if the controller type was guessed, false otherwise.
 */
bool uni_hid_device_guess_controller_type_from_name(uni_hid_device_t* d, const char* name);

/**
 * @brief Guesses the controller type from the vendor and product IDs.
 * @param d The HID device.
 */
void uni_hid_device_guess_controller_type_from_pid_vid(uni_hid_device_t* d);

/**
 * @brief Returns whether the given HID device has a controller type.
 * @param d The HID device.
 * @return true if it has a controller type, false otherwise.
 */
bool uni_hid_device_has_controller_type(const uni_hid_device_t* d);

/**
 * @brief Processes the controller data.
 *
 * To be called from the main loop.
 * @param d The HID device.
 */
void uni_hid_device_process_controller(uni_hid_device_t* d);

/**
 * @brief Sets the HCI connection handle for the given HID device.
 * @param d The HID device.
 * @param handle The HCI connection handle.
 */
void uni_hid_device_set_connection_handle(uni_hid_device_t* d, hci_con_handle_t handle);

/**
 * @brief Sends a report to the given HID device.
 * @param d The HID device.
 * @param cid The L2CAP CID.
 * @param report The report to send.
 * @param len The length of the report.
 */
void uni_hid_device_send_report(uni_hid_device_t* d, uint16_t cid, const uint8_t* report, uint16_t len);

/**
 * @brief Sends an interrupt report to the given HID device.
 * @param d The HID device.
 * @param report The report to send.
 * @param len The length of the report.
 */
void uni_hid_device_send_intr_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len);

/**
 * @brief Sends a control report to the given HID device.
 * @param d The HID device.
 * @param report The report to send.
 * @param len The length of the report.
 */
void uni_hid_device_send_ctrl_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len);

/**
 * @brief Sends any queued reports to the given HID device.
 * @param d The HID device.
 */
void uni_hid_device_send_queued_reports(uni_hid_device_t* d);

/**
 * @brief Returns whether the given HID device requires a HID descriptor.
 * @param d The HID device.
 * @return true if it requires a HID descriptor, false otherwise.
 */
bool uni_hid_device_does_require_hid_descriptor(const uni_hid_device_t* d);

/**
 * @brief Returns whether the given HID device is a gamepad.
 * @param d The HID device.
 * @return true if it is a gamepad, false otherwise.
 */
bool uni_hid_device_is_gamepad(const uni_hid_device_t* d);

/**
 * @brief Returns whether the given HID device is a mouse.
 * @param d The HID device.
 * @return true if it is a mouse, false otherwise.
 */
bool uni_hid_device_is_mouse(const uni_hid_device_t* d);

/**
 * @brief Returns whether the given HID device is a keyboard.
 * @param d The HID device.
 * @return true if it is a keyboard, false otherwise.
 */
bool uni_hid_device_is_keyboard(const uni_hid_device_t* d);

/**
 * @brief Returns whether the given HID device is a virtual device.
 * @param d The HID device.
 * @return true if it is a virtual device, false otherwise.
 */
bool uni_hid_device_is_virtual_device(const uni_hid_device_t* d);

#endif  // UNI_HID_DEVICE_H
