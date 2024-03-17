// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Ricardo Quesada
// http://retro.moe/unijoysticle2

// FF structure based on:
// https://git.kernel.org/pub/scm/linux/kernel/git/hid/hid.git/commit/?h=for-next&id=24175157b8520de2ed6219676bddb08c846f2d0d

#include "parser/uni_hid_parser_stadia.h"

#include "controller/uni_controller.h"
#include "uni_hid_device.h"
#include "uni_log.h"

#define STADIA_RUMBLE_REPORT_ID 0x05

struct stadia_ff_report {
    uint16_t strong_magnitude;
    uint16_t weak_magnitude;
} __attribute__((packed));

// stadia_instance_t represents data used by the Stadia driver instance.
typedef struct stadia_instance_s {
    btstack_timer_source_t rumble_timer;
    bool rumble_in_progress;
} stadia_instance_t;
_Static_assert(sizeof(stadia_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "Stadia instance too big");

static stadia_instance_t* get_stadia_instance(uni_hid_device_t* d);
static void stadia_set_rumble_off(btstack_timer_source_t* ts);

void uni_hid_parser_stadia_setup(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("Stadia: Invalid device\n");
        return;
    }

    stadia_instance_t* ins = get_stadia_instance(d);
    memset(ins, 0, sizeof(*ins));

    uni_hid_device_set_ready_complete(d);
}

void uni_hid_parser_stadia_set_rumble(uni_hid_device_t* d, uint8_t value, uint8_t duration) {
    uint8_t status;

    if (d == NULL) {
        loge("Stadia: Invalid device\n");
        return;
    }

    stadia_instance_t* ins = get_stadia_instance(d);
    if (ins->rumble_in_progress)
        return;

    // TODO: It seems that the max value is 65356. Double check
    uint16_t magnitude = value << 8;

    const struct stadia_ff_report ff = {
        .strong_magnitude = magnitude,
        .weak_magnitude = magnitude,
    };

    status = hids_client_send_write_report(d->hids_cid, STADIA_RUMBLE_REPORT_ID, HID_REPORT_TYPE_OUTPUT,
                                           (const uint8_t*)&ff, sizeof(ff));
    if (status != ERROR_CODE_SUCCESS) {
        loge("Stadia: Failed to send rumble report, error=%#x\n", status);
        return;
    }

    // Set timer to turn off rumble
    ins->rumble_timer.process = &stadia_set_rumble_off;
    ins->rumble_timer.context = d;
    ins->rumble_in_progress = 1;
    int ms = duration * 4;  // duration: 256 ~= 1 second
    btstack_run_loop_set_timer(&ins->rumble_timer, ms);
    btstack_run_loop_add_timer(&ins->rumble_timer);
}

//
// Helpers
//
static stadia_instance_t* get_stadia_instance(uni_hid_device_t* d) {
    return (stadia_instance_t*)&d->parser_data[0];
}
static void stadia_set_rumble_off(btstack_timer_source_t* ts) {
    uint8_t status;
    uni_hid_device_t* d = ts->context;
    stadia_instance_t* ins = get_stadia_instance(d);

    // No need to protect it with a mutex since it runs in the same main thread
    assert(ins->rumble_in_progress);
    ins->rumble_in_progress = 0;

    const struct stadia_ff_report ff = {
        .strong_magnitude = 0,
        .weak_magnitude = 0,
    };

    status = hids_client_send_write_report(d->hids_cid, STADIA_RUMBLE_REPORT_ID, HID_REPORT_TYPE_OUTPUT,
                                           (const uint8_t*)&ff, sizeof(ff));
    if (status != ERROR_CODE_SUCCESS)
        loge("Stadia: Failed to turn off rumble, error=%#x\n", status);
}
