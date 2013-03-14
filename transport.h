#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdlib.h>
#include <stdio.h>

#include "store.h"
#include "socket_io.h"
#include "safe_mem.h"

static void output_callback(session_t *session);
static void output_header(client_t *client);
static void output_body(client_t *client, char *http_msg);
static void output_whole(client_t *client, char *output_msg);
static void timeout_callback(ev_timer *timer) {
}

static void heartbeat_callback(client_t *client, char *sessionid) {
}

static void init_connect(client_t *client, char *sessionid) {
    session_t *session = (session_t *)store_lookup(sessionid);
    if (session) {
        ev_timer *timeout = &session->close_timeout;
        if (timeout == NULL) {
            fprintf(stderr, "init_connect time is NULL!\n");
            return;
        }
        ev_timer_stop(ev_default_loop(0), timeout);
        free(timeout->data);
    }
}

static void end_connect(char *sessionid) {
    session_t *session = (session_t *)store_lookup(sessionid);
    if (session == NULL) {
        fprintf(stderr, "the end_connect session is NULL!\n");
        return;
    }

    ev_timer *timeout = &session->close_timeout;
    if (timeout == NULL) {
        fprintf(stderr, "end_connect timeout is NULL!\n");
        return;
    }
    timeout->data = g_strdup(sessionid);

    extern config *global_config;
    ev_timer_set(timeout, global_config->server_close_timeout, 0);
    ev_timer_start(ev_default_loop(0), timeout);
}

static void common_output_callback(session_t *session, int keep_long) {
    if (session == NULL) {
        fprintf(stderr, "the Session is NULL\n");
        return;
    }

    client_t *client = session->client;
    if (client == NULL) {
        fprintf(stderr, "the client is NULL!\n");
        return;
    }

    GQueue *queue = session->queue;
    if (queue == NULL) {
        fprintf(stderr, "the queue is null now !\n");
        return;
    }

    if (keep_long) {
        char *str;
        while ((str = (char *)g_queue_pop_tail(queue)) != NULL) {
            output_body(client, str);
        }
    } else {
        char *str = (char *)g_queue_pop_tail(queue);
        if (str == NULL) {
            fprintf(stderr, "the str is NULL!\n");
        } else {
            output_body(client, str);
        }
        ev_timer_stop(ev_default_loop(0), &client->timeout);

        session->client = NULL;
    }
}


static transports_fn *init_default_transport() {
    transports_fn *trans_fn = (transports_fn *)malloc(sizeof(transports_fn));

    trans_fn->output_callback = output_callback;
    trans_fn->output_header = output_header;
    trans_fn->output_body = output_body;
    trans_fn->output_whole = output_whole;
    trans_fn->timeout_callback = timeout_callback;
    trans_fn->heartbeat_callback = heartbeat_callback;

    trans_fn->init_connect = init_connect;
    trans_fn->end_connect = end_connect;

    return trans_fn;
}

#endif