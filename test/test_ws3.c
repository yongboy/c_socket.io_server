/**
**  gcc test_ws3.c ../c-websocket/*.c -o test_ws3 ../include/libev.a ../http-parser/http_parser.o -lm
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <stddef.h>

#include "../include/ev.h"
#include "../http-parser/http_parser.h"
#include "../c-websocket/cWebSockets.h"
#define SERVER_PORT 8000

#define TRANSPORT_WEBSOCKET_VERSION 4

#define webdis_htonl64(p) {\
(char)(((p & ((uint64_t)0xff << 0)) >> 0) & 0xff), (char)(((p & ((uint64_t)0xff << 8)) >> 8) & 0xff), \
(char)(((p & ((uint64_t)0xff << 16)) >> 16) & 0xff), (char)(((p & ((uint64_t)0xff << 24)) >> 24) & 0xff), \
(char)(((p & ((uint64_t)0xff << 32)) >> 32) & 0xff), (char)(((p & ((uint64_t)0xff << 40)) >> 40) & 0xff), \
(char)(((p & ((uint64_t)0xff << 48)) >> 48) & 0xff), (char)(((p & ((uint64_t)0xff << 56)) >> 56) & 0xff) }

#define REQUEST_BUFFER_SIZE 2048

http_parser_settings settings;

typedef struct {
    int fd;
    ev_timer timeout;
    // ev_io ev_write;
    ev_io ev_read;
    http_parser parser;

    int trans_version;
    int head_done;
    // char *request_data;
} client_t;

ev_io ev_accept;

static void free_res(struct ev_loop *loop, ev_io *ws);

int setnonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0)
        return flags;

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
        return -1;

    return 0;
}

static void write_msg(client_t *cli, char *msg) {
    if (cli == NULL) {
        printf("the client is NULL !\n");
        return;
    }

    write(cli->fd, msg, strlen(msg));
}

int ws_reply(client_t *cli, const char *p, size_t sz) {
    char *frame = malloc(sz + 8); /* create frame by prepending header */
    size_t frame_sz = 0;
    if (frame == NULL)
        return -1;

    /*
    The length of the "Payload data", in bytes: if 0-125, that is the
    payload length. If 126, the following 2 bytes interpreted as a
    16-bit unsigned integer are the payload length. If 127, the
    following 8 bytes interpreted as a 64-bit unsigned integer (the
    most significant bit MUST be 0) are the payload length.
    */
    frame[0] = '\x81';
    if (sz <= 125) {
        frame[1] = sz;
        memcpy(frame + 2, p, sz);
        frame_sz = sz + 2;
    } else if (sz > 125 && sz <= 65536) {
        uint16_t sz16 = htons(sz);
        frame[1] = 126;
        memcpy(frame + 2, &sz16, 2);
        memcpy(frame + 4, p, sz);
        frame_sz = sz + 4;
    } else if (sz > 65536) {
        char sz64[8] = webdis_htonl64(sz);
        frame[1] = 127;
        memcpy(frame + 2, sz64, 8);
        memcpy(frame + 10, p, sz);
        frame_sz = sz + 10;
    }

    // write_msg(cli, dst);
    write(cli->fd, frame, frame_sz);

    return 0;
}

void write_ws_msg(client_t *cli, char *msg) {
    ws_reply(cli, msg, strlen(msg));
}

static void timeout_cb(EV_P_ struct ev_timer *timer, int revents) {
    if (EV_ERROR & revents) {
        printf("error event in timer_beat\n");
        return ;
    }

    if (timer == NULL) {
        printf("the timer is NULL now !\n");
    }

    client_t *cli = timer->data;

    if (cli == NULL) {
        printf("Timeout the client is NULL !\n");
        return;
    }

    char msg[] = "1::";
    write_ws_msg(cli, msg);

    ev_timer_set(&cli->timeout, 10.0, 0);
    ev_timer_start(ev_default_loop(0), &cli->timeout);
}

void hand_shake(client_t *client, const char *rbuff) {
    char dst[10240];
    WEBSOCKET_generate_handshake(rbuff, dst, 10240);
    write_msg(client, dst);

    client->timeout.data = client;
    ev_timer_init(&client->timeout, timeout_cb, 10.0, 0);
    ev_timer_start(ev_default_loop(0), &client->timeout);
}

