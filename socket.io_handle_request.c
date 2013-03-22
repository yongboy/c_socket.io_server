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
        log_warn("error event in timer_beat");
        return ;
    }

    if (timer == NULL) {
        log_warn("the timer is NULL now !");
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
            /*log_warn("session's endpoint is %s and sessionid is %s\n", session->endpoint, session->sessionid);*/
            endpoint_implement *endpoint_impl = endpoints_get(session->endpoint);
            if (endpoint_impl) {
                endpoint_impl->on_disconnect(sessionid, NULL);
            } else {
                log_warn("the endpoint_impl is null !");
            }

            if (session->endpoint) {
                free(session->endpoint);
                session->endpoint = NULL;
            }
        } else {
            log_warn("session's (null)endpoint is %s and sessionid is %s", session->endpoint, session->sessionid);
        }
    }

    free(timer->data);
    ev_timer_stop(ev_default_loop(0), timer);

    store_remove(sessionid);

    free(session->sessionid);
    free(session->client);
    if (session->queue) {
        g_queue_free(session->queue);
        session->queue = NULL;
    }
    free(session);
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
        log_warn("error event in timer_beat");
        return ;
    }

    if (timer == NULL) {
        log_warn("the timer is NULL now !");
    }

    client_t *client = timer->data;
    if (client == NULL) {
        log_warn("Timeout the client is NULL !");
        return;
    }

    transports_fn *trans_fn = get_transport_fn(client);
    if (trans_fn) {
        trans_fn->output_body(client, trans_fn->heartbeat);
        trans_fn->timeout_callback(timer);
    } else {
        log_warn("Got NO transport struct !\n");
    }
}

int handle_transport(client_t *client, const char *urlStr) {
    char body_msg[1024] = "";
    url_2_struct((gchar *)urlStr, &client->trans_info);

    transport_info *trans_info = &client->trans_info;
    if (trans_info == NULL) {
        log_warn("GOT NULLL MATCH!");
        write_output(client, RESPONSE_400, on_close);

        return 0;
    }

    transports_fn *trans_fn = get_transport_fn(client);
    if (!trans_fn) {
        log_warn("Got no transport struct !");
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
            free(str);
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
            log_warn("the session's(id = %s) client had exist !", trans_info->sessionid);
            /*client_t *old_client = session->client;
            close(old_client->fd);
            free(old_client);*/
            ev_timer *close_timeout = &session->close_timeout;
            if (close_timeout) {
                ev_timer_stop(ev_default_loop(0), close_timeout);
            }

            if (session->queue) {
                // TODO: bugs need to fix within IE6 browser
                // in sometimes, g_queue_free may cause g_queue_pop_tail () invald
                /*g_queue_free(session->queue);*/
                /*g_queue_clear(session->queue);*/
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