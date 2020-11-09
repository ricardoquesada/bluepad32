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

// Technical info taken from:
// http://wiibrew.org/wiki/Wiimote
// https://github.com/dvdhrm/xwiimote/blob/master/doc/PROTOCOL

#define ENABLE_EEPROM_DUMP 0

#if ENABLE_EEPROM_DUMP
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // ENABLE_EEPROM_DUMP

#include "hid_usage.h"
#include "uni_debug.h"
#include "uni_gamepad.h"
#include "uni_hid_device.h"
#include "uni_hid_parser.h"
#include "uni_hid_parser_wii.h"

#if ENABLE_EEPROM_DUMP
#define WII_DUMP_ROM_DATA_SIZE 16  // Max size is 16
static const uint32_t WII_DUMP_ROM_DATA_ADDR_START = 0x0000;
static const uint32_t WII_DUMP_ROM_DATA_ADDR_END = 0x1700;
#endif  // ENABLE_EEPROM_DUMP

enum wii_flags {
  WII_FLAGS_NONE = 0,
  WII_FLAGS_VERTICAL = 1 << 0,
  WII_FLAGS_ACCEL = 1 << 1,
};

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
  WII_FSM_EXT_DID_NO_ENCRYPTION,    // Extension no encription
  WII_FSM_EXT_DID_READ_REGISTER,    // Extension read register
  WII_FSM_DEV_GUESSED,              // Device type guessed
  WII_FSM_DEV_ASSIGNED,             // Device type assigned
  WII_FSM_LED_UPDATED,              // After device was assigned, update LEDs.
};

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

// wii_instance_t represents data used by the Wii driver instance.
typedef struct wii_instance_s {
  uint8_t state;
  uint8_t register_address;
  uint8_t flags;
  enum wii_devtype dev_type;
  enum wii_exttype ext_type;

  // Debug only
  int debug_fd;         // File descriptor where dump is saved
  uint32_t debug_addr;  // Current dump address
} wii_instance_t;

static void process_req_status(uni_hid_device_t* d, const uint8_t* report,
                               uint16_t len);
static void process_req_data(uni_hid_device_t* d, const uint8_t* report,
                             uint16_t len);
static void process_req_return(uni_hid_device_t* d, const uint8_t* report,
                               uint16_t len);
static void process_drm_k(uni_hid_device_t* d, const uint8_t* report,
                          uint16_t len);
static void process_drm_k_vertical(uni_gamepad_t* gp, const uint8_t* data);
static void process_drm_k_horizontal(uni_gamepad_t* gp, const uint8_t* data);
static void process_drm_ka(uni_hid_device_t* d, const uint8_t* report,
                           uint16_t len);
static void process_drm_ke(uni_hid_device_t* d, const uint8_t* report,
                           uint16_t len);
static void process_drm_kae(uni_hid_device_t* d, const uint8_t* report,
                            uint16_t len);
static void process_drm_kee(uni_hid_device_t* d, const uint8_t* report,
                            uint16_t len);
static void process_drm_e(uni_hid_device_t* d, const uint8_t* report,
                          uint16_t len);
static nunchuk_t process_nunchuk(const uint8_t* e, uint16_t len);

static void wii_process_fsm(uni_hid_device_t* d);
static void wii_fsm_ext_init(uni_hid_device_t* d);
static void wii_fsm_ext_encrypt_off(uni_hid_device_t* d);
static void wii_fsm_ext_read_register(uni_hid_device_t* d);
static void wii_fsm_req_status(uni_hid_device_t* d);
static void wii_fsm_assign_device(uni_hid_device_t* d);
static void wii_fsm_update_led(uni_hid_device_t* d);
static void wii_fsm_dump_eeprom(uni_hid_device_t* d);

static void wii_read_mem(uni_hid_device_t* d, wii_read_type_t t,
                         uint32_t offset, uint16_t size);
static wii_instance_t* get_wii_instance(uni_hid_device_t* d);

