#ifndef _ENDPOINT_IMPL_H
#define _ENDPOINT_IMPL_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// extend the endpoint.h
#include "endpoint.h"

static void on_init(const char *endpoint);
static void on_connect(const char *sessionid);
static void on_message(const char *sessionid, const message_fields *msg_fields) {
    printf("on_message recevie ori msg is %s\n", msg_fields->ori_data);
}
static void on_json_message(const char *sessionid, const message_fields *msg_fields) {
    printf("on_json_message recevie ori msg is %s\n", msg_fields->ori_data);
}
static void on_other(const char *sessionid, const message_fields *msg_fields) {
    printf("on_other recevie ori msg is %s\n", msg_fields->ori_data);
}
static void on_event(const char *sessionid, const message_fields *msg_fields);
static void on_disconnect(const char *sessionid, const message_fields *msg_fields);
static void on_destroy(const char *endpoint);

static endpoint_implement *init_default_endpoint_implement(char *endpoint_name) {
    endpoint_implement *impl_point = (endpoint_implement *)malloc(sizeof(endpoint_implement));
    impl_point->endpoint = strdup(endpoint_name);

    impl_point->on_init = on_init;
    impl_point->on_connect = on_connect;
    impl_point->on_message = on_message;
    impl_point->on_json_message = on_json_message;
    impl_point->on_event = on_event;
    impl_point->on_other = on_other;
    impl_point->on_disconnect = on_disconnect;
    impl_point->on_destroy = on_destroy;

    return impl_point;
}

#endif