// SPDX-License-Identifier: Apache-2.0
// Copyright 2022 Ricardo Quesada
// http://retro.moe/unijoysticle2

/*
 * Based on SmallyMouse2 by Simon Inns
 * https://github.com/simoninns/SmallyMouse2
 */
#include "uni_mouse_quadrature.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <sys/cdefs.h>

#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "uni_log.h"
#include "uni_property.h"

// Probably I could use a smaller divider, and only do "1 tick per 80us".
// That would work Ok except that it will lose resolution when we divide "128 steps by delta".
// APB clock runs at 80Mhz.
//   Option A:
//   80Mhz / 80 = 1Mhz = tick every 1us
// #define TIMER_DIVIDER (80)
// #define TICKS_PER_80US (80)  // How many ticks are in 80us
// #define ONE_SECOND (1000000)
//
//   Option B:
//   80Mhz / 6400 = 12500Hz = tick every 80us
#define TIMER_RESOLUTION_HZ (12500)
#define TICKS_PER_80US (1)  // How many ticks are in 80us, with 12500Hz resolution
#define ONE_SECOND (12500)

#define TASK_TIMER_STACK_SIZE (4096)
#define TASK_TIMER_PRIO (10)

// Quadrature phases
#define QUADRATURE_PHASES 4

// Helper to pack port and encoder indexes into a single argument for a task.
#define PACK_TIMER_ARG(port, encoder) ((void*)(((port) << 16) | (encoder)))

enum direction {
    PHASE_DIRECTION_NEG,
    PHASE_DIRECTION_POS,
};

// A mouse has two encoders.
struct quadrature_state {
    // horizontal or vertical encoder?
    int encoder;
    // Current direction
    enum direction dir;
    // Current value
    int value;
    // Current quadrature phase
    int phase;

    // GPIOs used
    struct uni_mouse_quadrature_encoder_gpios gpios;
};

static struct quadrature_state s_quadratures[UNI_MOUSE_QUADRATURE_PORT_MAX][UNI_MOUSE_QUADRATURE_ENCODER_MAX];
// Cache to prevent enabling/disabling timers that were already enabled/disabled
static bool timer_started[UNI_MOUSE_QUADRATURE_PORT_MAX];

static gptimer_handle_t s_gptimers[UNI_MOUSE_QUADRATURE_PORT_MAX][UNI_MOUSE_QUADRATURE_ENCODER_MAX];
static TaskHandle_t s_timer_tasks[UNI_MOUSE_QUADRATURE_PORT_MAX][UNI_MOUSE_QUADRATURE_ENCODER_MAX];

// "Scale factor" for mouse movement. To make the mouse move faster or slower.
// Bigger means slower movement.
static float s_scale_factor;

static bool initialized;

// Quadrature phase outputs for GPIO A and B.
// Index is the phase (0-3).
static const int phase_to_a[] = {0, 1, 1, 0};
static const int phase_to_b[] = {0, 0, 1, 1};

static void process_quadrature(struct quadrature_state* q) {
    if (q->value <= 0) {
        return;
    }
    q->value--;

    if (q->dir == PHASE_DIRECTION_NEG) {
        q->phase = (q->phase - 1 + QUADRATURE_PHASES) % QUADRATURE_PHASES;
    } else /* PHASE_DIRECTION_POS */ {
        q->phase = (q->phase + 1) % QUADRATURE_PHASES;
    }

    int a = phase_to_a[q->phase];
    int b = phase_to_b[q->phase];

    int gpio_a = q->gpios.a;
    int gpio_b = q->gpios.b;
    gpio_set_level(gpio_a, a);
    gpio_set_level(gpio_b, b);
    logd("value: %d, quadrature phase: %d, a=%d, b=%d (%d,%d)\n", q->value, q->phase, a, b, gpio_a, gpio_b);
}

// This function is the entry point for each of the timer tasks.
static void timer_task(void* arg) {
    uint32_t task_arg = (uint32_t)arg;
    uint16_t port_idx = (task_arg >> 16);
    uint16_t encoder_idx = (task_arg & 0xffff);

    while (true) {
        ulTaskNotifyTake(true, portMAX_DELAY);
        process_quadrature(&s_quadratures[port_idx][encoder_idx]);
    }
}

static bool IRAM_ATTR timer_handler(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* user_ctx) {
    ARG_UNUSED(timer);
    ARG_UNUSED(edata);
    uint32_t task_arg = (uint32_t)user_ctx;
    uint16_t port_idx = (task_arg >> 16);
    uint16_t encoder_idx = (task_arg & 0xffff);

    BaseType_t high_task_awoken = pdFALSE;
    vTaskNotifyGiveFromISR(s_timer_tasks[port_idx][encoder_idx], &high_task_awoken);
    return (high_task_awoken == pdTRUE);
}

