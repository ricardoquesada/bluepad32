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
 * Code based on SmallyMouse2 by Simon Inns
 * https://github.com/simoninns/SmallyMouse2
 */
#include "uni_mouse_quadrature.h"

#include <stdbool.h>
#include <stdint.h>

#include <driver/gpio.h>
#include <driver/timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "uni_debug.h"

#define TASK_TIMER_CPU (1)
#define TASK_TIMER_STACK_SIZE (2048)
#define TASK_TIMER_PRIO (1)

enum direction {
    DIRECTION_NEG,
    DIRECTION_NEUTRAL,
    DIRECTION_POS,
};

// Storing in its own data structure in case in the future we want to support more than one mouse
struct output_pins {
    int x1;
    int x2;
    int y1;
    int y2;
};
struct quadrature_state {
    // Current direction
    enum direction dir_x;
    enum direction dir_y;
    // Current value
    int x;
    int y;
    // Current quadrature phase
    int phase_x;
    int phase_y;
};

static struct output_pins s_out_pins;
static struct quadrature_state s_state;
static TaskHandle_t s_timer_x_task;
static TaskHandle_t s_timer_y_task;

static void process_timer_x() {
    static int toggle = 0;
    static int counter = 0;
    static int value = 0;

    if (s_state.dir_x == DIRECTION_NEG) {
        s_state.phase_x--;
        if (s_state.phase_x < 0)
            s_state.phase_x = 3;
    } else if (s_state.dir_x == DIRECTION_POS) {
        s_state.phase_x++;
        if (s_state.phase_x > 3)
            s_state.phase_x = 0;
    }

    gpio_set_level(5, toggle);
    toggle = !toggle;

    if (counter++ > 25) {
        if (value == 0) {
            timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 500000);
            value = 1;
        } else {
            timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 50000);
            value = 0;
        }
        counter = 0;
    }
}

static void process_timer_y() {
    if (s_state.dir_y == DIRECTION_NEG) {
        s_state.phase_y--;
        if (s_state.phase_y < 0)
            s_state.phase_y = 3;
    } else if (s_state.dir_y == DIRECTION_POS) {
        s_state.phase_y++;
        if (s_state.phase_y > 3)
            s_state.phase_y = 0;
    }
}

static void timer_x_task(void* arg) {
    logi("Task X started\n");
    while (true) {
        ulTaskNotifyTake(true, portMAX_DELAY);
        process_timer_x();
    }
}

static void timer_y_task(void* arg) {
    logi("Task Y started\n");
    while (true) {
        ulTaskNotifyTake(true, portMAX_DELAY);
        process_timer_y();
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
    s_out_pins.x1 = x1;
    s_out_pins.x2 = x2;
    s_out_pins.y1 = y1;
    s_out_pins.y2 = y2;

    // Create tasks
    xTaskCreatePinnedToCore(timer_x_task, "timer_x", TASK_TIMER_STACK_SIZE, NULL, TASK_TIMER_PRIO, &s_timer_x_task,
                            TASK_TIMER_CPU);
    xTaskCreatePinnedToCore(timer_y_task, "timer_y", TASK_TIMER_STACK_SIZE, NULL, TASK_TIMER_PRIO, &s_timer_y_task,
                            TASK_TIMER_CPU);
    // Create timers
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = 160,
        .counter_dir = TIMER_COUNT_DOWN,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TIMER_AUTORELOAD_EN,
    };
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_0, TIMER_0, &config));
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_0, TIMER_1, &config));

    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 50000);
    timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, timer_x_handler, NULL, 0);

    timer_set_counter_value(TIMER_GROUP_0, TIMER_1, 50000);
    timer_isr_callback_add(TIMER_GROUP_0, TIMER_1, timer_y_handler, NULL, 0);

    timer_start(TIMER_GROUP_0, TIMER_0);
    timer_start(TIMER_GROUP_0, TIMER_1);
}

void uni_mouse_quadrature_stop() {
    // Stop the timers

    // Delete the tasks
    vTaskDelete(s_timer_x_task);
    vTaskDelete(s_timer_y_task);
    s_timer_x_task = NULL;
    s_timer_y_task = NULL;
}

// Should be called everytime that mouse report is received.
void uni_mouse_quadrature_update(int8_t dx, int8_t dy) {
    s_state.x = dx;
    s_state.y = dy;
}