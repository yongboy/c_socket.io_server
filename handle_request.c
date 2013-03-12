#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>

#include <uuid/uuid.h>
#include "socket_io.h"
#include "endpoint.h"
#include "transports.h"
#include "safe_mem.h"
#include "memwatch/memwatch.h"

#define POLLING_FRAMEING_DELIM "\ufffd"

config *global_config;
void handle_disconnected(client_t *client);
int handle_body_cb_one(client_t *client, char *post_msg, void (*close_fn)(client_t *client), bool need_close_fn);

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
    global_config->heartbeat_timeout = g_key_file_get_integer(keyfile, "global", "heartbeat_timeout", 60);
    global_config->close_timeout = g_key_file_get_integer(keyfile, "global", "close_timeout", 60);
    global_config->server_close_timeout = g_key_file_get_integer(keyfile, "global", "server_close_timeout", 5);
    global_config->heartbeat_interval = g_key_file_get_integer(keyfile, "global", "heartbeat_interval", NULL);
    global_config->static_path = g_key_file_get_string(keyfile, "global", "static_path", NULL);
}

char *gen_uuid(char *uuidBuff) {
    uuid_t uuid;
    int result = uuid_generate_time_safe(uuid);
    if (result) {
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
    if (sessionid == NULL) {
        return;
    }

    session_t *session = store_lookup(sessionid);
    if (session) {
        session->state = DISCONNECTING_STATE;
        // something need to handle on_disconnected event ...
        if (session->endpoint) {
            /*fprintf(stderr, "session's endpoint is %s and sessionid is %s\n", session->endpoint, session->sessionid);*/
            endpoint_implement *endpoint_impl = endpoints_get(session->endpoint);
            if (endpoint_impl) {
                /*printf("call endpoint_impl->on_disconnect here ...\n");*/
                endpoint_impl->on_disconnect(sessionid, NULL);
            } else {
                fprintf(stderr, "the endpoint_impl is null !\n");
            }
        } else {
            fprintf(stderr, "session's (null)endpoint is %s and sessionid is %s\n", session->endpoint, session->sessionid);
        }
    }

    if (timer) {
        free(timer->data);
        ev_timer_stop(ev_default_loop(0), timer);

        store_remove(sessionid);

        free(session->sessionid);
        free(session->client);
        if(session->queue)
            g_queue_free(session->queue);
        free(session->endpoint);
        free(session);
    } else {
        printf("time is NULL !\n");
    }
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
    ev_timer_init(timeout, clear_handshake_cb, global_config->server_close_timeout, 0);
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
        write_output(client, RESPONSE_400, on_close);

        return 0;
    }

    transports_fn *trans_fn = get_transport_fn(client);
    if (!trans_fn) {
        fprintf(stderr, "Got no transport struct !\n");
        write_output(client, RESPONSE_400, on_close);

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
        trans_fn->output_header(client);

        client->timeout.data = client;
        ev_timer_init(&client->timeout, timeout_cb, global_config->heartbeat_interval, 0);
        ev_timer_start(ev_default_loop(0), &client->timeout);
        return 0;
    }

    trans_fn->output_whole(client, body_msg);

    return 0;
}

int on_url_cb(http_parser *parser, const char *at, size_t length) {
    char urlStr[200];
    sprintf(urlStr, "%.*s", (int)length, at);
    if (!strcmp(urlStr, "/") || !strcmp(urlStr, "")) {
        strcpy(urlStr, "/index.html");
    }

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
            // output ? '7::' [endpoint] ':' [reason] '+' [advice]
            write_output(client, RESPONSE_400, on_close);
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

    if (strchr(post_msg, 'd') == post_msg) {
        char *unescape_string = g_uri_unescape_string(post_msg, NULL);
        gchar *result = g_strcompress(unescape_string);
        char target[strlen(result) - 4];
        strncpy(target, result + 3, strlen(result));
        target[strlen(target) - 1] = '\0';

        strcpy(post_msg, target);
        free(result);
    }
    
    //check multiple messages framing, eg: `\ufffd` [message lenth] `\ufffd`
    if (strstr(post_msg, POLLING_FRAMEING_DELIM) == post_msg) {
        char *str = strtok(post_msg, POLLING_FRAMEING_DELIM);
        bool is_str = false;
        while (str != NULL) {
            if (is_str) {
                handle_body_cb_one(client, str, NULL, false);
            }
            is_str = !is_str;
            str = strtok(NULL, POLLING_FRAMEING_DELIM);
        }

        char http_msg[200];
        sprintf(http_msg, RESPONSE_PLAIN, 1, "1");
        write_output(client, http_msg, on_close);
    } else {
        handle_body_cb(client, post_msg, on_close);
    }

    return 0;
}

