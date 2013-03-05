//rm regurl;gcc -o regurl parseurl.c ./c-websocket/*.c ./include/libev.a http-parser/http_parser.o `pkg-config --cflags --libs glib-2.0` -lrt -lm -luuid -g;clear;./regurl
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "store.h"
#include "socket_io.h"
#include "endpoint.h"

static gchar *transport_url_reg = "^/[^/]*/\\d{1}/([^/]*)/([^?|#]*).*?(i=(\\d+))?$";
static gchar *transport_message_reg = "(\\d):(\\d+\\+?)?:(/[^:]*)?:?(.*)?";
static gchar *get_match_result(GMatchInfo *match_info, gint index);

// static void test_reg(char *input_string) {
//     GError *error = NULL;
//     GRegex *regex;
//     GMatchInfo *match_info;

//     regex = g_regex_new(transport_url_reg, 0, 0, &error );
//     // handle_error( &error, exit_app );
//     g_regex_match( regex, input_string, 0, &match_info );
//     if ( g_match_info_matches( match_info ) ) {
//         // printf("input_string is %s\n", input_string);
//         printf("1 : %s\n", get_match_result( match_info, 1));
//         printf("2 : %s\n", get_match_result( match_info, 2));
//         printf("3 : %s\n", get_match_result( match_info, 4));
//     } else {
//         printf("trans_info is NULL!!!!!!!!!\n");
//         //trans_info = NULL;
//     }

//     g_match_info_free( match_info );
//     g_regex_unref( regex );
// }

// int main(int argc, char const *argv[]) {
//     char input_string[] =  "/socket.io/1/jsonp-polling/37f62526-7cc6-11e2-946c-000c29957841?t=1361520170921&i=0";
//     test_reg(input_string);

//     test_reg("/socket.io/1/xhr-polling/37f62526-7cc6-11e2-946c-000c2995731?t=1361520170921");
//     test_reg("/socket.io/1/websocket/37f62526-7cc6-11e2-946c-000c29957841");

//     return 0;
// }

int check_match(gchar *input_string, gchar *pattern_string) {
    GError *error = NULL;
    GRegex *regex;
    GMatchInfo *match_info;

    regex = g_regex_new( pattern_string, 0, 0, &error );
    g_regex_match( regex, input_string, 0, &match_info );

    int result = 0;
    if ( g_match_info_matches( match_info ) ) {
        result = 1;
    }

    g_match_info_free( match_info );
    g_regex_unref( regex );

    return result;
}

transport_info *url_2_struct(gchar *input_string, transport_info *trans_info) {
    GError *error = NULL;
    GRegex *regex;
    GMatchInfo *match_info;

    regex = g_regex_new(transport_url_reg, 0, 0, &error );
    // handle_error( &error, exit_app );
    g_regex_match( regex, input_string, 0, &match_info );
    if ( g_match_info_matches( match_info ) ) {
        trans_info->oriurl = input_string;
        trans_info->transport = get_match_result( match_info, 1);
        trans_info->sessionid = get_match_result( match_info, 2);
        trans_info->i = get_match_result( match_info, 3);

        if(trans_info->i && strstr(trans_info->i, "i=")){
            char *new_val = g_strdup(trans_info->i + 2);
            g_free(trans_info->i);
            trans_info->i = new_val;
        }
    } else {
        printf("trans_info is NULL!!!!!!!!!\n");
        trans_info = NULL;
    }

    g_match_info_free( match_info );
    g_regex_unref( regex );

    return trans_info;
}

void *body_2_struct(gchar *post_string, message_fields *msg_fields) {
    GError *error = NULL;
    GRegex *regex;
    GMatchInfo *match_info;

    regex = g_regex_new(transport_message_reg, 0, 0, &error );
    g_regex_match( regex, post_string, 0, &match_info );

    if ( g_match_info_matches( match_info ) ) {
        msg_fields->ori_data = get_match_result(match_info, 0);
        msg_fields->message_type =    get_match_result(match_info, 1);
        msg_fields->message_id =    get_match_result(match_info, 2);
        msg_fields->endpoint =    get_match_result(match_info, 3);
        msg_fields->message_data =    get_match_result(match_info, 4);
    } else {
        msg_fields = NULL;
    }

    g_match_info_free( match_info );
    g_regex_unref( regex );

    return msg_fields;
}

char *format_message(message_fields *msg_fields) {
    char *msg_str = malloc(sizeof(message_fields) + 20);
    if (strlen(msg_fields->message_data) > 0)
        sprintf(msg_str, "%s:%s:%s:%s", msg_fields->message_type, msg_fields->message_id, msg_fields->endpoint, msg_fields->message_data);
    else
        sprintf(msg_str, "%s:%s:%s", msg_fields->message_type, msg_fields->message_id, msg_fields->endpoint);

    return msg_str;
}

static gchar *get_match_result(GMatchInfo *match_info, gint index) {
    gchar *match = g_match_info_fetch(match_info, index);
    gchar *result = g_strdup(match);
    g_free(match);

    return result;
}