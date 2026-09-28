// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "parser/uni_hid_parser_rumble.h"

#include <stdint.h>
#include <string.h>

#include "uni_hid_device.h"
#include "uni_log.h"

static void on_rumble_delayed_start(btstack_timer_source_t* ts);
static void on_rumble_duration_expired(btstack_timer_source_t* ts);
static void rumble_start_now(struct uni_hid_device_s* d);
static void rumble_stop_now(struct uni_hid_device_s* d);

void uni_hid_parser_rumble_stop_timers(struct uni_hid_device_s* d) {
    if (d == NULL) {
        return;
    }
    btstack_run_loop_remove_timer(&d->rumble.timer_delayed_start);
    btstack_run_loop_remove_timer(&d->rumble.timer_duration);
    d->rumble.state = UNI_RUMBLE_STATE_DISABLED;
}

void uni_hid_parser_rumble_init(struct uni_hid_device_s* d,
                                uni_rumble_start_fn_t start_fn,
                                uni_rumble_stop_fn_t stop_fn) {
    if (d == NULL) {
        return;
    }
    uni_hid_parser_rumble_stop_timers(d);
    memset(&d->rumble, 0, sizeof(d->rumble));
    d->rumble.start_fn = start_fn;
    d->rumble.stop_fn = stop_fn;
}

bool uni_hid_parser_rumble_is_in_progress(const struct uni_hid_device_s* d) {
    if (d == NULL) {
        return false;
    }
    return d->rumble.state == UNI_RUMBLE_STATE_IN_PROGRESS;
}

void uni_hid_parser_rumble_play_dual(struct uni_hid_device_s* d,
                                     uint16_t start_delay_ms,
                                     uint16_t duration_ms,
                                     uint8_t weak_magnitude,
                                     uint8_t strong_magnitude,
                                     uni_rumble_start_fn_t default_start_fn,
                                     uni_rumble_stop_fn_t default_stop_fn) {
    uni_hid_parser_rumble_play_quad(d, start_delay_ms, duration_ms, weak_magnitude, strong_magnitude,
                                    0 /* trigger_left */, 0 /* trigger_right */, default_start_fn, default_stop_fn);
}

void uni_hid_parser_rumble_play_quad(struct uni_hid_device_s* d,
                                     uint16_t start_delay_ms,
                                     uint16_t duration_ms,
                                     uint8_t weak_magnitude,
                                     uint8_t strong_magnitude,
                                     uint8_t trigger_left,
                                     uint8_t trigger_right,
                                     uni_rumble_start_fn_t default_start_fn,
                                     uni_rumble_stop_fn_t default_stop_fn) {
    if (d == NULL) {
        loge("Rumble: Invalid device\n");
        return;
    }

    if (d->rumble.start_fn == NULL) {
        d->rumble.start_fn = default_start_fn;
    }
    if (d->rumble.stop_fn == NULL) {
        d->rumble.stop_fn = default_stop_fn;
    }

    uni_rumble_state_t prev_state = d->rumble.state;
    switch (prev_state) {
        case UNI_RUMBLE_STATE_DELAYED:
            btstack_run_loop_remove_timer(&d->rumble.timer_delayed_start);
            break;
        case UNI_RUMBLE_STATE_IN_PROGRESS:
            btstack_run_loop_remove_timer(&d->rumble.timer_duration);
            break;
        case UNI_RUMBLE_STATE_DISABLED:
        default:
            break;
    }

    // Persist duration and actuator magnitudes before attempting an immediate send so that
    // a 50ms BLE retry (UNI_RUMBLE_RETRY_BLE) preserves them (Fixes B4 / Latent Bug 2A).
    d->rumble.duration_ms = duration_ms;
    d->rumble.weak_magnitude = weak_magnitude;
    d->rumble.strong_magnitude = strong_magnitude;
    d->rumble.trigger_left = trigger_left;
    d->rumble.trigger_right = trigger_right;

    if (start_delay_ms == 0) {
        if (duration_ms == 0) {
            if (prev_state == UNI_RUMBLE_STATE_IN_PROGRESS) {
                rumble_stop_now(d);
            } else {
                // If prev_state was DELAYED, the timer was already disarmed above and the hardware
                // motors have not started yet; transition directly to DISABLED without sending an
                // unnecessary stop report (fixes B4 assertion crash in DS4/Switch and stuck state in DS3/Wii/PSMove).
                d->rumble.state = UNI_RUMBLE_STATE_DISABLED;
            }
            return;
        }
        rumble_start_now(d);
    } else {
        d->rumble.timer_delayed_start.process = &on_rumble_delayed_start;
        d->rumble.timer_delayed_start.context = d;
        d->rumble.state = UNI_RUMBLE_STATE_DELAYED;
        btstack_run_loop_set_timer(&d->rumble.timer_delayed_start, start_delay_ms);
        btstack_run_loop_add_timer(&d->rumble.timer_delayed_start);
    }
}

