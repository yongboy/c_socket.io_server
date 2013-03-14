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