// process_ functions

// Defined here: http://wiibrew.org/wiki/Wiimote#0x20:_Status
static void process_req_status(uni_hid_device_t* d, const uint8_t* report,
                               uint16_t len) {
  if (len < 7) {
    loge("Wii: Unexpected report lenght; got %d, want >= 7\n", len);
    return;
  }
  wii_instance_t* ins = get_wii_instance(d);
  uint8_t flags = report[3] & 0x0f;  // LF (leds / flags)
  if (ins->state == WII_FSM_DID_REQ_STATUS) {
    if (d->product_id == 0x0306) {
      // We are possitive that this is a Wii Remote 1st gen
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
      ins->flags |= WII_FLAGS_ACCEL;
    } else if (report[1] & 0x10) {
      // Wii Remote only: Enter "vertical mode" if "+" is pressed.
      ins->flags |= WII_FLAGS_VERTICAL;
    }

    wii_process_fsm(d);
  }
}

// Defined here: http://wiibrew.org/wiki/Wiimote#0x21:_Read_Memory_Data
static void process_req_data_read_register(uni_hid_device_t* d,
                                           const uint8_t* report,
                                           uint16_t len) {
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
      logi("Wii: Pro Controller extension found\n");
    } else if (ins->dev_type == WII_DEVTYPE_UNK) {
      if (d->product_id == 0x0330) {
        ins->dev_type = WII_DEVTYPE_REMOTE_MP;
        logi("Wii Remote MP detected\n");
      } else if (d->product_id == 0x0306) {
        ins->dev_type = WII_DEVTYPE_REMOTE;
        logi("Wii Remote detected\n");
      } else {
        loge("Wii: Unknown product id: 0x%04x\n", d->product_id);
      }
    }

    // Try to guess extension type.
    if (ins->ext_type == WII_EXT_UNK) {
      if (report[10] == 0x00 && report[11] == 0x00) {
        // Nunchuck: 00 00 a4 20 00 00
        ins->ext_type = WII_EXT_NUNCHUK;
        logi("Wii: Nunchuk extension found\n");
      } else if (report[10] == 0x01 && report[11] == 0x01) {
        // Classic / Classic Pro: 0? 00 a4 20 01 01
        ins->ext_type = WII_EXT_CLASSIC_CONTROLLER;
        logi(
            "Wii: Classic Controller / Classic Controller Pro extension "
            "found\n");
      } else {
        loge("Wii: Unknown extension\n");
        printf_hexdump(report, len);
      }
    }

    ins->state = WII_FSM_DEV_GUESSED;
    wii_process_fsm(d);
  } else {
    loge("Wii: invalid response");
    printf_hexdump(report, len);
  }
}

static void process_req_data_dump_eeprom(uni_hid_device_t* d,
                                         const uint8_t* report, uint16_t len) {
  UNUSED(len);
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
  UNUSED(d);
  UNUSED(report);
#endif  // ENABLE_EEPROM_DUMP
  wii_process_fsm(d);
}

// Defined here: http://wiibrew.org/wiki/Wiimote#0x21:_Read_Memory_Data
static void process_req_data(uni_hid_device_t* d, const uint8_t* report,
                             uint16_t len) {
  logi("**** process_req_data\n");
  printf_hexdump(report, len);

  if (len < 22) {
    loge("Wii: invalid req_data lenght: got %d, want >= 22\n", len);
    printf_hexdump(report, len);
    return;
  }

  wii_instance_t* ins = get_wii_instance(d);
  switch (ins->state) {
    case WII_FSM_EXT_DID_READ_REGISTER:
      process_req_data_read_register(d, report, len);
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
static void process_req_return(uni_hid_device_t* d, const uint8_t* report,
                               uint16_t len) {
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
        // If it failed to read registers with 0xa4, then try with 0xa6
        // If 0xa6 works Ok, it is safe to assume it is a Wii Remote MP, but
        // for the sake of finishing the "read extension" (might be useful
        // in the future), we continue with it.
        logi(
            "Probably a Remote MP device. Switching to 0xa60000 address "
            "for registers.\n");
        ins->state = WII_FSM_DEV_UNK;
        ins->register_address =
            0xa6;  // Register address used for Wii Remote MP.
      }
    } else {
      // Status Ok. Good
    }
    wii_process_fsm(d);
  }
}

