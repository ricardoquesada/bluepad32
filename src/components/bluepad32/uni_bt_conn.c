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

#include "uni_log.h"

void uni_bt_conn_init(uni_bt_conn_t* conn) {
    memset(conn, 0, sizeof(*conn));
    conn->handle = UNI_BT_CONN_HANDLE_INVALID;
}

void uni_bt_conn_set_state(uni_bt_conn_t* conn, uni_bt_conn_state_t state) {
    conn->state = state;
}

void uni_bt_conn_set_protocol(uni_bt_conn_t* conn, uni_bt_conn_protocol_t protocol) {
    conn->protocol = protocol;
}

uni_bt_conn_state_t uni_bt_conn_get_state(uni_bt_conn_t* conn) {
    return conn->state;
}

void uni_bt_conn_get_address(uni_bt_conn_t* conn, bd_addr_t out_addr) {
    memcpy(out_addr, conn->btaddr, 6);
}

bool uni_bt_conn_is_incoming(uni_bt_conn_t* conn) {
    return conn->incoming;
}

void uni_bt_conn_set_connected(uni_bt_conn_t* conn, bool connected) {
    if (conn->connected == connected) {
        logi("connection %s already in state %d, ignoring\n", bd_addr_to_str(conn->btaddr), connected);
        return;
    }
    conn->connected = connected;
}

bool uni_bt_conn_is_connected(uni_bt_conn_t* conn) {
    return conn->connected;
}

void uni_bt_conn_disconnect(uni_bt_conn_t* conn) {
    if (gap_get_connection_type(conn->handle) != GAP_CONNECTION_INVALID) {
        gap_disconnect(conn->handle);
        conn->handle = UNI_BT_CONN_HANDLE_INVALID;
    } else {
        // After calling gap_disconnect() we should not call l2cap_disonnect(),
        // since gap_disconnect() will take care of it.
        // But if the handle is not present, then call it manually.
        if (conn->control_cid) {
            l2cap_disconnect(conn->control_cid);
            conn->control_cid = 0;
        }

        if (conn->interrupt_cid) {
            l2cap_disconnect(conn->interrupt_cid);
            conn->interrupt_cid = 0;
        }
    }

    uni_bt_conn_set_connected(conn, false);
}
