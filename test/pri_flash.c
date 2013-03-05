/**
**  gcc pri_flash.c -o pri_flash ../include/libev.a -lm
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

#define FLASH_SECURITY_REQ "<policy-file-request/>"
#define FLASH_SECURITY_FILE "<cross-domain-policy><allow-access-from domain='*' to-ports='*' /></cross-domain-policy>"

ev_io flash_ev_accept;

static void free_res(struct ev_loop *loop, ev_io *ws);

static int setnonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0)
        return flags;

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
        return -1;

    return 0;
}

static void read_cb(struct ev_loop *loop, ev_io *w, int revents) {
    int r = 0;
    char rbuff[1024] = "";
    if (revents & EV_READ) {
        r = read(w->fd, &rbuff, 1024);
    }

    if (EV_ERROR & revents) {
        printf("error event in read\n");
        free_res(loop, w);
        return ;
    }

    if (r < 0) {
        printf("read error\n");
        ev_io_stop(EV_A_ w);
        free_res(loop, w);
        return;
    }

    if (r == 0) {
        printf("client disconnected.\n");
        free_res(loop, w);
        return;
    }

    int result = strcmp(FLASH_SECURITY_REQ, rbuff);
    printf("result = %d\n", result);
    if(result == 0){
        write(w->fd, FLASH_SECURITY_FILE, strlen(FLASH_SECURITY_FILE));
    }else{
        printf("NOT MATCH !\n");
    }

    free_res(loop, w);
}   

static void accept_cb(struct ev_loop *loop, ev_io *w, int revents) {
    printf("here ............\n");
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(w->fd, (struct sockaddr *) &client_addr, &client_len);
    if (client_fd == -1) {
        printf("the client_fd is  NULL !\n");
        return;
    }

    if (setnonblock(client_fd) < 0)
        err(1, "failed to set client socket to non-blocking");

    ev_io *ev_read = malloc(sizeof(ev_io));
    ev_io_init(ev_read, read_cb, client_fd, EV_READ);
    ev_io_start(loop, ev_read);
    free(w);
}

void start_flash_policy_server(struct ev_loop *sloop, int flash_securiy_port) {
    // struct ev_loop *loop = ev_loop_new(0);
    int reuseaddr_on = 1;
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
        err(1, "listen failed");
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, sizeof(reuseaddr_on)) == -1)
        err(1, "setsockopt failed");

    struct sockaddr_in socket_listen_addr;
    memset(&socket_listen_addr, 0, sizeof(socket_listen_addr));
    socket_listen_addr.sin_family = AF_INET;
    socket_listen_addr.sin_addr.s_addr = INADDR_ANY;
    socket_listen_addr.sin_port = htons(flash_securiy_port);

    if (bind(listen_fd, (struct sockaddr *) &socket_listen_addr, sizeof(socket_listen_addr)) < 0)
        err(1, "socket_listen_addr bind failed");

    if (listen(listen_fd, 5) < 0)
        err(1, "listen failed");
    if (setnonblock(listen_fd) < 0)
        err(1, "failed to set server socket to non-blocking");

    // ev_io *flash_ev_accept = malloc(sizeof(ev_io));

    ev_io_init(&flash_ev_accept, accept_cb, listen_fd, EV_READ);

    struct ev_loop *loop = ev_loop_new(1);
    ev_io_start(loop, &flash_ev_accept);

    ev_loop(loop, 0);
}

static void free_res(struct ev_loop *loop, ev_io *w) {
    ev_io_stop(EV_A_ w);
    ev_io_stop(loop, w);
    close(w->fd);
    free(w);
}

// int main(int argc, char const *argv[]) {
//     printf("prepare start ...!\n");
    
//     struct ev_loop *loop = ev_default_loop(0);
//     int flash_securiy_port = 10843;
//     start_flash_policy_server(loop, flash_securiy_port);

//     ev_loop(loop, 0);

//     return 0;
// }