// Defined here: http://wiibrew.org/wiki/Wiimote#0x30:_Core_Buttons
static void process_drm_k(uni_hid_device_t* d, const uint8_t* report,
                          uint16_t len) {
  /* DRM_K: BB*2 */
  // Expecting something like:
  // 30 00 08
  if (len < 3) {
    loge("wii remote drm_k: invalid report len %d\n", len);
    return;
  }

  uni_gamepad_t* gp = &d->gamepad;
  const uint8_t* data = &report[1];
  wii_instance_t* ins = get_wii_instance(d);

  if (ins->flags & WII_FLAGS_VERTICAL) {
    process_drm_k_vertical(gp, data);
  } else {
    process_drm_k_horizontal(gp, data);
  }
  // Process misc buttons
  gp->misc_buttons |=
      (data[1] & 0x80) ? MISC_BUTTON_SYSTEM : 0;                // Button "home"
  gp->misc_buttons |= (data[0] & 0x10) ? MISC_BUTTON_HOME : 0;  // Button "+"
  gp->updated_states |=
      GAMEPAD_STATE_MISC_BUTTON_SYSTEM | GAMEPAD_STATE_MISC_BUTTON_HOME;
}

static void process_drm_k_horizontal(uni_gamepad_t* gp, const uint8_t* data) {
  // dpad
  gp->dpad |= (data[0] & 0x01) ? DPAD_DOWN : 0;
  gp->dpad |= (data[0] & 0x02) ? DPAD_UP : 0;
  gp->dpad |= (data[0] & 0x04) ? DPAD_RIGHT : 0;
  gp->dpad |= (data[0] & 0x08) ? DPAD_LEFT : 0;
  gp->updated_states |= GAMEPAD_STATE_DPAD;

  // buttons
  gp->buttons |= (data[1] & 0x04) ? BUTTON_Y : 0;  // Shoulder button
  gp->buttons |= (data[1] & 0x08) ? BUTTON_X : 0;  // Big button "A"
  gp->buttons |= (data[1] & 0x02) ? BUTTON_A : 0;  // Button "1"
  gp->buttons |= (data[1] & 0x01) ? BUTTON_B : 0;  // Button "2"
  gp->updated_states |= GAMEPAD_STATE_BUTTON_A | GAMEPAD_STATE_BUTTON_B |
                        GAMEPAD_STATE_BUTTON_X | GAMEPAD_STATE_BUTTON_Y;
}

static void process_drm_k_vertical(uni_gamepad_t* gp, const uint8_t* data) {
  // dpad
  gp->dpad |= (data[0] & 0x01) ? DPAD_LEFT : 0;
  gp->dpad |= (data[0] & 0x02) ? DPAD_RIGHT : 0;
  gp->dpad |= (data[0] & 0x04) ? DPAD_DOWN : 0;
  gp->dpad |= (data[0] & 0x08) ? DPAD_UP : 0;
  gp->updated_states |= GAMEPAD_STATE_DPAD;

  // buttons
  gp->buttons |= (data[1] & 0x04) ? BUTTON_A : 0;  // Shoulder button
  gp->buttons |= (data[1] & 0x08) ? BUTTON_B : 0;  // Big button "A"
  gp->buttons |= (data[1] & 0x02) ? BUTTON_X : 0;  // Button "1"
  gp->buttons |= (data[1] & 0x01) ? BUTTON_Y : 0;  // Button "2"
  gp->updated_states |= GAMEPAD_STATE_BUTTON_A | GAMEPAD_STATE_BUTTON_B |
                        GAMEPAD_STATE_BUTTON_X | GAMEPAD_STATE_BUTTON_Y;
}

