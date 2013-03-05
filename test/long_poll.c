/**
**  gcc long_poll.c -o long_poll ./include/libev.a -lm
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
#include "include/ev.h"

#define HTMLFILE_RESPONSE_HEADER \
    "HTTP/1.1 200 OK\r\n" \
    "Connection: keep-alive\r\n" \
    "Content-Type: text/html; charset=utf-8\r\n" \
    "\r\n"

#define HTMLFILE_RESPONSE_FIRST \
    "<html><head></head><body><script>var _ = function (msg) { parent.s._(msg, document); };</script>                                                                                                                                                                                                                  \r\n"

#define HTMLFILE_RESPONSE_MIDDLE "<script>_('1::');</script>\r\n"

#define HTMLFILE_RESPONSE_END "\r\n"

#define SERVER_PORT 8000

typedef struct client {
    int fd;
    ev_timer timeout;
    ev_io ev_write;
    ev_io ev_read;
} client;

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

static void write_msg(client *cli, char *msg) {
    if (cli == NULL) {
        printf("the client is NULL !\n");
        return;
    }

    printf("cli->fd is %d\n", cli->fd);

    write(cli->fd, msg, strlen(msg));
}

static void timeout_cb(EV_P_ struct ev_timer *timer, int revents) {
    printf("timeout now ~\n");
    printf("timeout revents is %d\n", revents);

    if (EV_ERROR & revents) {
        printf("error event in timer_beat\n");
        return ;
    }

    if (timer == NULL) {
        printf("the timer is NULL now !\n");
    }
    
    // client *cli = (client *) ( ((char *)timer) - offsetof(struct client, timeout) );
    client *cli = timer->data;

    if (cli == NULL) {
        printf("Timeout the client is NULL !\n");
        return;
    }

    write_msg(cli, HTMLFILE_RESPONSE_MIDDLE);
    // reduce repeat time interval by 1 seconds.
    // timer->repeat -= 1.0;
    // printf("timeout - repeat: %lf\n", timer->repeat);
    ev_timer_init(&cli->timeout, timeout_cb, 10.0, 0);
    ev_timer_start(loop, &cli->timeout);
    printf("timeout done !\n");
}

static void write_cb(struct ev_loop *loop, ev_io *w, int revents) {
    /**
    offsetof，返回一个数据域在它所属的数据结构中的相对偏移，单位是size_t，宏定义如下：
    #define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
    将地址0强制转换为type *，那么0指向某一个type类型的对象，也就是此type对象的地址，那么(TYPE *)0)->MEMBER就是type的member域，现在取址后&((TYPE *)0)->MEMBER 就是member域的地址，
    因为type的地址为0，则member的地址实质上就是它相对于type地址的偏移。
    */
    // client *cli = (client *) ( ((char *)w) - offsetof(struct client, ev_write) );
    client *cli = w->data;

    printf("write_cb revents is %d\n", revents);
    if (revents & EV_WRITE) {
        printf("going to write message!\n");
        write_msg(cli, HTMLFILE_RESPONSE_HEADER);
        write_msg(cli, HTMLFILE_RESPONSE_FIRST);
        write_msg(cli, HTMLFILE_RESPONSE_MIDDLE);

        cli->timeout.data = cli;

        ev_timer_init(&cli->timeout, timeout_cb, 10.0, 0);
        ev_timer_start(loop, &cli->timeout);
        printf("end going to write message!\n");

        //use the timer to write something ...
        // write(cli->fd, superjared, strlen(superjared));

        ev_io_stop(EV_A_ w);
    } else {
        printf("ERROR !\n");
        free_res(loop, w);
    }

    // close(cli->fd);
    // free (cli);
}

static void read_cb(struct ev_loop *loop, ev_io *w, int revents) {
    // client *cli = (client *) ( ((char *)w) - offsetof(struct client, ev_read) );
    client *cli = w->data;
    int r = 0;
    char rbuff[1024];
    if (revents & EV_READ) {
        r = read(cli->fd, &rbuff, 1024);
    }
    printf("read length is %d\n", r);

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
        //ev_io_stop(loop, watcher);
        //free(watcher);
        //如果客户端断开连接，释放响应的资源，并且关闭监听
        // freelibev(loop, watcher->fd);
        ev_io_stop(EV_A_ w);
        free_res(loop, w);
        return;
    }

    // ev_io_stop(EV_A_ w);
    ev_io_init(&cli->ev_write, write_cb, cli->fd, EV_WRITE);
    ev_io_start(loop, &cli->ev_write);
}

static void accept_cb(struct ev_loop *loop, ev_io *w, int revents) {
    client *cli;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(w->fd, (struct sockaddr *) &client_addr, &client_len);
    if (client_fd == -1) {
        printf("the client_fd is  NULL !\n");
        return;
    }

    printf("accept_cb revents is %d\n", revents);
    /**
    * calloc（配置内存空间）
    相关函数 malloc，free，realloc，brk
    表头文件 #include <stdlib.h>
    定义函数 void *calloc(size_t nmemb，size_t size);
    函数说明 calloc()用来配置nmemb个相邻的内存单位，每一单位的大小为size，并返回指向第一个元素的指针。这和使用下列的方式效果相同:malloc(nmemb*size);
    不过，在利用calloc()配置内存时会将内存内容初始化为0。
    返回值 若配置成功则返回一指针，失败则返回NULL。
    */
    cli = calloc(1, sizeof(*cli));
    cli->fd = client_fd;
    if (setnonblock(cli->fd) < 0)
        err(1, "failed to set client socket to non-blocking");

    cli->ev_read.data = cli;
    cli->ev_write.data = cli;
    cli->timeout.data = NULL;

    // ev_io_init(&client->ev_read, read_cb, client->fd, EV_READ);
    // ev_io_start(loop, &client->ev_read);    
    ev_io_init(&cli->ev_read, read_cb, cli->fd, EV_READ);
    ev_io_start(loop, &cli->ev_read);
}

int main(int argc, char const *argv[]) {
    struct ev_loop *loop = ev_default_loop(0);
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

static void free_res(struct ev_loop *loop, ev_io *w){
    client *cli = w->data;
    if(cli == NULL){
        // sprintf(stderr, "the client is NULL !!!!!!", "");
        printf("the client is NULL !!!!!!");
        return;
    }

    ev_io_stop(loop, &cli->ev_write);
    ev_io_stop(loop, &cli->ev_read);

    ev_timer *timer = &cli->timeout;
    if(timer != NULL && (timer->data != NULL)){
        ev_timer_stop(loop, timer);
    }
    
    close(cli->fd);

    free(cli);
}