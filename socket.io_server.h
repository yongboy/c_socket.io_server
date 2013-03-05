#ifndef SOCKET_IO_SERVER_H
#define SOCKET_IO_SERVER_H
#include "endpoint.h"

void server_init(void);

void server_register_endpoint(endpoint_implement *endpoint_impl);

void server_run(int port);

#endif