// Defined here:
// http://wiibrew.org/wiki/Wiimote#0x31:_Core_Buttons_and_Accelerometer
static void process_drm_ka(uni_hid_device_t* d, const uint8_t* report,
                           uint16_t len) {
  // Process Wiimote in "accelerator mode".
  const int16_t accel_threshold = 32;
  /* DRM_KA: BB*2 AA*3*/
  // Expecting something like:
  // 31 20 60 82 7F 99
  if (len < 6) {
    loge("wii remote drm_ka: invalid report len %d\n", len);
    return;
  }

  uint16_t x = report[3] << 2;
  uint16_t y = report[4] << 2;
  // uint16_t z = report[5] << 2;

  x |= (report[1] >> 5) & 0x3;
  y |= (report[2] >> 4) & 0x2;
  // z |= (report[2] >> 5) & 0x2;

  int16_t sx = x - 0x200;
  int16_t sy = y - 0x200;
  // int16_t sz = z - 0x200;

  // printf_hexdump(report, len);
  // printf("x=%d, y=%d, z=%d\n", x, y, z);

  uni_gamepad_t* gp = &d->gamepad;
  if (sx < -accel_threshold) {
    gp->dpad |= DPAD_LEFT;
  } else if (sx > accel_threshold) {
    gp->dpad |= DPAD_RIGHT;
  }
  if (sy < -accel_threshold) {
    gp->dpad |= DPAD_UP;
  } else if (sy > (accel_threshold / 2)) {
    // Threshold for down is 50% because it is not as easy to tilt the
    // device down as it is it to tilt it up.
    gp->dpad |= DPAD_DOWN;
  }
  gp->updated_states |= GAMEPAD_STATE_DPAD;

  gp->buttons |= (report[2] & 0x08) ? BUTTON_A : 0;  // Big button "A"
  gp->buttons |= (report[2] & 0x04) ? BUTTON_B : 0;  // Button Shoulder
  gp->updated_states |= GAMEPAD_STATE_BUTTON_A | GAMEPAD_STATE_BUTTON_B;

  gp->misc_buttons |=
      (report[2] & 0x80) ? MISC_BUTTON_SYSTEM : 0;  // Button "home"
  gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
}