int handle_body_cb_one(client_t *client, char *post_msg, void (*close_fn)(client_t *client), bool need_close_fn) {
    transport_info *trans_info = &client->trans_info;
    if (trans_info == NULL) {
        fprintf(stderr, "handle_body_cb_one's trans_info is NULL!\n");
        write_output(client, RESPONSE_400, on_close);

        return 0;
    }

    message_fields msg_fields;
    body_2_struct(post_msg, &msg_fields);
    if (msg_fields.endpoint == NULL) {
        fprintf(stderr, "msg_fields.endpoint is NULL and the post_msg is %s\n", post_msg);
        /*return 0;*/
    }

    transports_fn *trans_fn = get_transport_fn(client);
    char *sessionid = trans_info->sessionid;
    session_t *session = store_lookup(sessionid);
    endpoint_implement *endpoint_impl = endpoints_get(msg_fields.endpoint);
    if (endpoint_impl) {
        int num = atoi(msg_fields.message_type);
        switch (num) {
        case 0:
            endpoint_impl->on_disconnect(trans_info->sessionid, &msg_fields);
            notice_disconnect(&msg_fields, trans_info->sessionid);
            break;
        case 1:
            if (session->state != CONNECTED_STATE) {
                /*printf("endpoint is %s, sessionid is %s, post_msg is %s\n", msg_fields.endpoint, trans_info->sessionid, post_msg);*/
                notice_connect(&msg_fields, trans_info->sessionid, post_msg);
                endpoint_impl->on_connect(trans_info->sessionid);
            } else {
                fprintf(stderr, "invalid state is %d endpoint is %s, sessionid is %s, post_msg is %s\n", session->state, msg_fields.endpoint, trans_info->sessionid, post_msg);
            }
            break;
        case 2:
            trans_fn->heartbeat_callback(client, trans_info->sessionid);
            break;
        case 3:
            endpoint_impl->on_message(trans_info->sessionid, &msg_fields);
            break;
        case 4:
            endpoint_impl->on_json_message(trans_info->sessionid, &msg_fields);
            break;
        case 5:
            endpoint_impl->on_event(trans_info->sessionid, &msg_fields);
            break;
        default:
            endpoint_impl->on_other(trans_info->sessionid, &msg_fields);
            break;
        }
    } else {
        fprintf(stderr, "handle_body_cb_one's endpoint_impl is NULL and the post_msg is %s !\n", post_msg);
        write_output(client, RESPONSE_400, on_close);

        return 0;
    }

    if (need_close_fn) {
        char http_msg[200];
        if (close_fn) {
            sprintf(http_msg, RESPONSE_PLAIN, 1, "1");
            write_output(client, http_msg, close_fn);
        } else { // just for websocket, output message eg: "5::/chat:1";
            if (strlen(msg_fields.endpoint) > 0) {
                sprintf(http_msg, "5::%s:1", msg_fields.endpoint);
            } else {
                sprintf(http_msg, "5::%s:1", session->endpoint);
            }

            trans_fn->output_body(client, http_msg);
        }
    }

    return 0;
}

int handle_body_cb(client_t *client, char *post_msg, void (*close_fn)(client_t *client)) {
    return handle_body_cb_one(client, post_msg, close_fn, true);
}

void handle_disconnected(client_t *client) {
    printf("handle_disconnected here ...\n");
    if (client == NULL) {
        fprintf(stderr, "the client is NULL !\n");
        return;
    }

    http_parser *parser = &client->parser;
    if (parser == NULL) {
        return;
    }
    if (parser && parser->method == HTTP_POST) {
        printf("parser && parser->method == HTTP_POST is true!\n");
        return;
    }

    transport_info *trans_info = &client->trans_info;
    if (trans_info == NULL) {
        fprintf(stderr, "the trans_info is NULL !\n");
        return;
    }

    char *sessionid = trans_info->sessionid;
    if (sessionid == NULL) {
        fprintf(stderr, "handle_disconnected's the sessionid is NULL\n");
        return;
    }

    session_t *session = store_lookup(sessionid);
    if (session == NULL) {
        fprintf(stderr, "handle_disconnected's the session is NULL\n");
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