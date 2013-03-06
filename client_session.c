#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "socket_io.h"
#include "endpoint.h"
#include "store.h"

void broadcast_clients(char *except_sessionid, char *message) {
    GList *list = get_store_list();
    GList *it = NULL;
    for (it = list; it; it = it->next) {
        char *sessionid = it->data;
        if (strlen(sessionid) == 0) {
            fprintf(stderr, "the sessioin is NULL ****************\n");
            continue;
        }

        if (except_sessionid != NULL && strcmp(except_sessionid, sessionid) == 0) {
            continue;
        }

        insert_msg_into_queue(sessionid, message);
    }

    g_list_free(list);
}

void insert_msg_into_queue(char *sessionid, char *message) {
    session_t *session = store_lookup(sessionid);
    if (!session) {
        fprintf(stderr, "The sessionid %s has no value !\n", sessionid);
        return;
    }

    GQueue *queue = session->queue;
    if (queue == NULL) {
        fprintf(stderr, "The queue is NULL !\n");
        return;
    }

    g_queue_push_head(queue, g_strdup(message));

    if (session->state != CONNECTED_STATE) {
        return;
    }

    client_t *client = session->client;
    if (client) {
        transports_fn *trans_fn = get_transport_fn(client);
        if (trans_fn) {
            trans_fn->output_callback(session);
        } else {
            fprintf(stderr, "Got NO transport struct !\n");
        }
    }
}

void notice_connect(message_fields *msg_fields, char *sessionid, char *post_msg) {
    session_t *session = store_lookup(sessionid);
    if (!session) {
        fprintf(stderr, "The sessionid %s has no value !\n", sessionid);
        return;
    }
    session->state = CONNECTED_STATE;
    session->endpoint = g_strdup(msg_fields->endpoint);

    insert_msg_into_queue(sessionid, post_msg);
}

void notice_disconnect(message_fields *msg_fields, char *sessionid) {
    session_t *session = store_lookup(sessionid);
    if (!session) {
        fprintf(stderr, "The sessionid %s has no value !\n", sessionid);
        return;
    }

    session->state = DISCONNECTED_STATE;
    client_t *client = session->client;
    if (client) {
        on_close(client);
        session->client = NULL;
    }

    store_remove(sessionid);
    g_free(session);
}