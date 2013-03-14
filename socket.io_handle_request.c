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

extern config *global_config;

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
        if (session->queue){
            g_queue_free(session->queue);
            session->queue = NULL;
        }
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
            /*printf("post url is %s\n", urlStr);*/
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
            fprintf(stderr, "the session's(id = %s) client had exist !\n", trans_info->sessionid);
            /*struct ev_loop *loop = ev_default_loop(0);*/
            /*client_t *old_client = session->client;*/
            /*fprintf(stderr, "now jump old_client's ev_read event\n");*/
            /*ev_io_stop(loop, &old_client->ev_read);*/
            /*fprintf(stderr, "now stop old_client'timer event\n");*/
            /*ev_timer *timer = &old_client->timeout;
            if (timer != NULL && (timer->data != NULL)) {
                ev_timer_stop(loop, timer);
            }*/
            /*fprintf(stderr, "now close the old_client's fd \n");*/
            /*close(old_client->fd);*/
            /*fprintf(stderr, "now free old_client\n");*/
            /*free(old_client);*/
            /*free_client(ev_default_loop(0), session->old_client);*/
            // is it right to free session->queue ?
            if (session->queue) {
                // TODO: bugs need to fix
                // in sometimes, g_queue_free may cause g_queue_pop_tail () invald
                /*g_queue_free(session->queue);*/
                session->queue = NULL;
            }
        }

        // bind the client with session
        session->client = client;

        return handle_transport(client, urlStr);
    } else {
        return handle_static(parser, urlStr);
    }
}