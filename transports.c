#include "transports.h"

static GHashTable *_transport_fn_hash;

void transports_fn_init(void) {
    _transport_fn_hash = g_hash_table_new(g_str_hash, g_str_equal);

    transports_fn *xhr_trans_fn = init_xhr_polling_transport();
    transports_fn *jsonp_trans_fn = init_jsonp_polling_transport();
    transports_fn *htmlfile_trans_fn = init_htmlfile_polling_transport();
    transports_fn *websocket_trans_fn = init_websocket_transport();
    transports_fn *flashsocket_trans_fn = init_flashsocket_transport();

    g_hash_table_insert(_transport_fn_hash, g_strdup(xhr_trans_fn->name), xhr_trans_fn);
    g_hash_table_insert(_transport_fn_hash, g_strdup(jsonp_trans_fn->name), jsonp_trans_fn);
    g_hash_table_insert(_transport_fn_hash, g_strdup(htmlfile_trans_fn->name), htmlfile_trans_fn);
    g_hash_table_insert(_transport_fn_hash, g_strdup(websocket_trans_fn->name), websocket_trans_fn);
    g_hash_table_insert(_transport_fn_hash, g_strdup(flashsocket_trans_fn->name), flashsocket_trans_fn);
}

void *get_transport_fn(client_t *client) {
    if (client == NULL)
        return NULL;

    transport_info *trans_info = &client->trans_info;
    char *transport_name = trans_info->transport;

    return g_hash_table_lookup(_transport_fn_hash, transport_name);
}