static void init_from_cpu_task() {
    // From ESP-IDF documentation:
    // "Register Timer interrupt handler, the handler is an ISR.
    // The handler will be attached to the same CPU core that this function is running on."

    // Create timers
    /* Select and initialize basic parameters of the timer */
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_DOWN,
        .resolution_hz = TIMER_RESOLUTION_HZ,
    };

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_handler,
    };

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = ONE_SECOND * 60,
        .flags.auto_reload_on_alarm = true,
    };

    for (int i = 0; i < UNI_MOUSE_QUADRATURE_PORT_MAX; i++) {
        for (int j = 0; j < UNI_MOUSE_QUADRATURE_ENCODER_MAX; j++) {
            void* arg = PACK_TIMER_ARG(i, j);
            char name[32];
            ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &s_gptimers[i][j]));
            ESP_ERROR_CHECK(gptimer_register_event_callbacks(s_gptimers[i][j], &cbs, arg));
            ESP_ERROR_CHECK(gptimer_enable(s_gptimers[i][j]));
            ESP_ERROR_CHECK(gptimer_set_alarm_action(s_gptimers[i][j], &alarm_config));
            ESP_ERROR_CHECK(gptimer_set_raw_count(s_gptimers[i][j], ONE_SECOND * 60));

            // Create timer tasks
            sprintf(name, "bp.quad.timer%d%c", i, (j == 0) ? 'H' : 'V');
            xTaskCreatePinnedToCore(timer_task, name, TASK_TIMER_STACK_SIZE, arg, TASK_TIMER_PRIO, &s_timer_tasks[i][j],
                                    xPortGetCoreID());
        }
    }

    // Kill itself
    vTaskDelete(NULL);
}

static void process_update(struct quadrature_state* q, int32_t delta) {
    uint64_t count_value;
    int32_t abs_delta = (delta < 0) ? -delta : delta;

    if (delta != 0) {
        // Hack to make the movement smoother with "small deltas". It guarantees that
        // at least two phases (out of 4) of the quadrature will be run.
        // if (abs_delta < 2)
        //     abs_delta = 2;
        /* Don't update the phase, it should start from the previous phase */
        q->value = abs_delta;
        q->dir = (delta < 0) ? PHASE_DIRECTION_NEG : PHASE_DIRECTION_POS;

        // SmallyMouse2 mentions that 100-120 reports are received per second.
        // According to my test, they are ~90, which is in the same order.
        // For simplicity, I'll use 100. It means that, at most, reports are received
        // every 10ms (1 second / 100 reports = 10ms per report).
        //
        // "delta" is a "somewhat normalized" value that goes from 0 to 127.
        // So we should split 10 milliseconds (ms) in 128 steps = ~80 microseconds (us).
        // That's good because the minimum "ISR" in ESP32 is 50us.
        // So the ESP32 timer is configured as:
        // - down timer
        // - ticks every 1us
        // - when it reaches 0, triggers the ISR.
        //
        // But a quadrature has 4 states (hence the name). So takes 4 "ticks" to have
        // complete "state.", which is represented with "s_scale_factor",
        // kind of "hand tuned" so that the mice movement feels "good" (to me).
        //
        // The smaller "units" is, the faster the mouse moves.
        //
        // But to avoid a "division" in the mouse driver, and a multiplication here,
        // (which will lose precision), we just use a "s_scale_factor" of 1 instead of 4,
        // and we don't divide by 4 here.
        // Alternative: Do not divide the time, and use a constant "tick" time. But if we do so,
        // the movement will have "jank".
        // Perhaps for small deltas we can have a predefined "count_value time".
        //
        // s_scale_factor is used as a divisor to honor the "scale" name:
        // smaller numbers make it slower, high number faster
        float max_ticks = 128 * TICKS_PER_80US;
        float delta_f = abs_delta;
        float units_f = max_ticks / (delta_f * s_scale_factor);
        if (units_f < TICKS_PER_80US)
            units_f = TICKS_PER_80US;
        count_value = roundf(units_f);
    } else {
        // If there is no update, set timer to update less frequently
        count_value = ONE_SECOND * 60;
    }
    // Find which timer corresponds to this quadrature state
    int port_idx = (q - &s_quadratures[0][0]) / UNI_MOUSE_QUADRATURE_ENCODER_MAX;
    int encoder_idx = (q - &s_quadratures[0][0]) % UNI_MOUSE_QUADRATURE_ENCODER_MAX;

    gptimer_set_raw_count(s_gptimers[port_idx][encoder_idx], count_value);
}

