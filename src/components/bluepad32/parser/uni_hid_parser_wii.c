// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

// Technical info taken from:
// http://wiibrew.org/wiki/Wiimote
// https://github.com/dvdhrm/xwiimote/blob/master/doc/PROTOCOL

#include <assert.h>

#define ENABLE_EEPROM_DUMP 0

#if ENABLE_EEPROM_DUMP
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // ENABLE_EEPROM_DUMP

#include "parser/uni_hid_parser_wii.h"

#include "controller/uni_controller.h"
#include "hid_usage.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

#if ENABLE_EEPROM_DUMP
#define WII_DUMP_ROM_DATA_SIZE 16  // Max size is 16
static const uint32_t WII_DUMP_ROM_DATA_ADDR_START = 0x0000;
static const uint32_t WII_DUMP_ROM_DATA_ADDR_END = 0x1700;
#endif  // ENABLE_EEPROM_DUMP

#define DRM_KEE_BATTERY_MASK GENMASK(6, 4)

// Taken from Linux kernel: hid-wiimote.h
enum wiiproto_reqs {
    WIIPROTO_REQ_NULL = 0x0,
    WIIPROTO_REQ_RUMBLE = 0x10,
    WIIPROTO_REQ_LED = 0x11,
    WIIPROTO_REQ_DRM = 0x12,
    WIIPROTO_REQ_IR1 = 0x13,
    WIIPROTO_REQ_SREQ = 0x15,
    WIIPROTO_REQ_WMEM = 0x16,
    WIIPROTO_REQ_RMEM = 0x17,
    WIIPROTO_REQ_IR2 = 0x1a,
    WIIPROTO_REQ_STATUS = 0x20,
    WIIPROTO_REQ_DATA = 0x21,
    WIIPROTO_REQ_RETURN = 0x22,

    WIIPROTO_REQ_DRM_K = 0x30,
    WIIPROTO_REQ_DRM_KA = 0x31,
    WIIPROTO_REQ_DRM_KE = 0x32,
    WIIPROTO_REQ_DRM_KAI = 0x33,
    WIIPROTO_REQ_DRM_KEE = 0x34,
    WIIPROTO_REQ_DRM_KAE = 0x35,
    WIIPROTO_REQ_DRM_KIE = 0x36,
    WIIPROTO_REQ_DRM_KAIE = 0x37,
    WIIPROTO_REQ_DRM_E = 0x3d,
    WIIPROTO_REQ_DRM_SKAI1 = 0x3e,
    WIIPROTO_REQ_DRM_SKAI2 = 0x3f,

    WIIPROTO_REQ_MAX
};

// Supported Wii devices
enum wii_devtype {
    WII_DEVTYPE_UNK,
    WII_DEVTYPE_PRO_CONTROLLER,  // Wii U Pro controller
    WII_DEVTYPE_REMOTE,          // 1st gen
    WII_DEVTYPE_REMOTE_MP,       // MP: motion plus (wii remote 2nd gen)
};

enum wii_exttype {
    WII_EXT_NONE,                // No extensions detected
    WII_EXT_UNK,                 // Unknown extension
    WII_EXT_NUNCHUK,             // Nunchuk
    WII_EXT_CLASSIC_CONTROLLER,  // Classic Controller or Classic Controller Pro
    WII_EXT_U_PRO_CONTROLLER,    // Wii U Pro
    WII_EXT_BALANCE_BOARD        // Balance Board
};

// Required steps to determine what kind of extensions are supported.
enum wii_fsm {
    WII_FSM_UNINIT,                   // Uninitialized
    WII_FSM_SETUP,                    // Setup
    WII_FSM_DUMP_EEPROM_IN_PROGRESS,  // EEPROM dump in progress
    WII_FSM_DUMP_EEPROM_FINISHED,     // EEPROM dump finished
    WII_FSM_DID_REQ_STATUS,           // Status requested
    WII_FSM_DEV_UNK,                  // Device unknown
    WII_FSM_EXT_UNK,                  // Extension unknown
    WII_FSM_EXT_DID_INIT,             // Extension initialized
    WII_FSM_EXT_DID_NO_ENCRYPTION,    // Extension no encryption
    WII_FSM_EXT_DID_READ_REGISTER,    // Extension read register
    WII_FSM_BALANCE_BOARD_READ_CALIBRATION,
    WII_FSM_BALANCE_BOARD_READ_CALIBRATION2,
    WII_FSM_BALANCE_BOARD_DID_READ_CALIBRATION,
    WII_FSM_BALANCE_BOARD_DID_READ_CALIBRATION2,
    WII_FSM_DEV_GUESSED,   // Device type guessed
    WII_FSM_DEV_ASSIGNED,  // Device type assigned
    WII_FSM_LED_UPDATED,   // After a device was assigned, update LEDs.
                           // Gamepad ready to be used
};

typedef enum {
    WII_STATE_RUMBLE_DISABLED,
    WII_STATE_RUMBLE_DELAYED,
    WII_STATE_RUMBLE_IN_PROGRESS,
} wii_state_rumble_t;

// As defined here: http://wiibrew.org/wiki/Wiimote#0x21:_Read_Memory_Data
typedef enum wii_read_type {
    WII_READ_FROM_MEM = 0,
    WII_READ_FROM_REGISTERS = 0x04,
} wii_read_type_t;

// nunchuk_t represents the data provided by the Nunchuk.
typedef struct nunchuk_s {
    int sx;   // Analog stick X
    int sy;   // Analog stick Y
    int ax;   // Accelerometer X
    int ay;   // Accelerometer Y
    int az;   // Accelerometer Z
    bool bc;  // Button C
    bool bz;  // Button Z
} nunchuk_t;

// balance_board_t represents the data provided by the Balance Board.
typedef struct balance_board_s {
    uint16_t tr;      // Top right
    uint16_t br;      // Bottom right
    uint16_t tl;      // Top left
    uint16_t bl;      // Bottom left
    int temperature;  // Temperature
    uint8_t battery;  // Battery level
} balance_board_t;

// balance_board_calibration_s represents the calibration data that is used
// to convert read data to a mass measurement.
typedef struct balance_board_calibration_s {
    balance_board_t kg0;   // Calibration data at 0 kg
    balance_board_t kg17;  // Calibration data at 17 kg
    balance_board_t kg34;  // Calibration data at 34 kg
} balance_board_calibration_t;

