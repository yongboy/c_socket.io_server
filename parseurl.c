#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "socket_io.h"
#include "endpoint.h"

static gchar *transport_url_reg = "^/[^/]*/\\d{1}/([^/]*)/([^?|#]*).*?(i=(\\d+))?$";
static gchar *transport_message_reg = "(\\d):(\\d+\\+?)?:(/[^:]*)?:?(.*)?";
static gchar *get_match_result(GMatchInfo *match_info, gint index);

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
            free(trans_info->i);
            trans_info->i = new_val;
        }
    } else {
        log_error("trans_info is NULL!!!!!!!!!");
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

static gchar *get_match_result(GMatchInfo *match_info, gint index) {
    gchar *match = g_match_info_fetch(match_info, index);
    gchar *result = g_strdup(match);
    free(match);

    return result;
}