void uni_mouse_quadrature_init(int cpu_id) {
    memset(s_quadratures, 0, sizeof(s_quadratures));

    for (int i = 0; i < UNI_MOUSE_QUADRATURE_PORT_MAX; i++) {
        timer_started[i] = false;
    }

    // Default value that can be overridden from the console
    s_scale_factor = uni_mouse_quadrature_get_scale_factor();

    // Create tasks
    xTaskCreatePinnedToCore(init_from_cpu_task, "uni.init_timers", TASK_TIMER_STACK_SIZE, NULL, TASK_TIMER_PRIO, NULL,
                            cpu_id);

    initialized = true;
}

void uni_mouse_quadrature_setup_port(int port_idx,
                                     struct uni_mouse_quadrature_encoder_gpios h,
                                     struct uni_mouse_quadrature_encoder_gpios v) {
    if (port_idx < 0 || port_idx >= UNI_MOUSE_QUADRATURE_PORT_MAX) {
        loge("%s: Invalid port idx=%d\n", __func__, port_idx);
        return;
    }
    s_quadratures[port_idx][UNI_MOUSE_QUADRATURE_ENCODER_H].gpios = h;
    s_quadratures[port_idx][UNI_MOUSE_QUADRATURE_ENCODER_V].gpios = v;
}

void uni_mouse_quadrature_deinit(void) {
    // Stop the timers
    for (int i = 0; i < UNI_MOUSE_QUADRATURE_PORT_MAX; i++) {
        for (int j = 0; j < UNI_MOUSE_QUADRATURE_ENCODER_MAX; j++) {
            gptimer_del_timer(s_gptimers[i][j]);
            s_gptimers[i][j] = NULL;
            vTaskDelete(s_timer_tasks[i][j]);
            s_timer_tasks[i][j] = NULL;
        }
    }

    initialized = false;
}

void uni_mouse_quadrature_start(int port_idx) {
    if (!initialized) {
        loge("%s: Error, Not initialized\n");
        return;
    }

    if (port_idx < 0 || port_idx >= UNI_MOUSE_QUADRATURE_PORT_MAX) {
        loge("%s: Invalid port idx=%d\n", __func__, port_idx);
        return;
    }

    if (timer_started[port_idx])
        return;

    for (int j = 0; j < UNI_MOUSE_QUADRATURE_ENCODER_MAX; j++)
        gptimer_start(s_gptimers[port_idx][j]);

    timer_started[port_idx] = true;
}

void uni_mouse_quadrature_pause(int port_idx) {
    if (!initialized) {
        loge("%s: Error, Not initialized\n");
        return;
    }

    if (port_idx < 0 || port_idx >= UNI_MOUSE_QUADRATURE_PORT_MAX) {
        loge("%s: Invalid port idx=%d\n", __func__, port_idx);
        return;
    }

    if (!timer_started[port_idx])
        return;

    for (int j = 0; j < UNI_MOUSE_QUADRATURE_ENCODER_MAX; j++)
        gptimer_stop(s_gptimers[port_idx][j]);

    timer_started[port_idx] = false;
}

// Should be called everytime that mouse report is received.
void uni_mouse_quadrature_update(int port_idx, int32_t dx, int32_t dy) {
    if (!initialized) {
        loge("%s: Error, Not initialized\n");
        return;
    }
    if (port_idx < 0 || port_idx >= UNI_MOUSE_QUADRATURE_PORT_MAX) {
        loge("%s: Invalid port idx=%d\n", __func__, port_idx);
        return;
    }
    process_update(&s_quadratures[port_idx][UNI_MOUSE_QUADRATURE_ENCODER_H], dx);
    // Invert delta Y so that mouse goes the right direction.
    // This is based on empiric evidence. Also, it seems that SmallyMouse is doing the same thing
    process_update(&s_quadratures[port_idx][UNI_MOUSE_QUADRATURE_ENCODER_V], -dy);
}

void uni_mouse_quadrature_set_scale_factor(float scale) {
    uni_property_value_t value;
    value.f32 = scale;

    s_scale_factor = scale;
    uni_property_set(UNI_PROPERTY_IDX_MOUSE_SCALE, value);
}

float uni_mouse_quadrature_get_scale_factor(void) {
    uni_property_value_t value;

    value = uni_property_get(UNI_PROPERTY_IDX_MOUSE_SCALE);
    s_scale_factor = value.f32;
    return value.f32;
}