static void rumble_stop_now(struct uni_hid_device_s* d) {
    d->rumble.state = UNI_RUMBLE_STATE_DISABLED;
    if (d->rumble.stop_fn == NULL) {
        return;
    }

    uni_rumble_result_t res = d->rumble.stop_fn(d);
    if (res == UNI_RUMBLE_RETRY_BLE) {
        // Explicitly bind .process and .context = d before arming the 50ms BLE stop retry timer
        // in case timer_duration was never previously armed on this device slot.
        btstack_run_loop_remove_timer(&d->rumble.timer_duration);
        d->rumble.timer_duration.process = &on_rumble_duration_expired;
        d->rumble.timer_duration.context = d;
        d->rumble.state = UNI_RUMBLE_STATE_IN_PROGRESS;
        btstack_run_loop_set_timer(&d->rumble.timer_duration, UNI_RUMBLE_BLE_RETRY_MS);
        btstack_run_loop_add_timer(&d->rumble.timer_duration);
    }
}

static void rumble_start_now(struct uni_hid_device_s* d) {
    if (d->rumble.duration_ms == 0) {
        if (d->rumble.state == UNI_RUMBLE_STATE_IN_PROGRESS) {
            rumble_stop_now(d);
        } else {
            d->rumble.state = UNI_RUMBLE_STATE_DISABLED;
        }
        return;
    }

    if (d->rumble.start_fn == NULL) {
        d->rumble.state = UNI_RUMBLE_STATE_DISABLED;
        return;
    }

    uni_rumble_result_t res = d->rumble.start_fn(d, d->rumble.weak_magnitude, d->rumble.strong_magnitude,
                                                 d->rumble.trigger_left, d->rumble.trigger_right);
    switch (res) {
        case UNI_RUMBLE_OK:
            btstack_run_loop_remove_timer(&d->rumble.timer_duration);
            d->rumble.timer_duration.process = &on_rumble_duration_expired;
            d->rumble.timer_duration.context = d;
            d->rumble.state = UNI_RUMBLE_STATE_IN_PROGRESS;
            btstack_run_loop_set_timer(&d->rumble.timer_duration, d->rumble.duration_ms);
            btstack_run_loop_add_timer(&d->rumble.timer_duration);
            break;
        case UNI_RUMBLE_RETRY_BLE:
            // Explicitly bind .process and .context = d here as well: when start_delay_ms == 0,
            // timer_delayed_start was bypassed and its context/handler would otherwise be NULL
            // when the 50ms BLE retry expires (fixes Latent Bug 2B).
            btstack_run_loop_remove_timer(&d->rumble.timer_delayed_start);
            d->rumble.timer_delayed_start.process = &on_rumble_delayed_start;
            d->rumble.timer_delayed_start.context = d;
            d->rumble.state = UNI_RUMBLE_STATE_DELAYED;
            btstack_run_loop_set_timer(&d->rumble.timer_delayed_start, UNI_RUMBLE_BLE_RETRY_MS);
            btstack_run_loop_add_timer(&d->rumble.timer_delayed_start);
            break;
        case UNI_RUMBLE_ERR:
        default:
            d->rumble.state = UNI_RUMBLE_STATE_DISABLED;
            break;
    }
}

static void on_rumble_delayed_start(btstack_timer_source_t* ts) {
    btstack_run_loop_remove_timer(ts);
    uni_hid_device_t* d = btstack_run_loop_get_timer_context(ts);
    if (d == NULL) {
        return;
    }
    rumble_start_now(d);
}

static void on_rumble_duration_expired(btstack_timer_source_t* ts) {
    btstack_run_loop_remove_timer(ts);
    uni_hid_device_t* d = btstack_run_loop_get_timer_context(ts);
    if (d == NULL) {
        return;
    }
    rumble_stop_now(d);
}
