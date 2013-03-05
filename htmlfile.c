#include <stdio.h>
#include <string.h>

#include "transport.h"

#define HTMLFILE_RESPONSE_HEADER \
    "HTTP/1.1 200 OK\r\n" \
    "Connection: keep-alive\r\n" \
    "Content-Type: text/html; charset=utf-8\r\n" \
    "Transfer-Encoding: chunked\r\n" \
    "\r\n"

#define HTMLFILE_RESPONSE_FIRST \
    "132\r\n<html><head></head><body><script>var _ = function (msg) { parent.s._(msg, document); };</script>                                                                                                                                                                                                                  \r\n"

extern config *global_config;

static int format_message(const char *ori_message, char *target_message) {
    sprintf(target_message, "%X\r\n<script>_('%s');</script>\r\n", ((int)strlen(ori_message) + 23), ori_message);
}

static void output_header(client_t *client) {
    write_output(client, HTMLFILE_RESPONSE_HEADER, NULL);
}

static void output_body(client_t *client, char *http_msg) {
    char body_msg[strlen(http_msg) + 50];
    format_message(http_msg, body_msg);

    write_output(client, body_msg, NULL);
}

static void output_callback(session_t *session) {
    common_output_callback(session, 1);
}

static void output_whole(client_t *client, char *output_msg) {
    write_output(client, HTMLFILE_RESPONSE_HEADER, NULL);
    write_output(client, HTMLFILE_RESPONSE_FIRST, NULL);
    output_body(client, "1::");

    client->timeout.data = client;
    ev_timer_init(&client->timeout, timeout_cb, global_config->heartbeat_interval, 0);
    ev_timer_start(ev_default_loop(0), &client->timeout);
}

/*static void htmlfile_polling_output_finish(client_t *client, char *http_msg) {
    char body_msg[] = "0\r\n\r\n";

    write_output(client, body_msg, on_close);
}*/

static void _timeout_callback(struct ev_timer *time_handle) {
    if (time_handle) {
        client_t *client = time_handle->data;
        ev_timer_set(&client->timeout, global_config->heartbeat_interval, 0);
        ev_timer_start(ev_default_loop(0), &client->timeout);
    }
}

transports_fn *init_htmlfile_polling_transport() {
    transports_fn *trans_fn = init_default_transport();
    strcpy(trans_fn->name, "htmlfile");
    strcpy(trans_fn->heartbeat, "8::");
    trans_fn->timeout_callback = _timeout_callback;

    return trans_fn;
}