static void read_cb(struct ev_loop *loop, ev_io *w, int revents) {
    client_t *client = w->data;
    int len = 0;
    char rbuff[99999] = "";
    if (revents & EV_READ) {
        len = read(client->fd, &rbuff, 99999);
    }

    if (EV_ERROR & revents) {
        printf("error event in read\n");
        free_res(loop, w);
        return ;
    }

    if (len < 0) {
        printf("read error\n");
        ev_io_stop(EV_A_ w);
        free_res(loop, w);
        return;
    }

    if (len == 0) {
        printf("client disconnected.\n");
        ev_io_stop(EV_A_ w);
        free_res(loop, w);
        return;
    }

    // rbuff[len] = '\0';

    // size_t parsed = 0;
    // if(client->trans_version != TRANSPORT_WEBSOCKET_VERSION){
    //     http_parser_init(&client->parser, HTTP_REQUEST);
    //     parsed = http_parser_execute(&client->parser, &settings, rbuff, len);
    // }
    // if (client->parser.upgrade) {
    // client->trans_version = TRANSPORT_WEBSOCKET_VERSION;

    // int check_websocket_result = WEBSOCKET_valid_connection(rbuff);

    if (client->head_done == 0) {
        client->head_done = 1;
        hand_shake(client, rbuff);

        // write_ws_msg(client, "5::/chat:{\"name\":\"user message\",\"args\":[\"firefox\",\"顶顶顶顶的顶顶顶顶顶顶顶顶顶顶顶顶顶顶顶的顶顶顶顶顶顶顶顶顶顶的顶顶顶顶顶顶顶顶顶顶顶顶顶顶顶顶顶\"]}");
    } else {
        char dst[10240] = "";
        int result = WEBSOCKET_get_content(rbuff, len, dst, 10240);

        if (result == -2) {
            free_res(loop, w);
        } else {
            printf("write long message5 !\n");
            write_ws_msg(client, "顶顶顶顶的顶顶顶顶顶顶顶顶顶顶顶顶顶顶顶的顶顶顶顶顶顶顶顶顶顶的顶顶顶顶的顶顶顶顶的顶顶顶顶的顶顶顶顶的顶顶顶");//130
        }
    }
}

static void accept_cb(struct ev_loop *loop, ev_io *w, int revents) {
    client_t *cli;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(w->fd, (struct sockaddr *) &client_addr, &client_len);
    if (client_fd == -1) {
        printf("the client_fd is  NULL !\n");
        return;
    }

    cli = calloc(1, sizeof(*cli));
    cli->fd = client_fd;
    if (setnonblock(cli->fd) < 0)
        err(1, "failed to set client socket to non-blocking");

    cli->ev_read.data = cli;
    cli->timeout.data = NULL;
    cli->parser.data = cli;
    cli->head_done = 0;
    cli->trans_version = 1;

    ev_io_init(&cli->ev_read, read_cb, cli->fd, EV_READ);
    ev_io_start(loop, &cli->ev_read);
}

int on_url_cb(http_parser *parser, const char *at, size_t length) {
    char urlStr[100];
    sprintf(urlStr, "%.*s", (int)length, at);
    fprintf(stdout, "the url is %s\n", urlStr);

    // client_t *client = parser->data;

    if (strstr(at, "Sec-WebSocket-Version") > 0) {
        return 0;
    }

    printf("on_url_cb here .............\n");
    return 0;
}

int on_body_cb(http_parser *parser, const char *at, size_t length) {
    printf("on_body_cb here .....................\n");
    return 0;
}

int main(int argc, char const *argv[]) {
    struct ev_loop *loop = ev_default_loop(0);

    settings.on_url = on_url_cb;
    settings.on_body = on_body_cb;


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
    listen_addr.sin_port = htons(SERVER_PORT);

    if (bind(listen_fd, (struct sockaddr *) &listen_addr, sizeof(listen_addr)) < 0)
        err(1, "bind failed");
    if (listen(listen_fd, 5) < 0)
        err(1, "listen failed");
    if (setnonblock(listen_fd) < 0)
        err(1, "failed to set server socket to non-blocking");

    ev_io_init(&ev_accept, accept_cb, listen_fd, EV_READ);
    ev_io_start(loop, &ev_accept);
    ev_loop(loop, 0);

    return 0;
}

static void free_res(struct ev_loop *loop, ev_io *w) {
    client_t *cli = w->data;
    if (cli == NULL) {
        printf("the client is NULL !!!!!!");
        return;
    }

    ev_io_stop(loop, &cli->ev_read);

    ev_timer *timer = &cli->timeout;
    if (timer != NULL && (timer->data != NULL)) {
        ev_timer_stop(loop, timer);
    }

    close(cli->fd);

    free(cli);
}