// SPDX-License-Identifier: Apache-2.0
// Copyright 2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#include "bt/uni_bt_conn.h"

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
    uni_bt_conn_set_connected(conn, false);
}
