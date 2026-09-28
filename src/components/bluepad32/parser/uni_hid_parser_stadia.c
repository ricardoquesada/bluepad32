// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Ricardo Quesada
// http://retro.moe/unijoysticle2

// FF structure based on:
// https://git.kernel.org/pub/scm/linux/kernel/git/hid/hid.git/commit/?h=for-next&id=24175157b8520de2ed6219676bddb08c846f2d0d

#include "parser/uni_hid_parser_stadia.h"

#include <stdint.h>

#include "controller/uni_controller.h"
#include "parser/uni_hid_parser_rumble.h"
#include "uni_common.h"
#include "uni_hid_device.h"
#include "uni_log.h"

#define STADIA_RUMBLE_REPORT_ID 0x05

struct stadia_ff_report {
    uint16_t strong_magnitude;  // Left: 2100 RPM
    uint16_t weak_magnitude;    // Right: 3350 RPM
} __attribute__((packed));

static uni_rumble_result_t stadia_stop_rumble_now(struct uni_hid_device_s* d);
static uni_rumble_result_t stadia_start_rumble_now(struct uni_hid_device_s* d,
                                                   uint8_t weak_magnitude,
                                                   uint8_t strong_magnitude,
                                                   uint8_t trigger_left,
                                                   uint8_t trigger_right);

void uni_hid_parser_stadia_setup(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("Stadia: Invalid device\n");
        return;
    }

    uni_hid_parser_rumble_init(d, stadia_start_rumble_now, stadia_stop_rumble_now);

    uni_hid_device_set_ready_complete(d);
}

void uni_hid_parser_stadia_play_dual_rumble(struct uni_hid_device_s* d,
                                            uint16_t start_delay_ms,
                                            uint16_t duration_ms,
                                            uint8_t weak_magnitude,
                                            uint8_t strong_magnitude) {
    if (d == NULL) {
        loge("Stadia: Invalid device\n");
        return;
    }

    uni_hid_parser_rumble_play_dual(d, start_delay_ms, duration_ms, weak_magnitude, strong_magnitude,
                                    stadia_start_rumble_now, stadia_stop_rumble_now);
}

//
// Helpers
//
static uni_rumble_result_t stadia_stop_rumble_now(struct uni_hid_device_s* d) {
    uint8_t status;

    const struct stadia_ff_report ff = {
        .strong_magnitude = 0,
        .weak_magnitude = 0,
    };

    status = hids_host_send_write_report(d->hids_cid, STADIA_RUMBLE_REPORT_ID, HID_REPORT_TYPE_OUTPUT,
                                         (const uint8_t*)&ff, sizeof(ff));
    if (status == ERROR_CODE_COMMAND_DISALLOWED) {
        logd("Stadia: Failed to turn off rumble, error=%#x, retrying...\n", status);
        return UNI_RUMBLE_RETRY_BLE;
    } else if (status != ERROR_CODE_SUCCESS) {
        // Do nothing, just log the error
        logi("Stadia: Failed to turn off rumble, error=%#x\n", status);
        return UNI_RUMBLE_ERR;
    }
    // else, SUCCESS
    return UNI_RUMBLE_OK;
}

static uni_rumble_result_t stadia_start_rumble_now(struct uni_hid_device_s* d,
                                                   uint8_t weak_magnitude,
                                                   uint8_t strong_magnitude,
                                                   uint8_t trigger_left,
                                                   uint8_t trigger_right) {
    uint8_t status;
    ARG_UNUSED(trigger_left);
    ARG_UNUSED(trigger_right);

    const struct stadia_ff_report ff = {
        .strong_magnitude = strong_magnitude << 8,
        .weak_magnitude = weak_magnitude << 8,
    };

    status = hids_host_send_write_report(d->hids_cid, STADIA_RUMBLE_REPORT_ID, HID_REPORT_TYPE_OUTPUT,
                                         (const uint8_t*)&ff, sizeof(ff));
    if (status == ERROR_CODE_COMMAND_DISALLOWED) {
        logd("Stadia: Failed to send rumble report, error=%#x, retrying...\n", status);
        return UNI_RUMBLE_RETRY_BLE;
    } else if (status != ERROR_CODE_SUCCESS) {
        // Don't retry, just log the error and return
        logi("Stadia: Failed to send rumble report, error=%#x\n", status);
        return UNI_RUMBLE_ERR;
    }

    return UNI_RUMBLE_OK;
}
