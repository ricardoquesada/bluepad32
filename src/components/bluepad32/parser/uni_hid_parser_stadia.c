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

#define BLE_RETRY_MS 50

struct stadia_ff_report {
    uint16_t strong_magnitude;  // Left: 2100 RPM
    uint16_t weak_magnitude;    // Right: 3350 RPM
} __attribute__((packed));

enum {
    STATE_RUMBLE_DISABLED,
    STATE_RUMBLE_DELAYED,
    STATE_RUMBLE_IN_PROGRESS,
};

// stadia_instance_t represents data used by the Stadia driver instance.
typedef struct stadia_instance_s {
    // Although technically, we can use one timer for delay and duration, easier to debug/maintain if we have two.
    btstack_timer_source_t rumble_timer_duration;
    btstack_timer_source_t rumble_timer_delayed_start;
    int rumble_state;
    // Used by delayed start
    uint16_t rumble_weak_magnitude;
    uint16_t rumble_strong_magnitude;
    uint16_t rumble_duration_ms;
} stadia_instance_t;
_Static_assert(sizeof(stadia_instance_t) < HID_DEVICE_MAX_PARSER_DATA, "Stadia instance too big");

static stadia_instance_t* get_stadia_instance(uni_hid_device_t* d);
static void on_stadia_set_rumble_on(btstack_timer_source_t* ts);
static void on_stadia_set_rumble_off(btstack_timer_source_t* ts);
static void stadia_stop_rumble_now(uni_hid_device_t* d);
static void stadia_play_dual_rumble_now(uni_hid_device_t* d,
                                        uint16_t duration_ms,
                                        uint8_t weak_magnitude,
                                        uint8_t strong_magnitude);

void uni_hid_parser_stadia_setup(uni_hid_device_t* d) {
    if (d == NULL) {
        loge("Stadia: Invalid device\n");
        return;
    }

    stadia_instance_t* ins = get_stadia_instance(d);
    memset(ins, 0, sizeof(*ins));

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

    stadia_instance_t* ins = get_stadia_instance(d);
    switch (ins->rumble_state) {
        case STATE_RUMBLE_DELAYED:
            btstack_run_loop_remove_timer(&ins->rumble_timer_delayed_start);
            break;
        case STATE_RUMBLE_IN_PROGRESS:
            btstack_run_loop_remove_timer(&ins->rumble_timer_duration);
            break;
        default:
            // Do nothing
            break;
    }

    if (start_delay_ms == 0) {
        stadia_play_dual_rumble_now(d, duration_ms, weak_magnitude, strong_magnitude);
    } else {
        // Set timer to have a delayed start
        ins->rumble_timer_delayed_start.process = &on_stadia_set_rumble_on;
        ins->rumble_timer_delayed_start.context = d;
        ins->rumble_state = STATE_RUMBLE_DELAYED;
        ins->rumble_duration_ms = duration_ms;
        ins->rumble_strong_magnitude = strong_magnitude;
        ins->rumble_weak_magnitude = weak_magnitude;

        btstack_run_loop_set_timer(&ins->rumble_timer_delayed_start, start_delay_ms);
        btstack_run_loop_add_timer(&ins->rumble_timer_delayed_start);
    }
}

//
// Helpers
//
static void stadia_stop_rumble_now(uni_hid_device_t* d) {
    uint8_t status;
    stadia_instance_t* ins = get_stadia_instance(d);

    // No need to protect it with a mutex since it runs in the same main thread
    assert(ins->rumble_state != STATE_RUMBLE_DISABLED);
    ins->rumble_state = STATE_RUMBLE_DISABLED;

    const struct stadia_ff_report ff = {
        .strong_magnitude = 0,
        .weak_magnitude = 0,
    };

    status = hids_client_send_write_report(d->hids_cid, STADIA_RUMBLE_REPORT_ID, HID_REPORT_TYPE_OUTPUT,
                                           (const uint8_t*)&ff, sizeof(ff));
    if (status == ERROR_CODE_COMMAND_DISALLOWED) {
        logd("Stadia: Failed to turn off rumble, error=%#x, retrying...\n", status);
        ins->rumble_timer_duration.process = &on_stadia_set_rumble_off;
        ins->rumble_timer_duration.context = d;
        ins->rumble_state = STATE_RUMBLE_IN_PROGRESS;

        btstack_run_loop_set_timer(&ins->rumble_timer_duration, BLE_RETRY_MS);
        btstack_run_loop_add_timer(&ins->rumble_timer_duration);
    } else if (status != ERROR_CODE_SUCCESS) {
        // Do nothing, just log the error
        logi("Stadia: Failed to turn off rumble, error=%#x\n", status);
    }
    // else, SUCCESS
}

static void stadia_play_dual_rumble_now(uni_hid_device_t* d,
                                        uint16_t duration_ms,
                                        uint8_t weak_magnitude,
                                        uint8_t strong_magnitude) {
    uint8_t status;

    stadia_instance_t* ins = get_stadia_instance(d);

    if (duration_ms == 0) {
        if (ins->rumble_state != STATE_RUMBLE_DISABLED)
            stadia_stop_rumble_now(d);
        return;
    }

    const struct stadia_ff_report ff = {
        .strong_magnitude = strong_magnitude << 8,
        .weak_magnitude = weak_magnitude << 8,
    };

    status = hids_client_send_write_report(d->hids_cid, STADIA_RUMBLE_REPORT_ID, HID_REPORT_TYPE_OUTPUT,
                                           (const uint8_t*)&ff, sizeof(ff));
    if (status == ERROR_CODE_COMMAND_DISALLOWED) {
        logd("Stadia: Failed to send rumble report, error=%#x, retrying...\n", status);
        ins->rumble_timer_delayed_start.process = &on_stadia_set_rumble_on;
        ins->rumble_timer_delayed_start.context = d;
        ins->rumble_state = STATE_RUMBLE_DELAYED;

        btstack_run_loop_set_timer(&ins->rumble_timer_delayed_start, BLE_RETRY_MS);
        btstack_run_loop_add_timer(&ins->rumble_timer_delayed_start);
        return;
    } else if (status != ERROR_CODE_SUCCESS) {
        // Don't retry, just log the error and return
        logi("Stadia: Failed to send rumble report, error=%#x\n", status);
        return;
    }

    // Set timer to turn off rumble
    ins->rumble_timer_duration.process = &on_stadia_set_rumble_off;
    ins->rumble_timer_duration.context = d;
    ins->rumble_state = STATE_RUMBLE_IN_PROGRESS;
    btstack_run_loop_set_timer(&ins->rumble_timer_duration, duration_ms);
    btstack_run_loop_add_timer(&ins->rumble_timer_duration);
}

static stadia_instance_t* get_stadia_instance(uni_hid_device_t* d) {
    return (stadia_instance_t*)&d->parser_data[0];
}

static void on_stadia_set_rumble_on(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    stadia_instance_t* ins = get_stadia_instance(d);

    stadia_play_dual_rumble_now(d, ins->rumble_duration_ms, ins->rumble_weak_magnitude, ins->rumble_strong_magnitude);
}

static void on_stadia_set_rumble_off(btstack_timer_source_t* ts) {
    uni_hid_device_t* d = ts->context;
    stadia_stop_rumble_now(d);
}
