#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>

#include <uuid/uuid.h>
#include "socket_io.h"
#include "endpoint.h"
#include "transports.h"
#include "safe_mem.h"

config *global_config;
void handle_disconnected(client_t *client);

void init_config() {
    GKeyFile *keyfile;
    GKeyFileFlags flags;
    GError *error = NULL;

    keyfile = g_key_file_new();
    flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

    if (!g_key_file_load_from_file (keyfile, "server.ini", flags, &error)) {
        log_fatal("%s\n", "the server.ini not found !");
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
        log_debug("uuid not generated safely");
    }

    uuid_unparse(uuid, uuidBuff);

    return uuidBuff;
}

void handle_disconnected(client_t *client) {
    if (client == NULL) {
        log_warn("the client is NULL !");
        return;
    }

    http_parser *parser = &client->parser;
    if (parser == NULL) {
        return;
    }
    if (parser && parser->method == HTTP_POST) {
        log_warn("parser && parser->method == HTTP_POST is true!");
        return;
    }

    transport_info *trans_info = &client->trans_info;
    if (trans_info == NULL) {
        log_fatal("the trans_info is NULL !\n");
        return;
    }

    char *sessionid = trans_info->sessionid;
    if (sessionid == NULL) {
        log_warn("handle_disconnected's the sessionid is NULL");
        return;
    }

    session_t *session = store_lookup(sessionid);
    if (session == NULL) {
        log_warn("handle_disconnected's the session is NULL");
        return;
    }

    session->state = DISCONNECTED_STATE;

    // something need to handle on_disconnected event ...
    if (session->endpoint) {
        endpoint_implement *endpoint_impl = endpoints_get(session->endpoint);
        if (endpoint_impl) {
            endpoint_impl->on_disconnect(sessionid, NULL);
        } else {
            log_warn("the endpoint_impl is null !\n");
        }
    }

    // set timeout for delete the session
    transports_fn *trans_fn = get_transport_fn(client);
    if (trans_fn) {
        trans_fn->end_connect(sessionid);
    } else {
        log_warn("Got NO transport struct !\n");
    }
}



void free_client(struct ev_loop *loop, client_t *client) {
    if (client == NULL) {
        printf("free_res the clientent is NULL !!!!!!\n");
        return;
    }
    ev_io_stop(loop, &client->ev_read);

    ev_timer *timer = &client->timeout;
    if (timer != NULL && (timer->data != NULL)) {
        ev_timer_stop(loop, timer);
    }

    http_parser *parser = &client->parser;
    if (parser && parser->method == HTTP_GET) {
        transport_info *trans_info = &client->trans_info;
        if (trans_info->sessionid) {
            session_t *session = store_lookup(trans_info->sessionid);
            if (session != NULL) {
                session->client = NULL;
            }
        }
    }

    client->data = NULL;

    close(client->fd);

    if (client)
        free(client);
}

void free_res(struct ev_loop *loop, ev_io *w) {
    ev_io_stop(EV_A_ w);
    client_t *cli = w->data;
    free_client(loop, cli);
}

void on_close(client_t *client) {
    free_client(ev_default_loop(0), client);
}

void write_output(client_t *client, char *msg, void (*fn)(client_t *client)) {
    if (client == NULL) {
        log_error("the client is NULL !");
        return;
    }

    if (msg) {
        ssize_t write_len = write(client->fd, msg, strlen(msg));
        if (write_len == -1) {
            log_warn("write failed(errno = %d): %s", errno, strerror(errno));
        }
    }else{
        log_warn("the msg is NULL with file id = %d", client->fd);
    }

    if (fn) {
        fn(client);
    }
}