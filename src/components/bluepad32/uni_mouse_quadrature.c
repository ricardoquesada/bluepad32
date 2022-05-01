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

// One 1us per tick
#define TIMER_DIVIDER (80)
#define TASK_TIMER_CPU (1)
#define TASK_TIMER_STACK_SIZE (1024)
#define TASK_TIMER_PRIO (5)

enum direction {
    DIRECTION_NEG,
    DIRECTION_NEUTRAL,
    DIRECTION_POS,
};

// A mouse has two encoders.
struct quadrature_state {
    int gpio_a;
    int gpio_b;
    // Current direction
    enum direction dir;
    // Current value
    int value;
    // Current quadrature phase
    int phase;

    // Which timer is being used: 0 or 1
    timer_idx_t timer_idx;
};

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

    if (q->dir == DIRECTION_NEG) {
        q->phase--;
        if (q->phase < 0)
            q->phase = 3;
    } else if (q->dir == DIRECTION_POS) {
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
            a = 0;
            b = 1;
            break;
        case 2:
            a = 1;
            b = 1;
            break;
        case 3:
            a = 1;
            b = 0;
            break;
        default:
            loge("%s: invalid phase value: %d", __func__, q->phase);
            a = b = 0;
            break;
    }
    gpio_set_level(q->gpio_a, a);
    gpio_set_level(q->gpio_b, b);
    logd("value: %d, quadrature phase: %d, a=%d, b=%d (%d,%d)\n", q->value, q->phase, a, b, q->gpio_a, q->gpio_b);
}

static void timer_x_task(void* arg) {
    logi("Task X started\n");
    while (true) {
        ulTaskNotifyTake(true, portMAX_DELAY);
        process_quadrature(&s_quadrature_x);
    }
}

static void timer_y_task(void* arg) {
    logi("Task Y started\n");
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

void uni_mouse_quadrature_init(int x1, int x2, int y1, int y2) {
    memset(&s_quadrature_x, 0, sizeof(s_quadrature_x));
    memset(&s_quadrature_y, 0, sizeof(s_quadrature_y));

    s_quadrature_x.gpio_a = x1;
    s_quadrature_x.gpio_b = x2;
    s_quadrature_x.timer_idx = TIMER_0;

    s_quadrature_y.gpio_a = y1;
    s_quadrature_y.gpio_b = y2;
    s_quadrature_y.timer_idx = TIMER_1;

    // Create tasks
    xTaskCreatePinnedToCore(timer_x_task, "timer_x", TASK_TIMER_STACK_SIZE, NULL, TASK_TIMER_PRIO, &s_timer_x_task,
                            TASK_TIMER_CPU);
    xTaskCreatePinnedToCore(timer_y_task, "timer_y", TASK_TIMER_STACK_SIZE, NULL, TASK_TIMER_PRIO, &s_timer_y_task,
                            TASK_TIMER_CPU);
    // Create timers
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_DOWN,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TIMER_AUTORELOAD_EN,
    };

    // X
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_0, s_quadrature_x.timer_idx, &config));
    timer_set_counter_value(TIMER_GROUP_0, s_quadrature_x.timer_idx, 50000 * 100);
    timer_isr_callback_add(TIMER_GROUP_0, s_quadrature_x.timer_idx, timer_x_handler, NULL, 0);
    timer_start(TIMER_GROUP_0, s_quadrature_x.timer_idx);

    // Y
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_0, s_quadrature_y.timer_idx, &config));
    timer_set_counter_value(TIMER_GROUP_0, s_quadrature_y.timer_idx, 50000 * 100);
    timer_isr_callback_add(TIMER_GROUP_0, s_quadrature_y.timer_idx, timer_y_handler, NULL, 0);
    timer_start(TIMER_GROUP_0, s_quadrature_y.timer_idx);
}

void uni_mouse_quadrature_stop() {
    // Stop the timers

    // Delete the tasks
    vTaskDelete(s_timer_x_task);
    vTaskDelete(s_timer_y_task);
    s_timer_x_task = NULL;
    s_timer_y_task = NULL;
}

static void process_update(struct quadrature_state* q, int8_t delta) {
    uint64_t units;
    if (delta != 0) {
        /* Don't update the phase, it should start from the previous phase */
        q->value = abs(delta);
        q->dir = (delta < 0) ? DIRECTION_NEG : DIRECTION_POS;

        // Max mouse delta value: 127. Which should be the one that triggers faster,
        // about about 65 us (microseconds).
        units = (127 * 65) / abs(delta);
        timer_set_counter_value(TIMER_GROUP_0, q->timer_idx, units);
    }
}

// Should be called everytime that mouse report is received.
void uni_mouse_quadrature_update(int8_t dx, int8_t dy) {
    process_update(&s_quadrature_x, dx);
    process_update(&s_quadrature_y, dy);
}