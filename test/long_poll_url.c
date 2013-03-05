/**
**  gcc long_poll_url.c -o long_poll_url ../include/libev.a ../http-parser/http_parser.o -lm
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

#define HTMLFILE_RESPONSE_HEADER \
    "HTTP/1.1 200 OK\r\n" \
    "Connection: keep-alive\r\n" \
    "Content-Type: text/html; charset=utf-8\r\n" \
    "\r\n"

#define HTMLFILE_RESPONSE_FIRST \
    "<html><head></head><body><script>var _ = function (msg) { parent.s._(msg, document); };</script>                                                                                                                                                                                                                  \r\n"

#define HTMLFILE_RESPONSE_WELCOME "<script>_('1::');</script>\r\n"

#define HTMLFILE_RESPONSE_ECHO "<script>_('8::');</script>\r\n"    

#define HTMLFILE_RESPONSE_END "\r\n"

#define SERVER_PORT 8000

#define REQUEST_BUFFER_SIZE 2048

http_parser_settings settings;

typedef struct {
    int fd;
    ev_timer timeout;
    // ev_io ev_write;
    ev_io ev_read;
    http_parser parser;
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
    client_t *cli = timer->data;

    if (cli == NULL) {
        printf("Timeout the client is NULL !\n");
        return;
    }

    write_msg(cli, HTMLFILE_RESPONSE_ECHO);
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
    // client_t *cli = w->data;

    printf("write_cb revents is %d\n", revents);
    if (revents & EV_WRITE) {
        // printf("going to write message!\n");
        // write_msg(cli, HTMLFILE_RESPONSE_HEADER);
        // write_msg(cli, HTMLFILE_RESPONSE_FIRST);
        // write_msg(cli, HTMLFILE_RESPONSE_MIDDLE);

        // cli->timeout.data = cli;

        // ev_timer_init(&cli->timeout, timeout_cb, 10.0, 0);
        // ev_timer_start(loop, &cli->timeout);
        // printf("end going to write message!\n");

        ev_io_stop(EV_A_ w);
    } else {
        printf("ERROR !\n");
        free_res(loop, w);
    }

    // close(cli->fd);
    // free (cli);
}

static void read_cb(struct ev_loop *loop, ev_io *w, int revents) {
    client_t *client = w->data;
    int len = 0;
    char rbuff[1024];
    if (revents & EV_READ) {
        len = read(client->fd, &rbuff, 1024);
    }
    printf("read length is %d\n", len);

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

    rbuff[len] = '\0';
    http_parser_init(&client->parser, HTTP_REQUEST);

    size_t parsed = http_parser_execute(&client->parser, &settings, rbuff, len);
    if (parsed < len) {
        fprintf(stderr, "parse error\n");
        ev_io_stop(EV_A_ w);
        free_res(loop, w);
    }

    // ev_io_stop(EV_A_ w);
    // ev_io_init(&client->ev_write, write_cb, client->fd, EV_WRITE);
    // ev_io_start(loop, &client->ev_write);
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
    // cli->ev_write.data = cli;
    cli->timeout.data = NULL;
    cli->parser.data = cli;

    ev_io_init(&cli->ev_read, read_cb, cli->fd, EV_READ);
    ev_io_start(loop, &cli->ev_read);
}

int on_url_cb(http_parser *parser, const char *at, size_t length) {
    char urlStr[100];
    sprintf(urlStr, "%.*s", (int)length, at);
    fprintf(stdout, "the url is %s\n", urlStr);

    client_t *client = parser->data;

    printf("on url cb going to write message!\n");
    write_msg(client, HTMLFILE_RESPONSE_HEADER);
    write_msg(client, HTMLFILE_RESPONSE_FIRST);
    write_msg(client, HTMLFILE_RESPONSE_WELCOME);

    client->timeout.data = client;

    ev_timer_init(&client->timeout, timeout_cb, 10.0, 0);
    ev_timer_start(ev_default_loop(0), &client->timeout);
    printf("on url cb end going to write message!\n");

    return 0;
}

int on_body_cb(http_parser *parser, const char *at, size_t length) {
    // client_t *client_t = parser->data;
    // transport_info *trans_info = &client_t->trans_info;

    // char post_msg[(int)length + 100];
    // sprintf(post_msg, "%.*s", (int)length, at);

    // if (strchr(post_msg, 'd') == post_msg) {
    //     char *unescape_string = g_uri_unescape_string(post_msg, NULL);
    //     gchar *result = g_strcompress(unescape_string);
    //     char target[strlen(result) - 4];
    //     strncpy(target, result + 3, strlen(result));
    //     target[strlen(target) - 1] = '\0';

    //     strcpy(post_msg, target);
    //     g_free(result);
    // }

    // session_t *session = store_lookup(trans_info->sessionid);
    // GQueue *queue = session->queue;

    // message_fields msg_fields;
    // body_2_struct(post_msg, &msg_fields);
    // endpoint_implement *endpoint_impl = endpoints_get(msg_fields.endpoint);
    // int num = atoi(msg_fields.message_type);
    // switch (num) {
    // case 0:
    //     endpoint_impl->on_disconnect(trans_info->sessionid, &msg_fields);
    //     break;
    // case 1:
    //     g_queue_push_head(queue, g_strdup(post_msg));
    //     endpoint_impl->on_connect(trans_info->sessionid, &msg_fields);
    //     break;
    // case 2:
    //     break;
    // case 5:
    //     endpoint_impl->on_message(trans_info->sessionid, &msg_fields);
    //     break;
    // }

    // char http_msg[200];
    // sprintf(http_msg, RESPONSE_PLAIN, 1, "1");
    // common_http_output(http_msg, client_t, after_write);

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
        // sprintf(stderr, "the client is NULL !!!!!!", "");
        printf("the client is NULL !!!!!!");
        return;
    }

    // ev_io_stop(loop, &cli->ev_write);
    ev_io_stop(loop, &cli->ev_read);

    ev_timer *timer = &cli->timeout;
    if (timer != NULL && (timer->data != NULL)) {
        ev_timer_stop(loop, timer);
    }

    close(cli->fd);

    free(cli);
}