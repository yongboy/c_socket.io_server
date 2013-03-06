#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#include <uuid/uuid.h>
#include "store.h"
#include "socket_io.h"
#include "endpoint.h"
#include "transports.h"

config *global_config;
void handle_disconnected(client_t *client);

void init_config() {
    GKeyFile *keyfile;
    GKeyFileFlags flags;
    GError *error = NULL;

    keyfile = g_key_file_new();
    flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

    if (!g_key_file_load_from_file (keyfile, "server.ini", flags, &error)) {
        fprintf(stderr, "%s\n", "the server.ini not found !");
        return;
    }

    global_config = malloc(sizeof(config));
    global_config->transports = g_key_file_get_string(keyfile, "global", "transports", NULL);
    global_config->heartbeat_timeout = g_key_file_get_integer(keyfile, "global", "heartbeat_timeout", NULL);
    global_config->close_timeout = g_key_file_get_integer(keyfile, "global", "close_timeout", NULL);
    global_config->heartbeat_interval = g_key_file_get_integer(keyfile, "global", "heartbeat_interval", NULL);
    global_config->enable_flash_policy = g_key_file_get_integer(keyfile, "global", "enable_flash_policy", NULL);
    global_config->flash_policy_port = g_key_file_get_integer(keyfile, "global", "flash_policy_port", NULL);
    global_config->static_path = g_key_file_get_string(keyfile, "global", "static_path", NULL);
}

char *gen_uuid(char *uuidBuff) {
    uuid_t uuid;
    int result = uuid_generate_time_safe(uuid);
    if (result == 0) {
        printf("uuid generated safely\n");
    } else {
        printf("uuid not generated safely\n");
    }
    uuid_unparse(uuid, uuidBuff);

    return uuidBuff;
}

void clear_handshake_cb(EV_P_ struct ev_timer *timer, int revents) {
    if (EV_ERROR & revents) {
        printf("error event in timer_beat\n");
        return ;
    }

    if (timer == NULL) {
        fprintf(stderr, "the timer is NULL now !\n");
        return;
    }

    char *sessionid = timer->data;

    session_t *session = store_lookup(sessionid);
    if (session) {
        if (session->state == CONNECTING_STATE) {
            store_remove(sessionid);
            /*g_free(session);*/
        }
    }

    g_free(timer->data);
    ev_timer_stop(ev_default_loop(0), timer);
}

int handle_handshake(http_parser *parser) {
    char uuidBuff[36];
    gen_uuid(uuidBuff);

    session_t *session = malloc(sizeof(session_t));
    session->sessionid = g_strdup(uuidBuff);
    session->queue = NULL;
    session->client = NULL;
    session->endpoint = NULL;
    session->state = CONNECTING_STATE;

    ev_timer *timeout = &session->close_timeout;
    timeout->data = g_strdup(uuidBuff);
    ev_timer_init(timeout, clear_handshake_cb, global_config->close_timeout, 0);
    ev_timer_start(ev_default_loop(0), timeout);

    store_add(uuidBuff, session);

    char body_msg[120];
    sprintf(body_msg, "%s:%d:%d:%s", uuidBuff, global_config->heartbeat_timeout, global_config->close_timeout, global_config->transports);
    char http_msg[strlen(body_msg) + 200];
    sprintf(http_msg, RESPONSE_PLAIN, (int)strlen(body_msg), body_msg);

    client_t *client = parser->data;
    write_output(client, http_msg, on_close);

    return 0;
}

void timeout_cb(EV_P_ struct ev_timer *timer, int revents) {
    if (EV_ERROR & revents) {
        printf("error event in timer_beat\n");
        return ;
    }

    if (timer == NULL) {
        printf("the timer is NULL now !\n");
    }

    client_t *client = timer->data;
    if (client == NULL) {
        printf("Timeout the client is NULL !\n");
        return;
    }

    transports_fn *trans_fn = get_transport_fn(client);
    if (trans_fn) {
        trans_fn->output_body(client, trans_fn->heartbeat);
        trans_fn->timeout_callback(timer);
    } else {
        fprintf(stderr, "Got NO transport struct !\n");
    }
}

int handle_transport(client_t *client, const char *urlStr) {
    char body_msg[1024] = "";
    url_2_struct((gchar *)urlStr, &client->trans_info);

    transport_info *trans_info = &client->trans_info;
    if (trans_info == NULL) {
        fprintf(stderr, "GOT NULLL MATCH!");
        return 0;
    }

    transports_fn *trans_fn = get_transport_fn(client);
    if (!trans_fn) {
        fprintf(stderr, "Got no transport struct !\n");
        return 0;
    }

    session_t *session = store_lookup(trans_info->sessionid);
    GQueue *queue = session->queue;
    if (queue == NULL) {
        session->queue = g_queue_new();
        strcpy(body_msg, "1::");
    } else {
        char *str = g_queue_pop_tail(queue);
        if (str != NULL) {
            strcpy(body_msg, str);
        }
    }

    trans_fn->init_connect(client, trans_info->sessionid);

    if (strlen(body_msg) <= 0) {
        ev_io_stop(ev_default_loop(0), &client->ev_read);
        fprintf(stdout, "now just only output header now (global_config->heartbeat_interval = %d) ..\n", global_config->heartbeat_interval);
        trans_fn->output_header(client);

        client->timeout.data = client;
        ev_timer_init(&client->timeout, timeout_cb, global_config->heartbeat_interval, 0);
        ev_timer_start(ev_default_loop(0), &client->timeout);
        printf("had set timeout end ...\n");
        return 0;
    }

    printf("now output whole content(%s)\n", body_msg);
    trans_fn->output_whole(client, body_msg);

    return 0;
}

