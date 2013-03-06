#include <stdio.h>
#include <stdlib.h>
#include "store.h"

static GHashTable* _store_session_hash;

// gboolean wide_open(gpointer key, gpointer value, gpointer user_data) {
//     return TRUE;
// }

static void value_destroyed(gpointer data) {
}

static void key_destroyed(gpointer data) {
}

void store_init(void){
	_store_session_hash = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)key_destroyed,(GDestroyNotify)value_destroyed);
}

void store_add(char *key, void *value){
	g_hash_table_insert(_store_session_hash, g_strdup(key), value);
}

gboolean store_remove(char *key){
	return g_hash_table_remove(_store_session_hash, key);
}

gpointer store_lookup(char *key){
	return g_hash_table_lookup(_store_session_hash, key);
}

gint store_size(void){
	return g_hash_table_size(_store_session_hash);
}

void store_destroy(void){
    g_hash_table_destroy(_store_session_hash);
}

GList *get_store_list(){
	return g_hash_table_get_keys(_store_session_hash);
}