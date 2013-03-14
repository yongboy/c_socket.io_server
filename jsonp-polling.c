#include <stdio.h>
#include <string.h>

#include "transport.h"

#define JSONP_RESPONSE_PLAIN \
    "HTTP/1.1 200 OK\r\n" \
    "Connection: keep-alive\r\n" \
    "Content-Type: application/x-javascript; charset=utf-8\r\n" \
    "X-XSS-Protection: 0\r\n" \
    "Content-Length: %d\r\n" \
    "\r\n" \
    "%s\n"

static void output_callback(session_t *session) {
    common_output_callback(session, 0);
}

static void output_header(client_t *client) {
    char headStr[250] = "";
    strcat(headStr, "HTTP/1.1 200 OK\r\n");
    strcat(headStr, "Content-Type: application/x-javascript; charset=utf-8\r\n");
    strcat(headStr, "Connection: keep-alive\r\n");
    strcat(headStr, "X-XSS-Protection: 0\r\n");
    strcat(headStr, "\r\n");

    write_output(client, headStr, NULL);
}

static void output_whole(client_t *client, char *output_msg) {
    transport_info *trans_info = &client->trans_info;
    char *sessionid = trans_info->sessionid;

    char body_msg[strlen(output_msg) + 50];
    sprintf(body_msg, "io.j[%s]('%s');", trans_info->i, output_msg);

    char http_msg[strlen(body_msg) + 200];
    sprintf(http_msg, JSONP_RESPONSE_PLAIN, (int)strlen(body_msg), body_msg);

    write_output(client, http_msg, on_close);
    end_connect(sessionid);
}

static void output_body(client_t *client, char *http_msg) {
    transport_info *trans_info = &client->trans_info;
    char *sessionid = trans_info->sessionid;
    int body_len = (int)strlen(http_msg) + (int)atol(trans_info->i) + 13;
    char body_msg[body_len];
    sprintf(body_msg, "io.j[%s]('%s');", trans_info->i, http_msg);

    write_output(client, body_msg, on_close);
    end_connect(sessionid);
}

transports_fn *init_jsonp_polling_transport() {
    transports_fn *trans_fn = init_default_transport();
    strcpy(trans_fn->name, "jsonp-polling");
    strcpy(trans_fn->heartbeat, "8::");

    return trans_fn;
}