// wii_instance_t represents data used by the Wii driver instance.
typedef struct wii_instance_s {
    uint8_t state;
    uint8_t register_address;
    wii_mode_t mode; /* horizontal, accel, vertical, rumble, etc.. */
    enum wii_devtype dev_type;
    enum wii_exttype ext_type;
    uni_gamepad_seat_t gamepad_seat;

    // Although technically, we can use one timer for delay and duration, easier to debug/maintain if we have two.
    btstack_timer_source_t rumble_timer_duration;
    btstack_timer_source_t rumble_timer_delayed_start;
    wii_state_rumble_t rumble_state;

    // Used by delayed start
    uint16_t rumble_duration_ms;

    balance_board_calibration_t balance_board_calibration;

    // Debug only
    int debug_fd;         // File descriptor where dump is saved
    uint32_t debug_addr;  // Current dump address
} wii_instance_t;
_Static_assert(sizeof(wii_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "Wii instance too big");

static void process_req_status(uni_hid_device_t* d, const uint8_t* report, uint16_t len);
static void process_req_data(uni_hid_device_t* d, const uint8_t* report, uint16_t len);
static void process_req_return(uni_hid_device_t* d, const uint8_t* report, uint16_t len);
static void process_drm_k(uni_hid_device_t* d, const uint8_t* report, uint16_t len);
static void process_drm_k_vertical(uni_controller_t* ctl, const uint8_t* data);
static void process_drm_k_horizontal(uni_controller_t* ctl, const uint8_t* data);
static void process_drm_ka(uni_hid_device_t* d, const uint8_t* report, uint16_t len);
static void process_drm_ke(uni_hid_device_t* d, const uint8_t* report, uint16_t len);
static void process_drm_kae(uni_hid_device_t* d, const uint8_t* report, uint16_t len);
static void process_drm_kee(uni_hid_device_t* d, const uint8_t* report, uint16_t len);
static void process_drm_e(uni_hid_device_t* d, const uint8_t* report, uint16_t len);
static nunchuk_t process_nunchuk(const uint8_t* e, uint16_t len);
static balance_board_t process_balance_board(uni_hid_device_t* d, const uint8_t* e, uint16_t len);

static void wii_process_fsm(uni_hid_device_t* d);
static void wii_fsm_ext_init(uni_hid_device_t* d);
static void wii_fsm_ext_encrypt_off(uni_hid_device_t* d);
static void wii_fsm_ext_read_register(uni_hid_device_t* d);
static void wii_fsm_req_status(uni_hid_device_t* d);
static void wii_fsm_assign_device(uni_hid_device_t* d);
static void wii_fsm_update_led(uni_hid_device_t* d);
static void wii_fsm_dump_eeprom(uni_hid_device_t* d);

static void wii_read_mem(uni_hid_device_t* d, wii_read_type_t t, uint32_t offset, uint16_t size);
static wii_instance_t* get_wii_instance(uni_hid_device_t* d);
static void wii_set_led(uni_hid_device_t* d, uni_gamepad_seat_t seat);
static void on_wii_set_rumble_on(btstack_timer_source_t* ts);
static void on_wii_set_rumble_off(btstack_timer_source_t* ts);
static void wii_play_dual_rumble_now(struct uni_hid_device_s* d, uint16_t duration_ms);

// Constants
static const char* wii_devtype_names[] = {
    "Unknown",                         // WII_DEVTYPE_UNK,
    "Wii Pro Controller",              // WII_DEVTYPE_PRO_CONTROLLER
    "Wii Mote 1st Gen",                // WII_DEVTYPE_REMOTE
    "Wii Mote Motion Plus (2nd gen)",  // WII_DEVTYPE_REMOTE_MP
};

static const char* wii_exttype_names[] = {
    "N/A",                 // WII_EXT_NONE
    "Unknown",             // WII_EXT_UNK
    "Nunchuk",             // WII_EXT_NUNCHUK
    "Classic Controller",  // WII_EXT_CLASSIC_CONTROLLER
    "Pro Controller",      // WII_EXT_U_PRO_CONTROLLER
    "Balance Board"        // WII_EXT_BALANCE_BOARD
};

// process_ functions

// Defined here: http://wiibrew.org/wiki/Wiimote#0x20:_Status
static void process_req_status(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (len < 7) {
        loge("Wii: Unexpected report length; got %d, want >= 7\n", len);
        return;
    }
    wii_instance_t* ins = get_wii_instance(d);
    uint8_t flags = report[3] & 0x0f;  // LF (leds / flags)
    if (ins->state == WII_FSM_DID_REQ_STATUS) {
        if (d->product_id == 0x0306) {
            // We are positive that this is a Wii Remote 1st gen
            ins->state = WII_FSM_DEV_GUESSED;
            ins->dev_type = WII_DEVTYPE_REMOTE;
        } else if (d->product_id == 0x0330) {
            // It can be either a Wii Remote 2nd gen or a Wii U Pro Controller
            if ((flags & 0x02) == 0) {
                // If there are no extensions, then we are sure it is a Wii Remote MP.
                ins->state = WII_FSM_DEV_GUESSED;
                ins->dev_type = WII_DEVTYPE_REMOTE_MP;
            } else {
                // Otherwise, it can be either a Wii Remote MP with a Nunchuk or a
                // Wii U Pro controller.
                ins->state = WII_FSM_DEV_UNK;
            }
        }

        if ((flags & 0x02) != 0) {
            // Extension detected: Nunchuk?
            // Regardless of the previous FSM state, we overwrite it with "query
            // extension".
            logi("Wii: extension found.\n");
            ins->state = WII_FSM_EXT_UNK;
            ins->ext_type = WII_EXT_UNK;
        } else {
            logi("Wii: No extensions found.\n");
            ins->ext_type = WII_EXT_NONE;
        }

        if (report[2] & 0x08) {
            // Wii Remote only: Enter "accel mode" if "A" is pressed.
            ins->mode = WII_MODE_ACCEL;
        } else if (report[1] & 0x10) {
            // Wii Remote only: Enter "vertical mode" if "+" is pressed.
            ins->mode = WII_MODE_VERTICAL;
        }

        wii_process_fsm(d);
    }
}

// Defined here: http://wiibrew.org/wiki/Wiimote#0x21:_Read_Memory_Data
static void process_req_data_read_register(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    uint8_t se = report[3];  // SE: size and error
    uint8_t s = se >> 4;     // size
    uint8_t e = se & 0x0f;   // error
    if (e) {
        loge("Wii: error reading memory: 0x%02x\n.", e);
        return;
    }

    wii_instance_t* ins = get_wii_instance(d);

    // We are expecting to read 6 bytes from 0xXX00fa
    if (s == 5 && report[4] == 0x00 && report[5] == 0xfa) {
        // This contains the read memory from register 0xa?00fa
        // Data is in report[6]..report[11]

        // Try to guess device type.
        if (report[10] == 0x01 && report[11] == 0x20) {
            // Pro Controller: 00 00 a4 20 01 20
            ins->dev_type = WII_DEVTYPE_PRO_CONTROLLER;
            ins->ext_type = WII_EXT_U_PRO_CONTROLLER;
        } else if (ins->dev_type == WII_DEVTYPE_UNK) {
            if (d->product_id == 0x0330) {
                ins->dev_type = WII_DEVTYPE_REMOTE_MP;
            } else if (d->product_id == 0x0306) {
                ins->dev_type = WII_DEVTYPE_REMOTE;
            } else {
                loge("Wii: Unknown product id: 0x%04x\n", d->product_id);
            }
        }

        // Try to guess extension type.
        if (ins->ext_type == WII_EXT_UNK) {
            if (report[10] == 0x00 && report[11] == 0x00) {
                // Nunchuck: 00 00 a4 20 00 00
                ins->ext_type = WII_EXT_NUNCHUK;
                // If a Nunchuck is attached, WiiMode is treated as vertical mode
                ins->mode = WII_MODE_VERTICAL;
            } else if (report[10] == 0x04 && report[11] == 0x02) {
                // Balance Board: 00 00 a4 20 04 02
                ins->ext_type = WII_EXT_BALANCE_BOARD;
            } else if (report[10] == 0x01 && report[11] == 0x01) {
                // Classic / Classic Pro: 0? 00 a4 20 01 01
                ins->ext_type = WII_EXT_CLASSIC_CONTROLLER;
            } else {
                loge("Wii: Unknown extension\n");
                printf_hexdump(report, len);
            }
        }

        if (ins->ext_type == WII_EXT_BALANCE_BOARD) {
            ins->state = WII_FSM_BALANCE_BOARD_READ_CALIBRATION;
        } else {
            ins->state = WII_FSM_DEV_GUESSED;
        }

        logi("Wii: Device: %s, Extension: %s\n", wii_devtype_names[ins->dev_type], wii_exttype_names[ins->ext_type]);
        wii_process_fsm(d);
    } else {
        loge("Wii: invalid response");
        printf_hexdump(report, len);
    }
}

static void process_req_data_read_calibration_data(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    ARG_UNUSED(len);
    uint8_t se = report[3];  // SE: size and error
    uint8_t s = se >> 4;     // size
    uint8_t e = se & 0x0f;   // error
    if (e) {
        loge("Wii: error reading memory: 0x%02x\n.", e);
        return;
    }

    wii_instance_t* ins = get_wii_instance(d);

    // We are expecting to read 16 bytes from 0xXX0024
    if (s == 15 && report[4] == 0x00 && report[5] == 0x24) {
        logi("Wii: balance board read calibration\n");
        const uint8_t* cal = report + 6;
        ins->balance_board_calibration.kg0.tr = (cal[0] << 8) + cal[1];  // Top Right 0kg
        ins->balance_board_calibration.kg0.br = (cal[2] << 8) + cal[3];  // Bottom Right 0kg
        ins->balance_board_calibration.kg0.tl = (cal[4] << 8) + cal[5];  // Top Left 0kg
        ins->balance_board_calibration.kg0.bl = (cal[6] << 8) + cal[7];  // Bottom Left 0kg

        ins->balance_board_calibration.kg17.tr = (cal[8] << 8) + cal[9];    // Top Right 17kg
        ins->balance_board_calibration.kg17.br = (cal[10] << 8) + cal[11];  // Bottom Right 17kg
        ins->balance_board_calibration.kg17.tl = (cal[12] << 8) + cal[13];  // Top Left 17kg
        ins->balance_board_calibration.kg17.bl = (cal[14] << 8) + cal[15];  // Bottom Left 17kg
    }

    ins->state = WII_FSM_BALANCE_BOARD_READ_CALIBRATION2;
    wii_process_fsm(d);
}

static void process_req_data_read_calibration_data2(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    ARG_UNUSED(len);
    uint8_t se = report[3];  // SE: size and error
    uint8_t s = se >> 4;     // size
    uint8_t e = se & 0x0f;   // error
    if (e) {
        loge("Wii: error reading memory: 0x%02x\n.", e);
        return;
    }

    wii_instance_t* ins = get_wii_instance(d);

    // We are expecting to read 8 bytes from 0xXX0034
    if (s == 7 && report[4] == 0x00 && report[5] == 0x34) {
        logi("Wii: balance board read calibration 2\n");
        const uint8_t* cal = report + 6;
        ins->balance_board_calibration.kg34.tr = (cal[0] << 8) + cal[1];  // Top Right 34kg
        ins->balance_board_calibration.kg34.br = (cal[2] << 8) + cal[3];  // Bottom Right 34kg
        ins->balance_board_calibration.kg34.tl = (cal[4] << 8) + cal[5];  // Top Left 34kg
        ins->balance_board_calibration.kg34.bl = (cal[6] << 8) + cal[7];  // Bottom Left 34kg
    }

    logi("Wii: Balance Board calibration: kg0=%d,%d,%d,%d kg17=%d,%d,%d,%d kg35=%d,%d,%d,%d\n",
         ins->balance_board_calibration.kg0.tr, ins->balance_board_calibration.kg0.br,
         ins->balance_board_calibration.kg0.tl, ins->balance_board_calibration.kg0.bl,
         ins->balance_board_calibration.kg17.tr, ins->balance_board_calibration.kg17.br,
         ins->balance_board_calibration.kg17.tl, ins->balance_board_calibration.kg17.bl,
         ins->balance_board_calibration.kg34.tr, ins->balance_board_calibration.kg34.br,
         ins->balance_board_calibration.kg34.tl, ins->balance_board_calibration.kg34.bl);

    ins->state = WII_FSM_DEV_GUESSED;
    wii_process_fsm(d);
}

// Returns the calibrated weight in grams.
static int32_t balance_interpolate(uint16_t val, uint16_t kg0, uint16_t kg17, uint16_t kg34) {
    float weight = 0;

    // Each sensor can read up to 34kg, at least in theory.
    // It seems that it supports a bit more that's why we don't cap it to 34.
    if (val < kg0) {
        weight = 0;
    } else if (val < kg17) {
        weight = 17 * (float)(val - kg0) / (float)(kg17 - kg0);
    } else /* if (val < kg34) */ {
        weight = 17 + 17 * (float)(val - kg17) / (float)(kg34 - kg17);
    }

    return weight * 1000;
}

static void process_req_data_dump_eeprom(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    ARG_UNUSED(len);
#if ENABLE_EEPROM_DUMP
    uint8_t se = report[3];     // SE: size and error
    uint8_t s = (se >> 4) + 1;  // size
    uint8_t e = se & 0x0f;      // error
    if (e) {
        loge("Wii: error reading memory: 0x%02x\n.", e);
        return;
    }

    wii_instance_t* ins = get_wii_instance(d);
    uint16_t addr = report[4] << 8 | report[5];

    logi("Wii: dumping %d bytes at address: 0x%04x\n", s, addr);
    write(ins->debug_fd, &report[6], s);
#else
    ARG_UNUSED(d);
    ARG_UNUSED(report);
#endif  // ENABLE_EEPROM_DUMP
    wii_process_fsm(d);
}

// Defined here: http://wiibrew.org/wiki/Wiimote#0x21:_Read_Memory_Data
static void process_req_data(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    logi("**** process_req_data\n");
    printf_hexdump(report, len);

    if (len < 22) {
        loge("Wii: invalid req_data length: got %d, want >= 22\n", len);
        printf_hexdump(report, len);
        return;
    }

    wii_instance_t* ins = get_wii_instance(d);
    switch (ins->state) {
        case WII_FSM_EXT_DID_READ_REGISTER:
            process_req_data_read_register(d, report, len);
            break;
        case WII_FSM_BALANCE_BOARD_DID_READ_CALIBRATION:
            process_req_data_read_calibration_data(d, report, len);
            break;
        case WII_FSM_BALANCE_BOARD_DID_READ_CALIBRATION2:
            process_req_data_read_calibration_data2(d, report, len);
            break;
        case WII_FSM_DUMP_EEPROM_IN_PROGRESS:
            process_req_data_dump_eeprom(d, report, len);
            break;
        default:
            loge("process_req_data. Unknown FSM state: 0x%02x\n", ins->state);
            break;
    }
}

// Defined here:
// http://wiibrew.org/wiki/Wiimote#0x22:_Acknowledge_output_report.2C_return_function_result
static void process_req_return(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (len < 5) {
        loge("Invalid len report for process_req_return: got %d, want >= 5\n", len);
    }
    if (report[3] == WIIPROTO_REQ_WMEM) {
        wii_instance_t* ins = get_wii_instance(d);
        // Status != 0: Error. Probably invalid register
        if (report[4] != 0) {
            if (ins->register_address == 0xa6) {
                loge("Failed to read registers from 0xa6... mmmm\n");
                ins->state = WII_FSM_SETUP;
            } else {
                // If it failed to read registers with 0xa4, then try with 0xa6.
                // If 0xa6 works Ok, it is safe to assume it is a Wii Remote MP, but
                // for the sake of finishing the "read extension" (might be useful in the future),
                // we continue with it.
                logi("Probably a Remote MP device. Switching to 0xa60000 address for registers.\n");
                ins->state = WII_FSM_DEV_UNK;
                ins->register_address = 0xa6;  // Register address used for Wii Remote MP.
            }
        } else {
            // Status Ok. Good
        }
        wii_process_fsm(d);
    }
}

// Used for WiiMote in Sideways and Vertical Mode.
// Defined here: http://wiibrew.org/wiki/Wiimote#0x30:_Core_Buttons
static void process_drm_k(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    /* DRM_K: BB*2 */
    // Expecting something like:
    // 30 00 08
    if (len < 3) {
        loge("wii remote drm_k: invalid report len %d\n", len);
        return;
    }

    uni_controller_t* ctl = &d->controller;
    const uint8_t* data = &report[1];
    wii_instance_t* ins = get_wii_instance(d);

    switch (ins->mode) {
        case WII_MODE_HORIZONTAL:
            process_drm_k_horizontal(ctl, data);
            break;
        case WII_MODE_VERTICAL:
            process_drm_k_vertical(ctl, data);
            break;
        default:
            break;
    }
    // Process misc buttons
    ctl->gamepad.misc_buttons |= (data[1] & 0x80) ? MISC_BUTTON_SYSTEM : 0;  // Button "home"
    ctl->gamepad.misc_buttons |= (data[0] & 0x10) ? MISC_BUTTON_START : 0;   // Button "+"
    ctl->gamepad.misc_buttons |= (data[1] & 0x10) ? MISC_BUTTON_SELECT : 0;  // Button "-"
}

// Used for WiiMote in Sideways Mode (Directions and A/B/X/Y Buttons only).
static void process_drm_k_horizontal(uni_controller_t* ctl, const uint8_t* data) {
    // dpad
    ctl->gamepad.dpad |= (data[0] & 0x01) ? DPAD_DOWN : 0;
    ctl->gamepad.dpad |= (data[0] & 0x02) ? DPAD_UP : 0;
    ctl->gamepad.dpad |= (data[0] & 0x04) ? DPAD_RIGHT : 0;
    ctl->gamepad.dpad |= (data[0] & 0x08) ? DPAD_LEFT : 0;

    // buttons
    ctl->gamepad.buttons |= (data[1] & 0x04) ? BUTTON_Y : 0;  // Shoulder button
    ctl->gamepad.buttons |= (data[1] & 0x08) ? BUTTON_X : 0;  // Big button "A"
    ctl->gamepad.buttons |= (data[1] & 0x02) ? BUTTON_A : 0;  // Button "1"
    ctl->gamepad.buttons |= (data[1] & 0x01) ? BUTTON_B : 0;  // Button "2"
}

// Used for WiiMote in Vertical Mode (Directions and A/B/X/Y Buttons only).
static void process_drm_k_vertical(uni_controller_t* ctl, const uint8_t* data) {
    // dpad
    ctl->gamepad.dpad |= (data[0] & 0x01) ? DPAD_LEFT : 0;
    ctl->gamepad.dpad |= (data[0] & 0x02) ? DPAD_RIGHT : 0;
    ctl->gamepad.dpad |= (data[0] & 0x04) ? DPAD_DOWN : 0;
    ctl->gamepad.dpad |= (data[0] & 0x08) ? DPAD_UP : 0;

    // buttons
    ctl->gamepad.buttons |= (data[1] & 0x04) ? BUTTON_A : 0;  // Shoulder button
    ctl->gamepad.buttons |= (data[1] & 0x08) ? BUTTON_B : 0;  // Big button "A"
    ctl->gamepad.buttons |= (data[1] & 0x02) ? BUTTON_X : 0;  // Button "1"
    ctl->gamepad.buttons |= (data[1] & 0x01) ? BUTTON_Y : 0;  // Button "2"
}

// Used for WiiMote in Accelerometer Mode. Defined here:
// http://wiibrew.org/wiki/Wiimote#0x31:_Core_Buttons_and_Accelerometer
static void process_drm_ka(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    // Process Wiimote in "accelerator mode".
    /* DRM_KA: BB*2 AA*3*/
    // Expecting something like:
    // 31 20 60 82 7F 99
    if (len < 6) {
        loge("wii remote drm_ka: invalid report len %d\n", len);
        return;
    }

    uint16_t x = (report[3] << 2) | ((report[1] >> 5) & 0x3);
    uint16_t y = (report[4] << 2) | ((report[2] >> 4) & 0x2);
    uint16_t z = (report[5] << 2) | ((report[2] >> 5) & 0x2);

    int16_t sx = x - 0x200;
    int16_t sy = y - 0x200;
    int16_t sz = z - 0x200;

    // printf_hexdump(report, len);
    // logi("Wii: x=%d, y=%d, z=%d\n", sx, sy, sz);

    uni_controller_t* ctl = &d->controller;

    ctl->gamepad.accel[0] = sx;
    ctl->gamepad.accel[1] = sy;
    ctl->gamepad.accel[2] = sz;

    // Dpad works as dpad, useful to navigate menus.
    ctl->gamepad.dpad |= (report[1] & 0x01) ? DPAD_DOWN : 0;
    ctl->gamepad.dpad |= (report[1] & 0x02) ? DPAD_UP : 0;
    ctl->gamepad.dpad |= (report[1] & 0x04) ? DPAD_RIGHT : 0;
    ctl->gamepad.dpad |= (report[1] & 0x08) ? DPAD_LEFT : 0;

    ctl->gamepad.buttons |= (report[2] & 0x02) ? BUTTON_A : 0;  // Button "1"
    ctl->gamepad.buttons |= (report[2] & 0x01) ? BUTTON_B : 0;  // Button "2"
    ctl->gamepad.buttons |= (report[2] & 0x08) ? BUTTON_X : 0;  // Big button "A"
    ctl->gamepad.buttons |= (report[2] & 0x04) ? BUTTON_Y : 0;  // Button Shoulder

    ctl->gamepad.misc_buttons |= (report[2] & 0x80) ? MISC_BUTTON_SYSTEM : 0;  // Button "home"
    ctl->gamepad.misc_buttons |= (report[2] & 0x10) ? MISC_BUTTON_SELECT : 0;  // Button "-"
    ctl->gamepad.misc_buttons |= (report[1] & 0x10) ? MISC_BUTTON_START : 0;   // Button "+"
}

// Used in WiiMote + Nunchuk Mode
// Defined here:
// http://wiibrew.org/wiki/Wiimote#0x32:_Core_Buttons_with_8_Extension_bytes
static void process_drm_ke(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    // Expecting something like:
    // 32 BB BB EE EE EE EE EE EE EE EE
    if (len < 11) {
        loge("Wii: unexpected len; got %d, want >= 11\n", len);
        return;
    }

    wii_instance_t* ins = get_wii_instance(d);
    if (ins->ext_type != WII_EXT_NUNCHUK) {
        loge("Wii: unexpected Wii extension: got %d, want: %d", ins->ext_type, WII_EXT_NUNCHUK);
        return;
    }

    if (ins->mode != WII_MODE_VERTICAL) {
        loge("Wii: When Nunchuk is attached, only vertical mode is supported. Found: %d\n", ins->mode);
        return;
    }

    //
    // Process Nunchuk: Right axis, buttons X and Y
    //
    nunchuk_t n = process_nunchuk(&report[3], len - 3);
    uni_controller_t* ctl = &d->controller;
    const int factor = (AXIS_NORMALIZE_RANGE / 2) / 128;

    ctl->gamepad.axis_rx = n.sx * factor;
    ctl->gamepad.axis_ry = n.sy * factor;
    ctl->gamepad.buttons |= n.bc ? BUTTON_X : 0;
    ctl->gamepad.buttons |= n.bz ? BUTTON_Y : 0;

    //
    // Process Wii remote: DPAD, buttons A, B, Shoulder L & R, and misc.
    //

    // dpad
    ctl->gamepad.dpad |= (report[1] & 0x01) ? DPAD_LEFT : 0;
    ctl->gamepad.dpad |= (report[1] & 0x02) ? DPAD_RIGHT : 0;
    ctl->gamepad.dpad |= (report[1] & 0x04) ? DPAD_DOWN : 0;
    ctl->gamepad.dpad |= (report[1] & 0x08) ? DPAD_UP : 0;

    ctl->gamepad.buttons |= (report[2] & 0x04) ? BUTTON_A : 0;  // Shoulder button
    ctl->gamepad.buttons |= (report[2] & 0x08) ? BUTTON_B : 0;  // Big button "A"

    ctl->gamepad.buttons |= (report[2] & 0x02) ? BUTTON_SHOULDER_L : 0;  // Button "1"
    ctl->gamepad.buttons |= (report[2] & 0x01) ? BUTTON_SHOULDER_R : 0;  // Button "2"

    ctl->gamepad.misc_buttons |= (report[2] & 0x80) ? MISC_BUTTON_SYSTEM : 0;  // Button "home"
    ctl->gamepad.misc_buttons |= (report[2] & 0x10) ? MISC_BUTTON_SELECT : 0;  // Button "-"
    ctl->gamepad.misc_buttons |= (report[1] & 0x10) ? MISC_BUTTON_START : 0;   // Button "+"
}

// Defined here:
// http://wiibrew.org/wiki/Wiimote#0x35:_Core_Buttons_and_Accelerometer_with_16_Extension_Bytes
static void process_drm_kae(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    // Expecting something like:
    // (a1) 35 BB BB AA AA AA EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE
    ARG_UNUSED(d);
    ARG_UNUSED(report);
    ARG_UNUSED(len);
    loge("Wii: drm_kae not supported yet\n");
}

static nunchuk_t process_nunchuk(const uint8_t* e, uint16_t len) {
    // Nunchuk format here:
    // http://wiibrew.org/wiki/Wiimote/Extension_Controllers/Nunchuck
    nunchuk_t n = {0};
    if (len < 6) {
        loge("Wii: unexpected len; got %d, want >= 6\n", len);
        return n;
    }
    n.sx = e[0] - 0x80;
    // Invert polarity to match virtual gamepad.
    n.sy = -(e[1] - 0x80);
    n.ax = (e[2] << 2) | ((e[5] & 0b00001100) >> 2);
    n.ay = (e[3] << 2) | ((e[5] & 0b00110000) >> 4);
    n.az = (e[4] << 2) | ((e[5] & 0b11000000) >> 6);
    n.ax -= AXIS_NORMALIZE_RANGE / 2;
    n.ay -= AXIS_NORMALIZE_RANGE / 2;
    n.az -= AXIS_NORMALIZE_RANGE / 2;
    n.bc = !(e[5] & 0b00000010);
    n.bz = !(e[5] & 0b00000001);
    return n;
}
static balance_board_t process_balance_board(uni_hid_device_t* d, const uint8_t* e, uint16_t len) {
    // Balance board format here:
    // http://wiibrew.org/wiki/Wii_Balance_Board
    balance_board_t b = {0};
    balance_board_t b2 = {0};

    if (len < 11) {
        loge("Wii: unexpected len; got %d, want >= 11\n", len);
        return b;
    }

    wii_instance_t* ins = get_wii_instance(d);
    balance_board_t* kg0 = &ins->balance_board_calibration.kg0;
    balance_board_t* kg17 = &ins->balance_board_calibration.kg17;
    balance_board_t* kg34 = &ins->balance_board_calibration.kg34;

    b.tr = (e[0] << 8) + e[1];
    b.br = (e[2] << 8) + e[3];
    b.tl = (e[4] << 8) + e[5];
    b.bl = (e[6] << 8) + e[7];
    b.temperature = e[8];
    // Values go from 0x69 (empty) to 0x82 (full)
    uint8_t batt = e[10];
    if (batt >= 0x82)
        batt = 255;
    else if (batt >= 0x7d)
        batt = 192;
    else if (batt >= 0x78)
        batt = 128;
    else if (batt >= 0x6a)
        batt = 64;
    else
        batt = 0;
    b.battery = batt;

    // Interpolate:
    b2 = b;
    b2.tr = balance_interpolate(b.tr, kg0->tr, kg17->tr, kg34->tr);
    b2.br = balance_interpolate(b.br, kg0->br, kg17->br, kg34->br);
    b2.tl = balance_interpolate(b.tl, kg0->tl, kg17->tl, kg34->tl);
    b2.bl = balance_interpolate(b.bl, kg0->bl, kg17->bl, kg34->bl);

    return b2;
}

// Used for the Wii U Pro Controller and Balance Board
// Defined here:
// http://wiibrew.org/wiki/Wiimote#0x34:_Core_Buttons_with_19_Extension_bytes
static void process_drm_kee(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    wii_instance_t* ins = get_wii_instance(d);

    if (ins->ext_type != WII_EXT_BALANCE_BOARD && ins->ext_type != WII_EXT_U_PRO_CONTROLLER) {
        loge("Wii: unexpected Wii extension: got %d, want: %d OR %d", ins->ext_type, WII_EXT_U_PRO_CONTROLLER,
             WII_EXT_BALANCE_BOARD);
        return;
    }

    if (ins->ext_type == WII_EXT_BALANCE_BOARD) {
        //
        // Process Balance Board
        //
        uni_controller_t* ctl = &d->controller;
        balance_board_t b = process_balance_board(d, &report[3], len - 3);
        ctl->balance_board.tr = b.tr;
        ctl->balance_board.br = b.br;
        ctl->balance_board.tl = b.tl;
        ctl->balance_board.bl = b.bl;
        ctl->balance_board.temperature = b.temperature;
        ctl->battery = b.battery;
        if (ctl->battery < UNI_CONTROLLER_BATTERY_EMPTY)
            ctl->battery = UNI_CONTROLLER_BATTERY_EMPTY;
        ctl->klass = UNI_CONTROLLER_CLASS_BALANCE_BOARD;

        return;
    }

    // else WII_EXT_U_PRO_CONTROLLER

    /* DRM_KEE: BB*2 EE*19 */
    // Expecting something like:
    // 34 00 00 19 08 D5 07 20 08 21 08 FF FF CF 00 00 00 00 00 00 00 00

    // Doc taken from hid-wiimote-modules.c from Linux Kernel

    /*   Byte |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |
     *   -----+-----+-----+-----+-----+-----+-----+-----+-----+
     *    0   |                   LX <7:0>                    |
     *   -----+-----------------------+-----------------------+
     *    1   |  0     0     0     0  |       LX <11:8>       |
     *   -----+-----------------------+-----------------------+
     *    2   |                   RX <7:0>                    |
     *   -----+-----------------------+-----------------------+
     *    3   |  0     0     0     0  |       RX <11:8>       |
     *   -----+-----------------------+-----------------------+
     *    4   |                   LY <7:0>                    |
     *   -----+-----------------------+-----------------------+
     *    5   |  0     0     0     0  |       LY <11:8>       |
     *   -----+-----------------------+-----------------------+
     *    6   |                   RY <7:0>                    |
     *   -----+-----------------------+-----------------------+
     *    7   |  0     0     0     0  |       RY <11:8>       |
     *   -----+-----+-----+-----+-----+-----+-----+-----+-----+
     *    8   | BDR | BDD | BLT | B-  | BH  | B+  | BRT |  1  |
     *   -----+-----+-----+-----+-----+-----+-----+-----+-----+
     *    9   | BZL | BB  | BY  | BA  | BX  | BZR | BDL | BDU |
     *   -----+-----+-----+-----+-----+-----+-----+-----+-----+
     *   10   |  1  |     BATTERY     | USB |CHARG|LTHUM|RTHUM|
     *   -----+-----+-----------------+-----------+-----+-----+
     * All buttons are low-active (0 if pressed)
     * RX and RY are right analog stick
     * LX and LY are left analog stick
     * BLT is left trigger, BRT is right trigger.
     * BDR, BDD, BDL, BDU form the D-Pad with right, down, left, up buttons
     * BZL is left Z button and BZR is right Z button
     * B-, BH, B+ are +, HOME and - buttons
     * BB, BY, BA, BX are A, B, X, Y buttons
     *
     * Bits marked as 0/1 are unknown and never changed during tests.
     *
     * Not entirely verified:
     *   CHARG: 1 if uncharging, 0 if charging
     *   USB: 1 if not connected, 0 if connected
     *   BATTERY: battery capacity from 000 (empty) to 100 (full)
     */
    if (len < 14) {
        loge("wii remote drm_kee: invalid report len %d\n", len);
        return;
    }
    uni_controller_t* ctl = &d->controller;
    const uint8_t* data = &report[3];

    // Process axis
    const uint16_t axis_base = 0x800;
    int16_t lx = data[0] + ((data[1] & 0x0f) << 8) - axis_base;
    int16_t rx = data[2] + ((data[3] & 0x0f) << 8) - axis_base;
    int16_t ly = data[4] + ((data[5] & 0x0f) << 8) - axis_base;
    int16_t ry = data[6] + ((data[7] & 0x0f) << 8) - axis_base;

    // Axis have 12-bit of resolution, but Bluepad32 uses 10-bit for the axis.
    // In theory we could just convert "from wire to bluepad32" in just one step
    // using a few "shift right" operations.
    // But apparently Wii U Controller doesn't use the whole range of the 12-bits.
    // The max value seems to be 1280 instead of 2048.
    lx = lx * 512 / 1280;
    rx = rx * 512 / 1280;
    ly = ly * 512 / 1280;
    ry = ry * 512 / 1280;

    // Y is inverted
    ctl->gamepad.axis_x = lx;
    ctl->gamepad.axis_y = -ly;
    ctl->gamepad.axis_rx = rx;
    ctl->gamepad.axis_ry = -ry;

    // Process Dpad
    ctl->gamepad.dpad |= !(data[8] & 0x80) ? DPAD_RIGHT : 0;  // BDR
    ctl->gamepad.dpad |= !(data[8] & 0x40) ? DPAD_DOWN : 0;   // BDD
    ctl->gamepad.dpad |= !(data[9] & 0x02) ? DPAD_LEFT : 0;   // BDL
    ctl->gamepad.dpad |= !(data[9] & 0x01) ? DPAD_UP : 0;     // BDU

    // Process buttons. A,B -> B,A; X,Y -> Y,X; trigger <--> shoulder
    ctl->gamepad.buttons |= !(data[9] & 0x10) ? BUTTON_B : 0;           // BA
    ctl->gamepad.buttons |= !(data[9] & 0x40) ? BUTTON_A : 0;           // BB
    ctl->gamepad.buttons |= !(data[9] & 0x08) ? BUTTON_Y : 0;           // BX
    ctl->gamepad.buttons |= !(data[9] & 0x20) ? BUTTON_X : 0;           // BY
    ctl->gamepad.buttons |= !(data[9] & 0x80) ? BUTTON_TRIGGER_L : 0;   // BZL
    ctl->gamepad.buttons |= !(data[9] & 0x04) ? BUTTON_TRIGGER_R : 0;   // BZR
    ctl->gamepad.buttons |= !(data[8] & 0x20) ? BUTTON_SHOULDER_L : 0;  // BLT
    ctl->gamepad.buttons |= !(data[8] & 0x02) ? BUTTON_SHOULDER_R : 0;  // BRT
    ctl->gamepad.buttons |= !(data[10] & 0x02) ? BUTTON_THUMB_L : 0;    // LTHUM
    ctl->gamepad.buttons |= !(data[10] & 0x01) ? BUTTON_THUMB_R : 0;    // RTHUM

    // Process misc buttons
    ctl->gamepad.misc_buttons |= !(data[8] & 0x08) ? MISC_BUTTON_SYSTEM : 0;  // BH
    ctl->gamepad.misc_buttons |= !(data[8] & 0x04) ? MISC_BUTTON_START : 0;   // B+
    ctl->gamepad.misc_buttons |= !(data[8] & 0x10) ? MISC_BUTTON_SELECT : 0;  // B-

    // Value is in -1,255 range. Should be normalized.
    int bat = (data[10] & DRM_KEE_BATTERY_MASK) * 4 - 1;
    if (bat < UNI_CONTROLLER_BATTERY_EMPTY)
        bat = UNI_CONTROLLER_BATTERY_EMPTY;
    if (bat > UNI_CONTROLLER_BATTERY_FULL)
        bat = UNI_CONTROLLER_BATTERY_FULL;
    ctl->battery = bat;
}

// Used for the Wii Classic Controller (includes Pro?). Defined here:
// http://wiibrew.org/wiki/Wiimote#0x3d:_21_Extension_Bytes
static void process_drm_e(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (len < 22) {
        loge("Wii: unexpected report length: got %d, want >= 22", len);
        return;
    }
    wii_instance_t* ins = get_wii_instance(d);
    // Assumes Classic Controller detected
    if (ins->ext_type != WII_EXT_CLASSIC_CONTROLLER) {
        loge("Wii: unexpected Wii extension: got %d, want: %d", ins->ext_type, WII_EXT_CLASSIC_CONTROLLER);
        return;
    }
    // Classic Controller format taken from here:
    // http://wiibrew.org/wiki/Wiimote/Extension_Controllers/Classic_Controller

    const uint8_t* data = &report[1];
    uni_controller_t* ctl = &d->controller;

    // Axis
    int lx = data[0] & 0b00111111;
    int ly = data[1] & 0b00111111;
    int rx = (data[0] & 0b11000000) >> 3 | (data[1] & 0b11000000) >> 5 | (data[0] & 0b10000000) >> 7;
    int ry = data[2] & 0b00011111;
    // Left axis has 6 bit of resolution. While right axis has only 5 bits.
    lx -= 32;
    ly -= 32;
    rx -= 16;
    ry -= 16;
    lx *= (AXIS_NORMALIZE_RANGE / 2 / 32);
    ly *= (AXIS_NORMALIZE_RANGE / 2 / 32);
    rx *= (AXIS_NORMALIZE_RANGE / 2 / 16);
    ry *= (AXIS_NORMALIZE_RANGE / 2 / 16);
    ctl->gamepad.axis_x = lx;
    ctl->gamepad.axis_y = -ly;
    ctl->gamepad.axis_rx = rx;
    ctl->gamepad.axis_ry = -ry;

    // Accel / Brake
    int lt = (data[2] & 0b01100000) >> 2 | (data[3] & 0b11100000) >> 5;
    int rt = data[3] & 0b00011111;
    ctl->gamepad.brake = lt * (AXIS_NORMALIZE_RANGE / 32);
    ctl->gamepad.throttle = rt * (AXIS_NORMALIZE_RANGE / 32);

    // dpad
    ctl->gamepad.dpad |= (data[4] & 0b10000000) ? 0 : DPAD_RIGHT;
    ctl->gamepad.dpad |= (data[4] & 0b01000000) ? 0 : DPAD_DOWN;
    ctl->gamepad.dpad |= (data[5] & 0b00000001) ? 0 : DPAD_UP;
    ctl->gamepad.dpad |= (data[5] & 0b00000010) ? 0 : DPAD_LEFT;

    // Buttons A,B,X,Y
    ctl->gamepad.buttons |= (data[5] & 0b01000000) ? 0 : BUTTON_A;
    ctl->gamepad.buttons |= (data[5] & 0b00010000) ? 0 : BUTTON_B;
    ctl->gamepad.buttons |= (data[5] & 0b00100000) ? 0 : BUTTON_X;
    ctl->gamepad.buttons |= (data[5] & 0b00001000) ? 0 : BUTTON_Y;

    // Shoulder / Trigger
    ctl->gamepad.buttons |= (data[4] & 0b00100000) ? 0 : BUTTON_SHOULDER_L;  // BLT
    ctl->gamepad.buttons |= (data[4] & 0b00000010) ? 0 : BUTTON_SHOULDER_R;  // BRT
    ctl->gamepad.buttons |= (data[5] & 0b10000000) ? 0 : BUTTON_TRIGGER_L;   // BZL
    ctl->gamepad.buttons |= (data[5] & 0b00000100) ? 0 : BUTTON_TRIGGER_R;   // BZR

    // Buttons Misc
    ctl->gamepad.misc_buttons |= (data[4] & 0b00001000) ? 0 : MISC_BUTTON_SYSTEM;  // Home
    ctl->gamepad.misc_buttons |= (data[4] & 0b00000100) ? 0 : MISC_BUTTON_START;   // +
    ctl->gamepad.misc_buttons |= (data[4] & 0b00010000) ? 0 : MISC_BUTTON_SELECT;  // -

    // printf("lx=%d, ly=%d, rx=%d, ry=%d, lt=%d, rt=%d\n", lx, ly, rx, ry, lt, rt);
    // printf_hexdump(report, len);
}

// wii_fsm_ functions

static void wii_fsm_req_status(uni_hid_device_t* d) {
    logi("fsm: req_status\n");
    wii_instance_t* ins = get_wii_instance(d);
    ins->state = WII_FSM_DID_REQ_STATUS;
    const uint8_t status[] = {0xa2, WIIPROTO_REQ_SREQ, 0x00 /* LEDS & rumble off */};
    uni_hid_device_send_intr_report(d, status, sizeof(status));
}

static void wii_fsm_ext_init(uni_hid_device_t* d) {
    logi("fsm: ext_init\n");
    wii_instance_t* ins = get_wii_instance(d);
    ins->state = WII_FSM_EXT_DID_INIT;
    // Init Wii
    uint8_t report[] = {
        // clang-format off
      0xa2, WIIPROTO_REQ_WMEM,
      0x04,             // Control registers
      0xa4, 0x00, 0xf0, // register init extension
      0x01, 0x55,       // # bytes, byte to write
      // Padding, since at least 16 bytes must be sent
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00,
        // clang-format on
    };
    report[3] = ins->register_address;
    uni_hid_device_send_intr_report(d, report, sizeof(report));
}

static void wii_fsm_ext_encrypt_off(uni_hid_device_t* d) {
    logi("fsm: ext_encrypt_off\n");
    wii_instance_t* ins = get_wii_instance(d);
    ins->state = WII_FSM_EXT_DID_NO_ENCRYPTION;
    // Init Wii
    uint8_t report[] = {
        // clang-format off
      0xa2, WIIPROTO_REQ_WMEM,
      0x04,             // Control registers
      0xa4, 0x00, 0xfb, // register disable encryption
      0x01, 0x00,       // # bytes, byte to write
      // Padding, since at least 16 bytes must be sent
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00,
        // clang-format on
    };
    report[3] = ins->register_address;
    uni_hid_device_send_intr_report(d, report, sizeof(report));
}

static void wii_fsm_ext_read_register(uni_hid_device_t* d) {
    logi("fsm: ext_read_register\n");
    wii_instance_t* ins = get_wii_instance(d);
    ins->state = WII_FSM_EXT_DID_READ_REGISTER;

    // Addr is either 0xA400FA or 0xA600FA
    uint32_t offset = 0x0000fa | (ins->register_address << 16);
    uint16_t bytes_to_read = 6;
    wii_read_mem(d, WII_READ_FROM_REGISTERS, offset, bytes_to_read);
}

static void wii_fsm_balance_board_read_calibration(uni_hid_device_t* d) {
    logi("fsm: balance_board_read_calibration\n");
    wii_instance_t* ins = get_wii_instance(d);
    ins->state = WII_FSM_BALANCE_BOARD_DID_READ_CALIBRATION;

    // Addr is either 0xA40024 or 0xA60024
    uint32_t offset = 0x000024 | (ins->register_address << 16);
    uint16_t bytes_to_read = 16;
    wii_read_mem(d, WII_READ_FROM_REGISTERS, offset, bytes_to_read);
}

static void wii_fsm_balance_board_read_calibration2(uni_hid_device_t* d) {
    logi("fsm: balance_board_read_calibration\n");
    wii_instance_t* ins = get_wii_instance(d);
    ins->state = WII_FSM_BALANCE_BOARD_DID_READ_CALIBRATION2;

    // Addr is either 0xA40024 or 0xA60024
    uint32_t offset = 0x000034 | (ins->register_address << 16);
    uint16_t bytes_to_read = 8;
    wii_read_mem(d, WII_READ_FROM_REGISTERS, offset, bytes_to_read);
}

static void wii_fsm_assign_device(uni_hid_device_t* d) {
    logi("fsm: assign_device\n");
    wii_instance_t* ins = get_wii_instance(d);
    uint8_t dev = ins->dev_type;
    switch (dev) {
        case WII_DEVTYPE_UNK:
        case WII_DEVTYPE_REMOTE:
        case WII_DEVTYPE_REMOTE_MP: {
            if (dev == WII_DEVTYPE_REMOTE) {
                logi("Wii Remote detected.\n");
            } else if (dev == WII_DEVTYPE_REMOTE_MP) {
                logi("Wii Remote (2nd gen) Motion Plus detected.\n");
            } else {
                logi("Unknown Wii device detected. Treating it as Wii Remote.\n");
            }
            uint8_t reportType = 0xff;
            if (ins->ext_type == WII_EXT_NUNCHUK) {
                // Request Nunchuk data
                if (ins->mode == WII_MODE_ACCEL) {
                    // Request Core buttons + Accel + extension (nunchuk)
                    reportType = WIIPROTO_REQ_DRM_KAE;
                    logi("Wii: requesting Core buttons + Accelerometer + E (Nunchuk)\n");
                    d->controller_subtype = CONTROLLER_SUBTYPE_WIIMOTE_NUNCHUK_ACCEL;
                } else {
                    // Request Core buttons + extension (nunchuk)
                    reportType = WIIPROTO_REQ_DRM_KE;
                    logi("Wii: requesting Core buttons + E (Nunchuk)\n");
                    d->controller_subtype = CONTROLLER_SUBTYPE_WIIMOTE_NUNCHUK;
                }
            } else if (ins->ext_type == WII_EXT_CLASSIC_CONTROLLER) {
                logi("Wii: requesting E (Classic Controller)\n");
                d->controller_subtype = CONTROLLER_SUBTYPE_WII_CLASSIC;
                reportType = WIIPROTO_REQ_DRM_E;
            } else if (ins->ext_type == WII_EXT_BALANCE_BOARD) {
                logi("Wii: requesting Weight (Balance Board)\n");
                d->controller_subtype = CONTROLLER_SUBTYPE_WII_BALANCE_BOARD;
                reportType = WIIPROTO_REQ_DRM_KEE;
            } else {
                if (ins->mode == WII_MODE_ACCEL) {
                    // Request Core buttons + accel
                    reportType = WIIPROTO_REQ_DRM_KA;
                    logi("Wii: requesting Core buttons + Accelerometer\n");
                    d->controller_subtype = CONTROLLER_SUBTYPE_WIIMOTE_ACCEL;
                } else {
                    reportType = WIIPROTO_REQ_DRM_K;
                    logi("Wii: requesting Core buttons\n");
                    if (ins->mode == WII_MODE_VERTICAL) {
                        d->controller_subtype = CONTROLLER_SUBTYPE_WIIMOTE_VERTICAL;
                    } else {
                        d->controller_subtype = CONTROLLER_SUBTYPE_WIIMOTE_HORIZONTAL;
                    }
                }
            }
            uint8_t report[] = {0xa2, WIIPROTO_REQ_DRM, 0x00, reportType};
            uni_hid_device_send_intr_report(d, report, sizeof(report));
            break;
        }
        case WII_DEVTYPE_PRO_CONTROLLER: {
            logi("Wii U Pro controller detected.\n");
            d->controller_subtype = CONTROLLER_SUBTYPE_WIIUPRO;
            // 0x34 WIIPROTO_REQ_DRM_KEE (present in Wii U Pro controller)
            const uint8_t reportKee[] = {0xa2, WIIPROTO_REQ_DRM, 0x00, WIIPROTO_REQ_DRM_KEE};
            uni_hid_device_send_intr_report(d, reportKee, sizeof(reportKee));
            break;
        }
        default:
            loge("Wii: wii_fsm_assign_device() unexpected device type: %d\n", ins->dev_type);
    }
    ins->state = WII_FSM_DEV_ASSIGNED;
    wii_process_fsm(d);
}

static void wii_fsm_update_led(uni_hid_device_t* d) {
    logi("fsm: upload_led\n");
    wii_instance_t* ins = get_wii_instance(d);
    wii_set_led(d, ins->gamepad_seat);
    ins->state = WII_FSM_LED_UPDATED;
    wii_process_fsm(d);

    uni_hid_device_set_ready_complete(d);
}

static void wii_fsm_dump_eeprom(struct uni_hid_device_s* d) {
#if ENABLE_EEPROM_DUMP
    wii_instance_t* ins = get_wii_instance(d);
    uint32_t addr = ins->debug_addr;

    ins->state = WII_FSM_DUMP_EEPROM_IN_PROGRESS;

    if (addr >= WII_DUMP_ROM_DATA_ADDR_END || ins->debug_fd < 0) {
        ins->state = WII_FSM_DUMP_EEPROM_FINISHED;
        close(ins->debug_fd);
        wii_process_fsm(d);
        return;
    }

    wii_read_mem(d, WII_READ_FROM_MEM, ins->debug_addr, WII_DUMP_ROM_DATA_SIZE);
    ins->debug_addr += WII_DUMP_ROM_DATA_SIZE;
#else
    wii_instance_t* ins = get_wii_instance(d);
    ins->state = WII_FSM_DUMP_EEPROM_FINISHED;
    wii_process_fsm(d);
#endif  // ENABLE_EEPROM_DUMP
}

static void wii_process_fsm(uni_hid_device_t* d) {
    wii_instance_t* ins = get_wii_instance(d);
    switch (ins->state) {
        case WII_FSM_SETUP:
        case WII_FSM_DUMP_EEPROM_IN_PROGRESS:
            wii_fsm_dump_eeprom(d);
            break;
        case WII_FSM_DUMP_EEPROM_FINISHED:
            wii_fsm_req_status(d);
            break;
        case WII_FSM_DID_REQ_STATUS:
            // Do nothing
            break;
        case WII_FSM_DEV_UNK:
        case WII_FSM_EXT_UNK:
            // Query extension
            wii_fsm_ext_init(d);
            break;
        case WII_FSM_EXT_DID_INIT:
            wii_fsm_ext_encrypt_off(d);
            break;
        case WII_FSM_EXT_DID_NO_ENCRYPTION:
            wii_fsm_ext_read_register(d);
            break;
        case WII_FSM_EXT_DID_READ_REGISTER:
            // Do nothing
            break;
        case WII_FSM_DEV_GUESSED:
            wii_fsm_assign_device(d);
            break;
        case WII_FSM_BALANCE_BOARD_READ_CALIBRATION:
            wii_fsm_balance_board_read_calibration(d);
            break;
        case WII_FSM_BALANCE_BOARD_READ_CALIBRATION2:
            wii_fsm_balance_board_read_calibration2(d);
            break;
        case WII_FSM_BALANCE_BOARD_DID_READ_CALIBRATION:
        case WII_FSM_BALANCE_BOARD_DID_READ_CALIBRATION2:
            // Do nothing;
            break;
        case WII_FSM_DEV_ASSIGNED:
            wii_fsm_update_led(d);
            break;
        case WII_FSM_LED_UPDATED:
            break;
        default:
            loge("Wii: wii_process_fsm() unexpected state: %d\n", ins->state);
    }
}

// uni_hid_ exported functions

void uni_hid_parser_wii_setup(uni_hid_device_t* d) {
    wii_instance_t* ins = get_wii_instance(d);

    memset(ins, 0, sizeof(*ins));

    ins->mode = WII_MODE_HORIZONTAL;
    ins->state = WII_FSM_SETUP;

    // Start with 0xa40000 (all Wii devices, except for the Wii Remote Plus)
    // If it fails it will use 0xa60000
    ins->register_address = 0xa4;

    // Dump EEPROM
#if ENABLE_EEPROM_DUMP
    ins->debug_addr = WII_DUMP_ROM_DATA_ADDR_START;
    ins->debug_fd = open("/tmp/wii_eeprom.bin", O_CREAT | O_RDWR);
    if (ins->debug_fd < 0) {
        loge("Wii: failed to create dump file");
    }
#endif  // ENABLE_EEPROM_DUMP

    wii_process_fsm(d);
}

void uni_hid_parser_wii_init_report(uni_hid_device_t* d) {
    // Reset old state. Each report contains a full-state.
    memset(&d->controller, 0, sizeof(d->controller));
    d->controller.klass = UNI_CONTROLLER_CLASS_GAMEPAD;
}

void uni_hid_parser_wii_parse_input_report(uni_hid_device_t* d, const uint8_t* report, uint16_t len) {
    if (len == 0)
        return;
    switch (report[0]) {
        case WIIPROTO_REQ_STATUS:
            process_req_status(d, report, len);
            break;
        case WIIPROTO_REQ_DRM_K:
            process_drm_k(d, report, len);
            break;
        case WIIPROTO_REQ_DRM_KA:
            process_drm_ka(d, report, len);
            break;
        case WIIPROTO_REQ_DRM_KE:
            process_drm_ke(d, report, len);
            break;
        case WIIPROTO_REQ_DRM_KAE:
            process_drm_kae(d, report, len);
            break;
        case WIIPROTO_REQ_DRM_KEE:
            process_drm_kee(d, report, len);
            break;
        case WIIPROTO_REQ_DRM_E:
            process_drm_e(d, report, len);
            break;
        case WIIPROTO_REQ_DATA:
            process_req_data(d, report, len);
            break;
        case WIIPROTO_REQ_RETURN:
            process_req_return(d, report, len);
            break;
        default:
            logi("Wii parser: unknown report type: 0x%02x\n", report[0]);
            printf_hexdump(report, len);
    }
}

void uni_hid_parser_wii_set_player_leds(uni_hid_device_t* d, uint8_t leds) {
    if (d == NULL) {
        loge("Wii: ERROR: Invalid device\n");
        return;
    }

    wii_instance_t* ins = get_wii_instance(d);
    // Always update gamepad_seat regardless of the state
    ins->gamepad_seat = leds;

    if (ins->state < WII_FSM_LED_UPDATED)
        return;

    wii_set_led(d, leds);
}

void uni_hid_parser_wii_play_dual_rumble(struct uni_hid_device_s* d,
                                         uint16_t start_delay_ms,
                                         uint16_t duration_ms,
                                         uint8_t weak_magnitude,
                                         uint8_t strong_magnitude) {
    ARG_UNUSED(weak_magnitude);
    ARG_UNUSED(strong_magnitude);

    if (d == NULL) {
        loge("Wii: Invalid device\n");
        return;
    }

    wii_instance_t* ins = get_wii_instance(d);
    if (ins->state < WII_FSM_LED_UPDATED) {
        return;
    }

    switch (ins->rumble_state) {
        case WII_STATE_RUMBLE_DELAYED:
            btstack_run_loop_remove_timer(&ins->rumble_timer_delayed_start);
            break;
        case WII_STATE_RUMBLE_IN_PROGRESS:
            btstack_run_loop_remove_timer(&ins->rumble_timer_duration);
            break;
        default:
            // Do nothing
            break;
    }

    if (start_delay_ms == 0) {
        wii_play_dual_rumble_now(d, duration_ms);
    } else {
        // Set timer to have a delayed start
        ins->rumble_timer_delayed_start.process = &on_wii_set_rumble_on;
        ins->rumble_timer_delayed_start.context = d;
        ins->rumble_state = WII_STATE_RUMBLE_DELAYED;
        ins->rumble_duration_ms = duration_ms;

        btstack_run_loop_set_timer(&ins->rumble_timer_delayed_start, start_delay_ms);
        btstack_run_loop_add_timer(&ins->rumble_timer_delayed_start);
    }
}

void uni_hid_parser_wii_set_mode(uni_hid_device_t* d, wii_mode_t mode) {
    wii_instance_t* ins = get_wii_instance(d);

    ins->mode = mode;
    switch (ins->mode) {
        case WII_MODE_HORIZONTAL:
            d->controller_subtype = CONTROLLER_SUBTYPE_WIIMOTE_HORIZONTAL;
            break;
        case WII_MODE_VERTICAL:
            d->controller_subtype = CONTROLLER_SUBTYPE_WIIMOTE_VERTICAL;
            break;
        case WII_MODE_ACCEL:
            // TODO: request Accel report. As it is, it doesn't work.
            if (ins->ext_type == WII_EXT_NONE)
                d->controller_subtype = CONTROLLER_SUBTYPE_WIIMOTE_ACCEL;
            else if (ins->ext_type == WII_EXT_NUNCHUK)
                d->controller_subtype = CONTROLLER_SUBTYPE_WIIMOTE_NUNCHUK_ACCEL;
            break;
        default:
            break;
    }
}

//
// Helpers
//
static wii_instance_t* get_wii_instance(uni_hid_device_t* d) {
    return (wii_instance_t*)&d->parser_data[0];
}

static void wii_set_led(uni_hid_device_t* d, uni_gamepad_seat_t seat) {
    wii_instance_t* ins = get_wii_instance(d);

    // Set LED to 1.
    uint8_t report[] = {
        0xa2, WIIPROTO_REQ_LED, 0x00 /* LED & Rumble off */
    };
    uint8_t led = seat << 4;

    // If vertical mode is on, enable LED 4.
    if (ins->mode == WII_MODE_VERTICAL) {
        led |= 0x80;
    }
    // If accelerometer enabled, enable LED 3.
    if (ins->mode == WII_MODE_ACCEL) {
        led |= 0x40;
    }

    // Rumble could be enabled
    if (ins->rumble_state == WII_STATE_RUMBLE_IN_PROGRESS)
        led |= 0x01;

    report[2] = led;
    uni_hid_device_send_intr_report(d, report, sizeof(report));
}

static void wii_stop_rumble_now(uni_hid_device_t* d) {
    wii_instance_t* ins = get_wii_instance(d);

    // No need to protect it with a mutex since it runs in the same main thread
    assert(ins->rumble_state == WII_STATE_RUMBLE_IN_PROGRESS);
    ins->rumble_state = WII_STATE_RUMBLE_DISABLED;

    // Disable rumble
    uint8_t report[] = {
        0xa2, WIIPROTO_REQ_RUMBLE, 0x00 /* Rumble off*/
    };
    uni_hid_device_send_intr_report(d, report, sizeof(report));
}

static void wii_play_dual_rumble_now(uni_hid_device_t* d, uint16_t duration_ms) {
    wii_instance_t* ins = get_wii_instance(d);

    if (duration_ms == 0) {
        if (ins->rumble_state == WII_STATE_RUMBLE_IN_PROGRESS)
            wii_stop_rumble_now(d);
        return;
    }

    uint8_t report[] = {
        0xa2, WIIPROTO_REQ_RUMBLE, 0x01 /* Rumble on*/
    };
    uni_hid_device_send_intr_report(d, report, sizeof(report));

    // Set timer to turn off rumble
    ins->rumble_timer_duration.process = &on_wii_set_rumble_off;
    ins->rumble_timer_duration.context = d;
    ins->rumble_state = WII_STATE_RUMBLE_IN_PROGRESS;
    btstack_run_loop_set_timer(&ins->rumble_timer_duration, duration_ms);
    btstack_run_loop_add_timer(&ins->rumble_timer_duration);
}

static void on_wii_set_rumble_on(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    wii_instance_t* ins = get_wii_instance(d);

    wii_play_dual_rumble_now(d, ins->rumble_duration_ms);
}

static void on_wii_set_rumble_off(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = btstack_run_loop_get_timer_context(ts);
    wii_stop_rumble_now(d);
}

static void wii_read_mem(uni_hid_device_t* d, wii_read_type_t t, uint32_t offset, uint16_t size) {
    logi("****** read_mem: offset=0x%04x, size=%d from=%d\n", offset, size, t);
    uint8_t report[] = {
        // clang-format off
      0xa2, WIIPROTO_REQ_RMEM,
      t, // Read from registers or memory
      (offset & 0xff0000) >> 16, (offset & 0xff00) >> 8, (offset & 0xff), // Offset
      (size & 0xff00) >> 8, (size & 0xff), // Size in bytes
        // clang-format on
    };
    uni_hid_device_send_intr_report(d, report, sizeof(report));
}

void uni_hid_parser_wii_device_dump(uni_hid_device_t* d) {
    wii_instance_t* ins = get_wii_instance(d);
    logi("\tWii: device '%s', extension '%s'\n", wii_devtype_names[ins->dev_type], wii_exttype_names[ins->ext_type]);
}
