/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2021 Ricardo Quesada

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

#include "uni_bt_conn.h"

#include <string.h>

void uni_bt_conn_init(uni_bt_conn_t* conn) {
    memset(conn, 0, sizeof(*conn));
}

void uni_bt_conn_set_state(uni_bt_conn_t* conn, uni_bt_conn_state_t state) {
    conn->state = state;
}

uni_bt_conn_state_t uni_bt_conn_get_state(uni_bt_conn_t* conn) {
    return conn->state;
}