// Defined here:
// http://wiibrew.org/wiki/Wiimote#0x32:_Core_Buttons_with_8_Extension_bytes
static void process_drm_ke(uni_hid_device_t* d, const uint8_t* report,
                           uint16_t len) {
  // Expecting something like:
  // 32 BB BB EE EE EE EE EE EE EE EE
  if (len < 11) {
    loge("Wii: unexpected len; got %d, want >= 11\n", len);
    return;
  }

  wii_instance_t* ins = get_wii_instance(d);
  if (ins->ext_type != WII_EXT_NUNCHUK) {
    loge("Wii: unexpected Wii extension: got %d, want: %d", ins->ext_type,
         WII_EXT_NUNCHUK);
    return;
  }

  //
  // Process Nunchuk
  //
  nunchuk_t n = process_nunchuk(&report[3], len - 3);
  uni_gamepad_t* gp = &d->gamepad;
  const int factor = (AXIS_NORMALIZE_RANGE / 2) / 128;
  // When VERTICAL mode is enabled, Nunchuk behaves as "right" joystick,
  // and the Wii remote behaves as the "left" joystick.
  if (ins->flags == WII_FLAGS_VERTICAL) {
    // Treat Nunchuk as "right" joyspad.
    gp->axis_rx = n.sx * factor;
    gp->axis_ry = n.sy * factor;
    gp->updated_states |= GAMEPAD_STATE_AXIS_RX | GAMEPAD_STATE_AXIS_RY;
    gp->buttons |= n.bc ? BUTTON_B : 0;
    gp->buttons |= n.bz ? BUTTON_SHOULDER_R : 0;  // Autofire
  } else {
    // Treat Nunchuk as "left" joypad.
    gp->axis_x = n.sx * factor;
    gp->axis_y = n.sy * factor;
    gp->updated_states |= GAMEPAD_STATE_AXIS_X | GAMEPAD_STATE_AXIS_Y;
    gp->buttons |= n.bc ? BUTTON_A : 0;
    gp->buttons |= n.bz ? BUTTON_B : 0;
  }

  //
  // Process Wii remote
  //

  // dpad
  gp->dpad |= (report[1] & 0x01) ? DPAD_LEFT : 0;
  gp->dpad |= (report[1] & 0x02) ? DPAD_RIGHT : 0;
  gp->dpad |= (report[1] & 0x04) ? DPAD_DOWN : 0;
  gp->dpad |= (report[1] & 0x08) ? DPAD_UP : 0;
  gp->updated_states |= GAMEPAD_STATE_DPAD;

  gp->buttons |= (report[2] & 0x04) ? BUTTON_A : 0;  // Shoulder button
  if (ins->flags == WII_FLAGS_VERTICAL) {
    gp->buttons |=
        (report[2] & 0x08) ? BUTTON_SHOULDER_L : 0;  // Big button "A"
  } else {
    // If "vertical" not enabled, update Button B as well
    gp->buttons |= (report[2] & 0x08) ? BUTTON_B : 0;  // Big button "A"
  }
  gp->updated_states |= GAMEPAD_STATE_BUTTON_A | GAMEPAD_STATE_BUTTON_B |
                        GAMEPAD_STATE_BUTTON_SHOULDER_L |
                        GAMEPAD_STATE_BUTTON_SHOULDER_R;

  gp->misc_buttons |=
      (report[2] & 0x80) ? MISC_BUTTON_SYSTEM : 0;  // Button "home"
  gp->updated_states |= GAMEPAD_STATE_MISC_BUTTON_SYSTEM;
}

