/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2022 Ricardo Quesada

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

/*
 * Based on SmallyMouse2 by Simon Inns
 * https://github.com/simoninns/SmallyMouse2
 */
#include "uni_mouse_quadrature.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <driver/gpio.h>
#include <driver/timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "uni_debug.h"

/*
 * FIXME: only supports one mouse
 * Should be refactored to support multiple mice, or be able to swap the ports.
 * E.g: Amiga 4000 has the port#1 and port#2 swapped.
 */

// Probably I could use a smaller divider, and only do "1 tick per 80us".
// That would work Ok except that it will loose resolution when we divide "128 steps by delta".
// APB clock runs at 80Mhz.
//   Option A:
//   80Mhz / 80 = 1Mhz = tick every 1us
#define TIMER_DIVIDER (80)
#define TICKS_PER_80US (80)  // How many ticks are in 80us
//
//   Option B:
//   80Mhz / 6400 = 12500Hz = tick every 80us
// #define TIMER_DIVIDER (80 *80)
// #define TICKS_PER_80US (1) // How many ticks are in 80us
#define ONE_SECOND (1000000)

#define TASK_TIMER_STACK_SIZE (1024)
#define TASK_TIMER_PRIO (5)

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

    // Which timer is being used: 0 or 1
    timer_idx_t timer_idx;
};

// We have two mouse ports, but only one can be used at the time.
// This is because it should be possible to swap the mouse port.
// A real example: Amiga 4000 has port#2 at the left, port#1 at the right.
static struct uni_mouse_quadrature_port g_ports[UNI_MOUSE_QUADRATURE_PORT_MAX];
// Which port is active;
static int g_port_idx;

static struct quadrature_state s_quadrature_x;
static struct quadrature_state s_quadrature_y;

static TaskHandle_t s_timer_x_task;
static TaskHandle_t s_timer_y_task;

static void process_quadrature(struct quadrature_state* q) {
    int a, b;

    if (q->value <= 0) {
        return;
    }
    q->value--;

    if (q->dir == PHASE_DIRECTION_NEG) {
        q->phase--;
        if (q->phase < 0)
            q->phase = 3;
    } else /* PHASE_DIRECTION_POS */ {
        q->phase++;
        if (q->phase > 3)
            q->phase = 0;
    }

    switch (q->phase) {
        case 0:
            a = 0;
            b = 0;
            break;
        case 1:
            a = 1;
            b = 0;
            break;
        case 2:
            a = 1;
            b = 1;
            break;
        case 3:
            a = 0;
            b = 1;
            break;
        default:
            loge("%s: invalid phase value: %d", __func__, q->phase);
            a = b = 0;
            break;
    }

    // Get GPIOs from the correct port / encoder.
    int gpio_a = g_ports[g_port_idx].encoders[q->encoder].gpio_a;
    int gpio_b = g_ports[g_port_idx].encoders[q->encoder].gpio_b;
    gpio_set_level(gpio_a, a);
    gpio_set_level(gpio_b, b);
    logd("value: %d, quadrature phase: %d, a=%d, b=%d (%d,%d)\n", q->value, q->phase, a, b, gpio_a, gpio_b);
}

static void timer_x_task(void* arg) {
    while (true) {
        ulTaskNotifyTake(true, portMAX_DELAY);
        process_quadrature(&s_quadrature_x);
    }
}

static void timer_y_task(void* arg) {
    while (true) {
        ulTaskNotifyTake(true, portMAX_DELAY);
        process_quadrature(&s_quadrature_y);
    }
}

static bool IRAM_ATTR timer_x_handler(void* arg) {
    BaseType_t higher_priority_task_woken = false;
    vTaskNotifyGiveFromISR(s_timer_x_task, &higher_priority_task_woken);
    return higher_priority_task_woken;
}

static bool IRAM_ATTR timer_y_handler(void* arg) {
    BaseType_t higher_priority_task_woken = false;
    vTaskNotifyGiveFromISR(s_timer_y_task, &higher_priority_task_woken);
    return higher_priority_task_woken;
}

static void init_from_cpu1_task() {
    // From ESP-IDF documentation:
    // "Register Timer interrupt handler, the handler is an ISR.
    // The handler will be attached to the same CPU core that this function is running on."

    // Create timers
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_DOWN,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TIMER_AUTORELOAD_EN,
    };

    // FIXME: Don't start timers unless it is needed
    // Setup timer X
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_0, s_quadrature_x.timer_idx, &config));
    timer_set_counter_value(TIMER_GROUP_0, s_quadrature_x.timer_idx, ONE_SECOND * 60);
    timer_isr_callback_add(TIMER_GROUP_0, s_quadrature_x.timer_idx, timer_x_handler, NULL, 0);
    timer_start(TIMER_GROUP_0, s_quadrature_x.timer_idx);

    // Setup timer Y
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_0, s_quadrature_y.timer_idx, &config));
    timer_set_counter_value(TIMER_GROUP_0, s_quadrature_y.timer_idx, ONE_SECOND * 60);
    timer_isr_callback_add(TIMER_GROUP_0, s_quadrature_y.timer_idx, timer_y_handler, NULL, 0);
    timer_start(TIMER_GROUP_0, s_quadrature_y.timer_idx);

    // Create timer tasks
    xTaskCreatePinnedToCore(timer_x_task, "timer_x", TASK_TIMER_STACK_SIZE, NULL, TASK_TIMER_PRIO, &s_timer_x_task,
                            xPortGetCoreID());
    xTaskCreatePinnedToCore(timer_y_task, "timer_y", TASK_TIMER_STACK_SIZE, NULL, TASK_TIMER_PRIO, &s_timer_y_task,
                            xPortGetCoreID());

    // Kill itself
    vTaskDelete(NULL);
}

