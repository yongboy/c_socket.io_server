#ifndef _STORE_H_
#define _STORE_H_
#include <glib.h>

void store_init(void);

void store_add(char *key, void *value);

gboolean store_remove(char *key);

gpointer store_lookup(char *key);

gint store_size(void);

void store_destroy(void);

#endif