// Defined here:
// http://wiibrew.org/wiki/Wiimote#0x35:_Core_Buttons_and_Accelerometer_with_16_Extension_Bytes
static void process_drm_kae(uni_hid_device_t* d, const uint8_t* report,
                            uint16_t len) {
  // Expecting something like:
  // (a1) 35 BB BB AA AA AA EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE
  UNUSED(d);
  UNUSED(report);
  UNUSED(len);
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
  // Invert polarity to match Unijoysticle virtual gamepad.
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

// Defined here:
// http://wiibrew.org/wiki/Wiimote#0x34:_Core_Buttons_with_19_Extension_bytes
static void process_drm_kee(uni_hid_device_t* d, const uint8_t* report,
                            uint16_t len) {
  wii_instance_t* ins = get_wii_instance(d);
  if (ins->ext_type != WII_EXT_U_PRO_CONTROLLER) {
  }
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
  uni_gamepad_t* gp = &d->gamepad;
  const uint8_t* data = &report[3];

  // Process axis
  const uint16_t axis_base = 0x800;
  // const uint16_t axis_range = 0x500;
  int16_t lx = data[0] + ((data[1] & 0x0f) << 8) - axis_base;
  int16_t rx = data[2] + ((data[3] & 0x0f) << 8) - axis_base;
  int16_t ly = data[4] + ((data[5] & 0x0f) << 8) - axis_base;
  int16_t ry = data[6] + ((data[7] & 0x0f) << 8) - axis_base;

  // Y is inverted
  gp->axis_x = lx;
  gp->axis_y = -ly;
  gp->axis_rx = rx;
  gp->axis_ry = -ry;
  gp->updated_states |= GAMEPAD_STATE_AXIS_X | GAMEPAD_STATE_AXIS_Y |
                        GAMEPAD_STATE_AXIS_RX | GAMEPAD_STATE_AXIS_RY;

  // Process Dpad
  gp->dpad |= !(data[8] & 0x80) ? DPAD_RIGHT : 0;  // BDR
  gp->dpad |= !(data[8] & 0x40) ? DPAD_DOWN : 0;   // BDD
  gp->dpad |= !(data[9] & 0x02) ? DPAD_LEFT : 0;   // BDL
  gp->dpad |= !(data[9] & 0x01) ? DPAD_UP : 0;     // BDU
  gp->updated_states |= GAMEPAD_STATE_DPAD;

  // Process buttons. A,B -> B,A; X,Y -> Y,X; trigger <--> shoulder
  gp->buttons |= !(data[9] & 0x10) ? BUTTON_B : 0;           // BA
  gp->buttons |= !(data[9] & 0x40) ? BUTTON_A : 0;           // BB
  gp->buttons |= !(data[9] & 0x08) ? BUTTON_Y : 0;           // BX
  gp->buttons |= !(data[9] & 0x20) ? BUTTON_X : 0;           // BY
  gp->buttons |= !(data[9] & 0x80) ? BUTTON_TRIGGER_L : 0;   // BZL
  gp->buttons |= !(data[9] & 0x04) ? BUTTON_TRIGGER_R : 0;   // BZR
  gp->buttons |= !(data[8] & 0x20) ? BUTTON_SHOULDER_L : 0;  // BLT
  gp->buttons |= !(data[8] & 0x02) ? BUTTON_SHOULDER_R : 0;  // BRT
  gp->buttons |= !(data[10] & 0x02) ? BUTTON_THUMB_L : 0;    // LTHUM
  gp->buttons |= !(data[10] & 0x01) ? BUTTON_THUMB_R : 0;    // RTHUM
  gp->updated_states |=
      GAMEPAD_STATE_BUTTON_SHOULDER_L | GAMEPAD_STATE_BUTTON_SHOULDER_R |
      GAMEPAD_STATE_BUTTON_TRIGGER_L | GAMEPAD_STATE_BUTTON_TRIGGER_R |
      GAMEPAD_STATE_BUTTON_A | GAMEPAD_STATE_BUTTON_B | GAMEPAD_STATE_BUTTON_X |
      GAMEPAD_STATE_BUTTON_Y | GAMEPAD_STATE_BUTTON_THUMB_L |
      GAMEPAD_STATE_BUTTON_THUMB_R;

  // Process misc buttons
  gp->misc_buttons |= !(data[8] & 0x08) ? MISC_BUTTON_SYSTEM : 0;  // BH
  gp->misc_buttons |= !(data[8] & 0x04) ? MISC_BUTTON_HOME : 0;    // B+
  gp->updated_states |=
      GAMEPAD_STATE_MISC_BUTTON_SYSTEM | GAMEPAD_STATE_MISC_BUTTON_HOME;
}

// Defined here:
// http://wiibrew.org/wiki/Wiimote#0x3d:_21_Extension_Bytes
static void process_drm_e(uni_hid_device_t* d, const uint8_t* report,
                          uint16_t len) {
  if (len < 22) {
    loge("Wii: unexpected report lenght: got %d, want >= 22", len);
    return;
  }
  wii_instance_t* ins = get_wii_instance(d);
  // Assumes Classic Controller detected
  if (ins->ext_type != WII_EXT_CLASSIC_CONTROLLER) {
    loge("Wii: unexpected Wii extension: got %d, want: %d", ins->ext_type,
         WII_EXT_CLASSIC_CONTROLLER);
    return;
  }
  // Classic Controller format taken from here:
  // http://wiibrew.org/wiki/Wiimote/Extension_Controllers/Classic_Controller

  const uint8_t* data = &report[1];
  uni_gamepad_t* gp = &d->gamepad;

  // Axis
  int lx = data[0] & 0b00111111;
  int ly = data[1] & 0b00111111;
  int rx = (data[0] & 0b11000000) >> 3 | (data[1] & 0b11000000) >> 5 |
           (data[0] & 0b10000000) >> 7;
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
  gp->axis_x = lx;
  gp->axis_y = -ly;
  gp->axis_rx = rx;
  gp->axis_ry = -ry;
  gp->updated_states |= GAMEPAD_STATE_AXIS_X | GAMEPAD_STATE_AXIS_Y |
                        GAMEPAD_STATE_AXIS_RX | GAMEPAD_STATE_AXIS_RY;

  // Accel / Brake
  int lt = (data[2] & 0b01100000) >> 2 | (data[3] & 0b11100000) >> 5;
  int rt = data[3] & 0b00011111;
  gp->brake = lt * (AXIS_NORMALIZE_RANGE / 32);
  gp->accelerator = rt * (AXIS_NORMALIZE_RANGE / 32);
  gp->updated_states |= GAMEPAD_STATE_BRAKE | GAMEPAD_STATE_ACCELERATOR;

  // dpad
  gp->dpad |= (data[4] & 0b10000000) ? 0 : DPAD_RIGHT;
  gp->dpad |= (data[4] & 0b01000000) ? 0 : DPAD_DOWN;
  gp->dpad |= (data[5] & 0b00000001) ? 0 : DPAD_UP;
  gp->dpad |= (data[5] & 0b00000010) ? 0 : DPAD_LEFT;
  gp->updated_states |= GAMEPAD_STATE_DPAD;

  // Buttons A,B,X,Y
  gp->buttons |= (data[5] & 0b01000000) ? 0 : BUTTON_A;
  gp->buttons |= (data[5] & 0b00010000) ? 0 : BUTTON_B;
  gp->buttons |= (data[5] & 0b00100000) ? 0 : BUTTON_X;
  gp->buttons |= (data[5] & 0b00001000) ? 0 : BUTTON_Y;
  gp->updated_states |= GAMEPAD_STATE_BUTTON_A | GAMEPAD_STATE_BUTTON_B |
                        GAMEPAD_STATE_BUTTON_X | GAMEPAD_STATE_BUTTON_Y;

  // Shoulder / Trigger
  gp->buttons |= (data[4] & 0b00100000) ? 0 : BUTTON_SHOULDER_L;  // BLT
  gp->buttons |= (data[4] & 0b00000010) ? 0 : BUTTON_SHOULDER_R;  // BRT
  gp->buttons |= (data[5] & 0b10000000) ? 0 : BUTTON_TRIGGER_L;   // BZL
  gp->buttons |= (data[5] & 0b00000100) ? 0 : BUTTON_TRIGGER_R;   // BZR
  gp->updated_states |=
      GAMEPAD_STATE_BUTTON_SHOULDER_L | GAMEPAD_STATE_BUTTON_SHOULDER_R |
      GAMEPAD_STATE_BUTTON_TRIGGER_L | GAMEPAD_STATE_BUTTON_TRIGGER_R;

  // Buttons Misc
  gp->misc_buttons |= (data[4] & 0b00001000) ? 0 : MISC_BUTTON_SYSTEM;  // Home
  gp->misc_buttons |= (data[4] & 0b00000100) ? 0 : MISC_BUTTON_HOME;    // +
  gp->updated_states |=
      GAMEPAD_STATE_MISC_BUTTON_SYSTEM | GAMEPAD_STATE_MISC_BUTTON_HOME;

  // printf("lx=%d, ly=%d, rx=%d, ry=%d, lt=%d, rt=%d\n", lx, ly, rx, ry, lt,
  // rt);
  // printf_hexdump(report, len);
}

// wii_fsm_ functions

static void wii_fsm_req_status(uni_hid_device_t* d) {
  logi("fsm: req_status\n");
  wii_instance_t* ins = get_wii_instance(d);
  ins->state = WII_FSM_DID_REQ_STATUS;
  const uint8_t status[] = {0xa2, WIIPROTO_REQ_SREQ, 0x00 /* rumble off */};
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
        if (ins->flags & WII_FLAGS_ACCEL) {
          // Request Core buttons + Accel + extension (nunchuk)
          reportType = WIIPROTO_REQ_DRM_KAE;
          logi("Wii: requesting Core buttons + Accelerometer + E (Nunchuk)\n");
        } else {
          // Request Core buttons + extension (nunchuk)
          reportType = WIIPROTO_REQ_DRM_KE;
          logi("Wii: requesting Core buttons + E (Nunchuk)\n");
        }
      } else if (ins->ext_type == WII_EXT_CLASSIC_CONTROLLER) {
        logi("Wii: requesting E (Classic Controller)\n");
        reportType = WIIPROTO_REQ_DRM_E;
      } else {
        if (ins->flags & WII_FLAGS_ACCEL) {
          // Request Core buttons + accel
          reportType = WIIPROTO_REQ_DRM_KA;
          logi("Wii: requesting Core buttons + Accelerometer\n");
        } else {
          reportType = WIIPROTO_REQ_DRM_K;
          logi("Wii: requesting Core buttons\n");
        }
      }
      uint8_t report[] = {0xa2, WIIPROTO_REQ_DRM, 0x00, reportType};
      uni_hid_device_send_intr_report(d, report, sizeof(report));
      break;
    }
    case WII_DEVTYPE_PRO_CONTROLLER: {
      logi("Wii U Pro controller detected.\n");
      // 0x34 WIIPROTO_REQ_DRM_KEE (present in Wii U Pro controller)
      const uint8_t reportKee[] = {0xa2, WIIPROTO_REQ_DRM, 0x00,
                                   WIIPROTO_REQ_DRM_KEE};
      uni_hid_device_send_intr_report(d, reportKee, sizeof(reportKee));
      break;
    }
  }
  ins->state = WII_FSM_DEV_ASSIGNED;
  wii_process_fsm(d);
}

