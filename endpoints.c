#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include "store.h"
#include "socket_io.h"
#include "endpoint.h"

static GHashTable *endpoint_hashs;

static void endpoints_value_destroyed(gpointer data) {
    /*g_free(data);*/
}

static void endpoints_key_destroyed(gpointer data) {
    /*g_printf("endpoint was destroyed with key %s\n", (char *)data);*/
}

void endpoints_init(void) {
    endpoint_hashs = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)endpoints_key_destroyed, (GDestroyNotify)endpoints_value_destroyed);
}

void endpoints_register(char *key, endpoint_implement *endpoint_impl) {
    g_hash_table_insert(endpoint_hashs, g_strdup(key), endpoint_impl);
}

gboolean endpoint_unregister(char *key) {
    return g_hash_table_remove(endpoint_hashs, g_strdup(key));
}

endpoint_implement *endpoints_get(char *key) {
    return g_hash_table_lookup(endpoint_hashs, g_strdup(key));
}

gint endpoints_size(void) {
    return g_hash_table_size(endpoint_hashs);
}