int on_url_cb(http_parser *parser, const char *at, size_t length) {
    char urlStr[200];
    sprintf(urlStr, "%.*s", (int)length, at);
    if (!strcmp(urlStr, "/") || !strcmp(urlStr, "")) {
        strcpy(urlStr, "/index.html");
    }

    fprintf(stdout, "request url is %s\n", urlStr);

    gchar *pattern_string = "^/([^/])*/\\d{1}/\\?t=\\d+.*?";
    if (check_match((gchar *)urlStr, pattern_string)) {
        return handle_handshake(parser);
    } else if (check_match((gchar *)urlStr, "^/[^/]*/\\d{1}/[^/]*/([^/]*)")) {
        client_t *client = parser->data;
        url_2_struct((gchar *)urlStr, &client->trans_info);

        if (parser->method != HTTP_GET) {
            return 0;
        }

        transport_info *trans_info = &client->trans_info;
        session_t *session = store_lookup(trans_info->sessionid);
        // need to handle the session is NULL
        if (session == NULL) {
            fprintf(stderr, "The session is NULL, now output 500 !\n");
            // '7::' [endpoint] ':' [reason] '+' [advice]
            // 401 Unauthorized
            char headStr[200] = "";
            strcat(headStr, "HTTP/1.1 401 Unauthorized\r\n");
            strcat(headStr, "\r\n");

            write_output(client, headStr, on_close);

            return 0;
        }

        // check if exists hang-up connections
        if (session->client) {
            fprintf(stderr, "the session's client had exist !\n");
            free_client(ev_default_loop(0), session->client);
            // is it right to free session->queue ?
            g_queue_free(session->queue);
            session->queue = NULL;
        }

        // bind the client with session
        session->client = client;

        return handle_transport(client, urlStr);
    } else {
        return handle_static(parser, urlStr);
    }
}

int on_body_cb(http_parser *parser, const char *at, size_t length) {
    client_t *client = parser->data;

    char post_msg[(int)length + 1];
    sprintf(post_msg, "%.*s", (int)length, at);

    return handle_body_cb(client, post_msg, on_close);
}

int handle_body_cb(client_t *client, char *post_msg, void (*close_fn)(client_t *client)) {
    printf("post_msg is %s\n", post_msg);
    transport_info *trans_info = &client->trans_info;

    if (strchr(post_msg, 'd') == post_msg) {
        char *unescape_string = g_uri_unescape_string(post_msg, NULL);
        gchar *result = g_strcompress(unescape_string);
        char target[strlen(result) - 4];
        strncpy(target, result + 3, strlen(result));
        target[strlen(target) - 1] = '\0';

        strcpy(post_msg, target);
        g_free(result);
    }

    message_fields msg_fields;
    body_2_struct(post_msg, &msg_fields);

    endpoint_implement *endpoint_impl = endpoints_get(msg_fields.endpoint);
    transports_fn *trans_fn = get_transport_fn(client);

    int num = atoi(msg_fields.message_type);
    switch (num) {
    case 0:
        printf("case 0 ......................\n");
        endpoint_impl->on_disconnect(trans_info->sessionid, &msg_fields);
        notice_disconnect(&msg_fields, trans_info->sessionid);
        break;
    case 1:
        notice_connect(&msg_fields, trans_info->sessionid, post_msg);
        endpoint_impl->on_connect(trans_info->sessionid, &msg_fields);
        break;
    case 2:
        trans_fn->heartbeat_callback(client, trans_info->sessionid);
        break;
    case 5:
        endpoint_impl->on_message(trans_info->sessionid, &msg_fields);
        break;
    }

    char http_msg[200];
    if (close_fn) {
        sprintf(http_msg, RESPONSE_PLAIN, 1, "1");
        write_output(client, http_msg, close_fn);
    } else { // just for websocket
        // output message eg: "5::/chat:1";
        if (strlen(msg_fields.endpoint) > 0) {
            sprintf(http_msg, "5::%s:1", msg_fields.endpoint);
        } else {
            char *sessionid = trans_info->sessionid;
            session_t *session = store_lookup(sessionid);
            sprintf(http_msg, "5::%s:1", session->endpoint);
        }

        trans_fn->output_body(client, http_msg);
    }

    return 0;
}

void handle_disconnected(client_t *client) {
    if (client == NULL) {
        fprintf(stderr, "the client is NULL !\n");
        return;
    }

    http_parser *parser = &client->parser;
    if (parser == NULL) {
        return;
    }
    if (parser && parser->method == HTTP_POST) {
        return;
    }

    transport_info *trans_info = &client->trans_info;
    if (trans_info == NULL) {
        fprintf(stderr, "the trans_info is NULL !\n");
        return;
    }

    char *sessionid = trans_info->sessionid;
    if (sessionid == NULL) {
        fprintf(stderr, "the sessionid is NULL !\n");
        return;
    }

    session_t *session = store_lookup(sessionid);
    if (session == NULL) {
        return;
    }

    session->state = DISCONNECTED_STATE;

    // something need to handle on_disconnected event ...
    if (session->endpoint) {
        endpoint_implement *endpoint_impl = endpoints_get(session->endpoint);
        if (endpoint_impl) {
            endpoint_impl->on_disconnect(sessionid, NULL);
        } else {
            fprintf(stderr, "the endpoint_impl is null !\n");
        }
    }

    // set timeout for delete the session
    transports_fn *trans_fn = get_transport_fn(client);
    if (trans_fn) {
        trans_fn->end_connect(sessionid);
    } else {
        fprintf(stderr, "Got NO transport struct !\n");
    }
}