static void wii_fsm_update_led(uni_hid_device_t* d) {
  logi("fsm: upload_led\n");
  uni_hid_parser_wii_update_led(d);
  wii_instance_t* ins = get_wii_instance(d);
  ins->state = WII_FSM_LED_UPDATED;
  wii_process_fsm(d);
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
    case WII_FSM_DEV_ASSIGNED:
      wii_fsm_update_led(d);
      break;
    case WII_FSM_LED_UPDATED:
      // do nothing. FSM finished.
      break;
  }
}

// uni_hid_ exported functions

void uni_hid_parser_wii_setup(uni_hid_device_t* d) {
  wii_instance_t* ins = get_wii_instance(d);

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
  d->gamepad.updated_states = 0;
  d->gamepad.dpad = 0;
  d->gamepad.buttons = 0;
  d->gamepad.misc_buttons = 0;
}

void uni_hid_parser_wii_parse_raw(uni_hid_device_t* d, const uint8_t* report,
                                  uint16_t len) {
  if (len == 0) return;
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

void uni_hid_parser_wii_update_led(uni_hid_device_t* d) {
  if (d == NULL) {
    loge("Wii: ERROR: Invalid device\n");
    return;
  }
  // Set LED to 1.
  uint8_t report[] = {
      0xa2, WIIPROTO_REQ_LED, 0x00 /* LED */
  };
  uint8_t led = (d->joystick_port) << 4;

  wii_instance_t* ins = get_wii_instance(d);
  // If vertical mode is on, enable LED 4.
  if (ins->flags & WII_FLAGS_VERTICAL) {
    led |= 0x80;
  }
  // If accelerometer enabled, enable LED 3.
  if (ins->flags & WII_FLAGS_ACCEL) {
    led |= 0x40;
  }
  report[2] = led;
  uni_hid_device_send_intr_report(d, report, sizeof(report));
}

//
// Helpers
//
static wii_instance_t* get_wii_instance(uni_hid_device_t* d) {
  return (wii_instance_t*)&d->data[0];
}

static void wii_read_mem(uni_hid_device_t* d, wii_read_type_t t,
                         uint32_t offset, uint16_t size) {
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