static void process_update(struct quadrature_state* q, int32_t delta) {
    uint64_t units;
    int32_t abs_delta = (delta < 0) ? -delta : delta;
    const int resolution = 1;

    if (delta != 0) {
        // Hack to make the movement smoother with "small deltas". It guarantees that
        // at least two phases (out of 4) of the quadrature will be run.
        if (abs_delta < 2)
            abs_delta = 2;
        /* Don't update the phase, it should start from the previous phase */
        q->value = abs_delta;
        q->dir = (delta < 0) ? PHASE_DIRECTION_NEG : PHASE_DIRECTION_POS;

        // SmallyMouse2 mentions that 100-120 reports are recevied per second.
        // According to my test they are ~90, which is in the same order.
        // For simplicity, I'll use 100. It means that, at most, reports are received
        // every 10ms (1 second / 100 reports = 10ms per report).
        //
        // "delta" is a "somewhat normalized" value that goes from 0 to 127.
        // So we should split 10 milliseconds (ms) in 128 steps = ~80 microseconds (us).
        // That's good because the minimum "ISR" in ESP32 is 50us.
        // So the ESP32 timer is configured as:
        // - down timer
        // - ticks every 1us
        // - when it reaches 0, triggers the ISR
        //
        // But a quadrature has 4 states (hence the name). So takes 4 "ticks" to have
        // complete "state.", which is represented with "resolution", kind of "hand tuned"
        // so that the mice movement feels "good" (to me).
        //
        // The smaller "units" is, the faster the mouse moves.
        //
        // But in order to avoid a "division" in the mouse driver, and a multiplication here,
        // (which will  loose precision), we just use a "resolution" of 1 instead of 4,
        // and we don't divide by 4 here.
        // Alternative: Do not divide the time, and use a constant "tick" time. But if we do so,
        // the movement will have "jank".
        // Perhaps for small deltas we can have a predefined "unit time"
        units = (128 * TICKS_PER_80US) / (abs_delta * resolution);
        if (units < TICKS_PER_80US)
            units = TICKS_PER_80US;
    } else {
        // If there is no update, set timer to update less frequently
        units = ONE_SECOND * 10;
    }
    timer_set_counter_value(TIMER_GROUP_0, q->timer_idx, units);
}

void uni_mouse_quadrature_init(int cpu_id,
                               struct uni_mouse_quadrature_port port_a,
                               struct uni_mouse_quadrature_port port_b) {
    g_port_idx = 0;
    g_ports[UNI_MOUSE_QUADRATURE_PORT_0] = port_a;
    g_ports[UNI_MOUSE_QUADRATURE_PORT_1] = port_b;

    memset(&s_quadrature_x, 0, sizeof(s_quadrature_x));
    memset(&s_quadrature_y, 0, sizeof(s_quadrature_y));

    s_quadrature_x.encoder = UNI_MOUSE_QUADRATURE_ENCODER_H;
    s_quadrature_x.timer_idx = TIMER_0;

    s_quadrature_y.encoder = UNI_MOUSE_QUADRATURE_ENCODER_V;
    s_quadrature_y.timer_idx = TIMER_1;

    // Create tasks
    xTaskCreatePinnedToCore(init_from_cpu1_task, "init_timers", TASK_TIMER_STACK_SIZE, NULL, TASK_TIMER_PRIO, NULL,
                            cpu_id);
}

void uni_mouse_quadrature_deinit() {
    // Stop the timers
    timer_deinit(TIMER_GROUP_0, s_quadrature_x.timer_idx);
    timer_deinit(TIMER_GROUP_0, s_quadrature_y.timer_idx);

    // Delete the tasks
    vTaskDelete(s_timer_x_task);
    vTaskDelete(s_timer_y_task);
    s_timer_x_task = NULL;
    s_timer_y_task = NULL;
}

void uni_mouse_quadrature_pause() {
    timer_pause(TIMER_GROUP_0, s_quadrature_x.timer_idx);
    timer_pause(TIMER_GROUP_0, s_quadrature_y.timer_idx);
}

void uni_mouse_quadrature_start() {
    timer_start(TIMER_GROUP_0, s_quadrature_x.timer_idx);
    timer_start(TIMER_GROUP_0, s_quadrature_y.timer_idx);
}

// Should be called everytime that mouse report is received.
void uni_mouse_quadrature_update(int port_idx, int32_t dx, int32_t dy) {
    // Looks overkill to pass the "idx" each time.
    // But adding another function just to say which port is active looks worse(?)
    g_port_idx = port_idx;
    process_update(&s_quadrature_x, dx);
    process_update(&s_quadrature_y, dy);
}