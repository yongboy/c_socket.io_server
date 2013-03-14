#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include <glib.h>

#include "include/ev.h"
#include "include/http-parser/http_parser.h"

#define RESPONSE_404 \
    "HTTP/1.1 404 Not Found\r\n"

#define RESPONSE_400 \
    "HTTP/1.1 400 Bad Request\r\n"

#define RESPONSE_TEMPLATE \
    "HTTP/1.1 200 OK\r\n" \
    "Connection: keep-alive\r\n" \
    "Content-Type: %s\r\n" \
    "Content-Length: %d\r\n"  \
    "\r\n"

#define RESPONSE_PLAIN \
    "HTTP/1.1 200 OK\r\n" \
    "Connection: keep-alive\r\n" \
    "Content-Type: text/plain\r\n" \
    "Content-Length: %d\r\n" \
    "\r\n" \
    "%s\n"

#define FLASH_SECURITY_REQ "<policy-file-request/>"
#define FLASH_SECURITY_FILE "<cross-domain-policy><allow-access-from domain='*' to-ports='*' /></cross-domain-policy>"

#define TRANSPORT_WEBSOCKET_VERSION 4
#define REQUEST_BUFFER_SIZE 8192

struct _ext_to_content_type {
    char *extnsn[6];
    char *contentname;
};

typedef struct {
    char *transport;
    char *sessionid;
    char *i;
    char *oriurl;
} transport_info;

typedef struct {
    int fd;
    ev_io ev_read;
    http_parser parser;
    ev_timer timeout;
    transport_info trans_info;

    int trans_version;
    // int header_done;
    void *data;
} client_t;

typedef struct {
    char *sessionid;
    client_t *client;
    GQueue *queue;
    char *endpoint;
    int state; /* -2:disconnected; -1:disconnecting; 0:connecting; 1:connected; */
    ev_timer close_timeout;
} session_t;

enum SESSION_STATE{
    DISCONNECTED_STATE = -2,
    DISCONNECTING_STATE = -1,
    CONNECTING_STATE = 0,
    CONNECTED_STATE = 1
};

typedef struct {
    char    *transports;
    int     heartbeat_timeout;
    int     close_timeout;
    int     server_close_timeout;
    int     heartbeat_interval;
    int     enable_flash_policy;
    int     flash_policy_port;
    char    *static_path;
} config;

typedef struct {
    char name[20];
    char heartbeat[4];
    void (*output_callback)(session_t *session);
    void (*output_header)(client_t *client);
    void (*output_body)(client_t *client, char *http_msg);
    void (*output_whole)(client_t *client, char *output_msg);
    void (*timeout_callback)(ev_timer *timer);
    void (*heartbeat_callback)(client_t *client, char *sessionid);

    void (*init_connect)(client_t *client, char *sessionid);
    void (*end_connect)(char *sessionid);
} transports_fn;

void init_config();
void store_init();
void transports_fn_init();
void endpoints_init();

int on_url_cb(http_parser *parser, const char *at, size_t length);
int on_body_cb(http_parser *parser, const char *at, size_t length);

void timeout_cb(EV_P_ struct ev_timer *timer, int revents);
void on_close(client_t *client);

#endif