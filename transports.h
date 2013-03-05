#ifndef TRANSPORTS_H
#define TRANSPORTS_H
#include "socket_io.h"

void transports_fn_init(void);
void *get_transport_fn(client_t *client);

#endif