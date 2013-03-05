#include <stdio.h>
#include <string.h>

#include "include/c-websocket/cWebSockets.h"

#include "transport.h"

extern config *global_config;

static void write_ws_msg(client_t *client, const char *ori_msg) {
    int ori_len = (int)strlen(ori_msg);
    int target_len = (int)ori_len + 9;
    unsigned char target_msg[target_len];
    memset(target_msg, '\0', target_len);

    int frame_len = WEBSOCKET_set_content(ori_msg, ori_len, target_msg, target_len);

    write(client->fd, target_msg, frame_len);
}

static void output_header(client_t *client) {
    client->trans_version = TRANSPORT_WEBSOCKET_VERSION;

    char dst[REQUEST_BUFFER_SIZE];
    memset(dst, '\0', REQUEST_BUFFER_SIZE);
    WEBSOCKET_generate_handshake(client->data, dst, REQUEST_BUFFER_SIZE);

    write_output(client, dst, NULL);

    client->data = NULL;
}

static void output_body(client_t *client, char *http_msg) {
    write_ws_msg(client, http_msg);
}

static void output_callback(session_t *session) {
    common_output_callback(session, 1);
}

static void _heartbeat_callback(client_t *client, char *sessionid) {
    ev_timer_stop(ev_default_loop(0), &client->timeout);

    ev_timer_set(&client->timeout, global_config->heartbeat_interval, 0);
    ev_timer_start(ev_default_loop(0), &client->timeout);
}

static void output_whole(client_t *client, char *output_msg) {
    output_header(client);
    output_body(client, output_msg);

    client->timeout.data = client;
    ev_timer_init(&client->timeout, timeout_cb, global_config->heartbeat_interval, 0);
    _heartbeat_callback(client, NULL);
}

transports_fn *init_websocket_transport() {
    transports_fn *trans_fn = init_default_transport();
    strcpy(trans_fn->name, "websocket");
    strcpy(trans_fn->heartbeat, "2::");

    trans_fn->heartbeat_callback = _heartbeat_callback;

    return trans_fn;
}