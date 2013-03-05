#include <stdio.h>
#include <string.h>

#include "transport.h"

static void output_callback(session_t *session) {
    common_output_callback(session, 0);
}

static void output_header(client_t *client) {
    char headStr[200] = "";
    strcat(headStr, "HTTP/1.1 200 OK\r\n");
    strcat(headStr, "Content-Type: text/plain; charset=UTF-8\r\n");
    strcat(headStr, "Connection: keep-alive\r\n");
    strcat(headStr, "\r\n");

    write_output(client, headStr, NULL);
}

static void output_whole(client_t *client, char *body_msg) {
    transport_info *trans_info = &client->trans_info;
    char *sessionid = trans_info->sessionid;
    
    char http_msg[strlen(body_msg) + 200];
    sprintf(http_msg, RESPONSE_PLAIN, (int)strlen(body_msg), body_msg);

    write_output(client, http_msg, on_close);

    end_connect(sessionid);
}

static void output_body(client_t *client, char *http_msg) {
    transport_info *trans_info = &client->trans_info;
    char *sessionid = trans_info->sessionid;

    write_output(client, http_msg, on_close);

    end_connect(sessionid);
}

transports_fn *init_xhr_polling_transport() {
    transports_fn *trans_fn = init_default_transport();
    strcpy(trans_fn->name, "xhr-polling");
    strcpy(trans_fn->heartbeat, "8::");

    return trans_fn;
}