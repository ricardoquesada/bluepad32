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

void uni_bt_conn_get_address(uni_bt_conn_t* conn, bd_addr_t out_addr) {
    memcpy(out_addr, conn->remote_addr, 6);
}

bool uni_bt_conn_is_incoming(uni_bt_conn_t* conn) {
    return conn->incoming;
}

bool uni_bt_conn_is_connected(uni_bt_conn_t* conn) {
    return conn->connected;
}