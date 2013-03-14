#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "socket_io.h"
#include "endpoint.h"
#include "transports.h"

http_parser_settings settings;
ev_io ev_accept;

static void free_res(struct ev_loop *loop, ev_io *ws);
void free_client(struct ev_loop *loop, client_t *client); typedef int MyCustomType;
void on_close(client_t *client);

int setnonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0)
        return flags;

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
        return -1;

    return 0;
}

static void read_cb(struct ev_loop *loop, ev_io *w, int revents) {
    client_t *client = w->data;
    if (EV_ERROR & revents) {
        printf("error event in read\n");
        free_res(loop, w);
        return ;
    }

    if (!(revents & EV_READ)) {
        fprintf(stdout, "revents & EV_READ is false !\n");
        free_res(loop, w);
        return;
    }

    /*int len = 0;
    char rbuff[REQUEST_BUFFER_SIZE];
    memset(rbuff, '\0', REQUEST_BUFFER_SIZE);
    len = read(client->fd, &rbuff, REQUEST_BUFFER_SIZE);*/

    char read_buff[REQUEST_BUFFER_SIZE + 1];
    int sum = 0, len = 0;
    char *request_data = NULL;


    while ((len = read(client->fd, &read_buff, REQUEST_BUFFER_SIZE)) > 0) {
        /*len = read(client->fd, &read_buff, REQUEST_BUFFER_SIZE);*/
        sum += len;
        if (len < REQUEST_BUFFER_SIZE)
            read_buff[len] = '\0';
        if (request_data == NULL) {
            request_data = malloc(len + 1);
            memcpy(request_data, read_buff, len);
        } else {
            request_data = realloc(request_data, sum + 1);
            memcpy(request_data + sum - len, read_buff, len);
        }
    }
    len = sum;
    char *rbuff = request_data;

    /*    do {
            len = read(client->fd, &rbuff, REQUEST_BUFFER_SIZE);
            sum += len;
            if (len < REQUEST_BUFFER_SIZE)
                rbuff[len] = '\0';
            if (request_data == NULL) {
                request_data = malloc(len+1);
                memcpy(request_data, rbuff, len);
            } else {
                printf("request_data is not null!\n");
                request_data = realloc(request_data, sum + 1);
                memcpy(request_data + sum - len, rbuff, len);
            }
        } while (len == REQUEST_BUFFER_SIZE);  */

    if (len < 0) {
        printf("read error\n");
        handle_disconnected(client);
        free_res(loop, w);
        return;
    }

    if (len == 0) {
        /*printf("client disconnected with read buff len = 0.\n");*/
        handle_disconnected(client);
        free_res(loop, w);
        return;
    }

    size_t parsed = 0;
    if (client->trans_version != TRANSPORT_WEBSOCKET_VERSION) {
        client->data = rbuff;
        http_parser_init(&client->parser, HTTP_BOTH);
        parsed = http_parser_execute(&client->parser, &settings, rbuff, len);
    }

    if (client->parser.upgrade) {
        if (parsed == 0) {
            transport_info *trans_info = &client->trans_info;

            if (strstr(trans_info->transport, "websocket")) {
                unsigned char dst[REQUEST_BUFFER_SIZE];
                memset(dst, '\0', REQUEST_BUFFER_SIZE);
                int result = WEBSOCKET_get_content(rbuff, len, dst, REQUEST_BUFFER_SIZE);

                if (result == -2) {
                    printf("websocket client close the connection now ...\n");
                    // handle disconnecting event ...
                    handle_disconnected(client);
                    fprintf(stderr, "now close the connection .............\n");
                    free_res(loop, w);
                } else {
                    handle_body_cb(client, dst, NULL);
                }
            } else {
                char data[len];
                memset(data, '\0', len);
                memcpy(data, rbuff + 1, len - 2);
                handle_body_cb(client, data, NULL);
            }
        }
    } else {
        if (parsed == 0) {
            if (strcmp(FLASH_SECURITY_REQ, rbuff) == 0) {
                write(client->fd, FLASH_SECURITY_FILE, strlen(FLASH_SECURITY_FILE));
                free_res(loop, w);
            } else {
                handle_body_cb(client, rbuff, on_close);
            }
        } else if (parsed < len) {
            fprintf(stderr, "parse error(len=%d, parsed=%d) and rbuff is \n%s \n", len, (int)parsed, rbuff);

            handle_disconnected(client);

            free_res(loop, w);
        }
    }

    free(rbuff);
}

static void accept_cb(struct ev_loop *loop, ev_io *w, int revents) {
    client_t *client;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(w->fd, (struct sockaddr *) &client_addr, &client_len);
    if (client_fd == -1) {
        printf("the client_fd is  NULL !\n");
        return;
    }

    client = calloc(1, sizeof(*client));
    client->fd = client_fd;
    if (setnonblock(client->fd) < 0)
        err(1, "failed to set client socket to non-blocking");

    client->ev_read.data = client;
    client->timeout.data = NULL;
    client->parser.data = client;
    client->trans_version = 0;
    client->data = NULL;

    ev_io_init(&client->ev_read, read_cb, client->fd, EV_READ);
    ev_io_start(loop, &client->ev_read);
}

void server_init(void) {
    init_config();
    store_init();
    transports_fn_init();
    endpoints_init();

    settings.on_url = on_url_cb;
    settings.on_body = on_body_cb;
}

void server_register_endpoint(endpoint_implement *endpoint_impl) {
    if (endpoint_impl == NULL) {
        fprintf(stderr, "the endpoint_impl is NULL !\n");
        return;
    }

    endpoints_register(endpoint_impl->endpoint, endpoint_impl);
    endpoint_impl->on_init(endpoint_impl->endpoint);
}

void server_run(int port) {
    struct sockaddr_in listen_addr;
    int reuseaddr_on = 1;
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
        err(1, "listen failed");
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, sizeof(reuseaddr_on)) == -1)
        err(1, "setsockopt failed");

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *) &listen_addr, sizeof(listen_addr)) < 0)
        err(1, "bind failed");
    if (listen(listen_fd, 5) < 0)
        err(1, "listen failed");
    if (setnonblock(listen_fd) < 0)
        err(1, "failed to set server socket to non-blocking");

    struct ev_loop *loop = ev_default_loop(0);
    ev_io_init(&ev_accept, accept_cb, listen_fd, EV_READ);
    ev_io_start(loop, &ev_accept);

    ev_loop(loop, 0);
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

static void free_res(struct ev_loop *loop, ev_io *w) {
    ev_io_stop(EV_A_ w);
    client_t *cli = w->data;
    free_client(loop, cli);
}

void on_close(client_t *client) {
    free_client(ev_default_loop(0), client);
}

void write_output(client_t *client, char *msg, void (*fn)(client_t *client)) {
    if (client == NULL) {
        printf("the client is NULL !\n");
        return;
    }

    if (msg) {
        write(client->fd, msg, strlen(msg));
    }
    if (fn) {
        fn(client);
    }
}