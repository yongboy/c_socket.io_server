#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/c-websocket/cWebSockets.h"

#include "transport.h"

extern config *global_config;

static void write_ws_msg(client_t *client, const char *ori_msg) {
    int ori_len = (int)strlen(ori_msg);
    int target_len = ori_len + 3;
    char target_msg[target_len];
    memset(target_msg, '\0', target_len);
    target_msg[0] = '\x00';
    memcpy(target_msg + 1 , ori_msg, ori_len);
    target_msg[ori_len + 1] = '\xFF';
    write(client->fd, target_msg, target_len - 1);
}

static void output_header(client_t *client) {
    client->trans_version = TRANSPORT_WEBSOCKET_VERSION;

    transport_info *trans_info = &client->trans_info;
    char *url = trans_info->oriurl;

    char dst[REQUEST_BUFFER_SIZE];
    memset(dst, '\0', REQUEST_BUFFER_SIZE);
    int output_len = WEBSOCKET_generate_handshake_76(url, client->data, dst, REQUEST_BUFFER_SIZE);
    write(client->fd, dst, output_len);

    client->data = NULL;
}

static void output_body(client_t *client, char *http_msg) {
    write_ws_msg(client, http_msg);
}

static void output_callback(session_t *session) {
    common_output_callback(session, 1);
}

static void _heartbeat_callback(client_t *client, char *sessionid) {
    /*ev_timer_stop(ev_default_loop(0), &client->timeout);*/

    ev_timer_set(&client->timeout, global_config->heartbeat_interval, 0);
    ev_timer_start(ev_default_loop(0), &client->timeout);
}

static void output_whole(client_t *client, char *output_msg) {
    output_header(client);
    output_body(client, output_msg);

    // the server must have to send heartbeat
    client->timeout.data = client;
    ev_timer_init(&client->timeout, timeout_cb, global_config->heartbeat_interval, 0);
    _heartbeat_callback(client, NULL);
}

transports_fn *init_flashsocket_transport() {
    transports_fn *trans_fn = init_default_transport();
    strcpy(trans_fn->name, "flashsocket");
    strcpy(trans_fn->heartbeat, "2::");

    trans_fn->heartbeat_callback = _heartbeat_callback;

    return trans_fn;
}