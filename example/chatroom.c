#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "../endpoint_impl.h"

#define MAX_BUFF_SIZE 10240

typedef struct {
    char *event_name;
    char *event_args;
} event_message;

static char *event_message_reg = "\\{\\\\?\"name\\\\?\":\\\\?\"(.*?)\\\\?\",\\\\?\"args\\\\?\":\\[\\\\?\"(.*?)\\\\?\"\\]\\}";

static gchar *get_match_result(GMatchInfo *match_info, gint index) {
    gchar *match = g_match_info_fetch(match_info, index);
    gchar *result = g_strdup(match);
    free(match);

    return result;
}

static void *message_2_struct(gchar *post_string, event_message *event_msg) {
    GError *error = NULL;
    GRegex *regex;
    GMatchInfo *match_info;

    regex = g_regex_new(event_message_reg, 0, 0, &error );
    g_regex_match( regex, post_string, 0, &match_info );

    if (g_match_info_matches(match_info)) {
        event_msg->event_name =    get_match_result(match_info, 1);
        event_msg->event_args =    get_match_result(match_info, 2);
    } else {
        event_msg = NULL;
    }

    g_match_info_free( match_info );
    g_regex_unref( regex );

    return event_msg;
}

static GHashTable *session_nickname_hash;

static void session_nickname_init(void) {
    session_nickname_hash = g_hash_table_new(g_str_hash, g_str_equal);
}

static void session_nickname_add(const char *key, const char *value) {
    g_hash_table_insert(session_nickname_hash, g_strdup(key), g_strdup(value));
}

static gboolean session_nickname_remove(const char *key) {
    return g_hash_table_remove(session_nickname_hash, key);
}

static char  *session_nickname_lookup(const char *key) {
    return (char *)g_hash_table_lookup(session_nickname_hash, key);
}

static gint session_nickname_size(void) {
    return g_hash_table_size(session_nickname_hash);
}

static void session_nickname_destroy(void) {
    g_hash_table_destroy(session_nickname_hash);
}

static void broadcast_room(gpointer except_sessionid, gpointer msg) {
    GList *list = g_hash_table_get_keys(session_nickname_hash);
    GList *it = NULL;
    for (it = list; it; it = it->next) {
        gpointer session_id = it->data;
        if (session_id == NULL)
            continue;

        if (except_sessionid != NULL && strcmp(except_sessionid, session_id) == 0) {
            continue;
        }

        send_msg(session_id, msg);
    }

    g_list_free(list);
}

/**
** use the struct to warpper the demo implment
**/
static char *endpoint_name;
static void on_init(const char *endpoint) {
    session_nickname_init();
    printf("%s has been inited now\n", endpoint);
    endpoint_name = g_strdup(endpoint);
}

static void on_connect(const char *sessionid) {
    printf("%s has connect now\n", sessionid);
}

static void on_event(const char *sessionid, const message_fields *msg_fields) {
    if (msg_fields->message_id) {
        char messages[strlen(msg_fields->ori_data)];
        sprintf(messages, "6::%s:%s[false]", msg_fields->endpoint, msg_fields->message_id);
        // "6::/chat:1+[false]"
        send_msg(sessionid, messages);
    }

    event_message event_msg;
    if (!message_2_struct(msg_fields->message_data, &event_msg)) {
        fprintf(stderr, "Parse Message Error !\n");
        return;
    }
    if (!strcmp(event_msg.event_name, "nickname")) {
        char connect_msg[MAX_BUFF_SIZE] = "";
        sprintf(connect_msg, "%s::%s:{\"name\":\"announcement\",\"args\":[\"%s connected\"]}", msg_fields->message_type, msg_fields->endpoint, event_msg.event_args);
        broadcast_room(NULL, connect_msg);

        // add nickname into queue
        session_nickname_add(sessionid, event_msg.event_args);

        char nicknames_list[MAX_BUFF_SIZE] = "";
        // foreach all nicknames
        GList *list = g_hash_table_get_values(session_nickname_hash);
        GList *it = NULL;
        for (it = list; it; it = it->next) {
            char *nickname = it->data;
            if (nickname == NULL)
                continue;

            char double_nicknames[strlen(nickname) * 2 + 7];
            sprintf(double_nicknames, "\"%s\":\"%s\",", nickname, nickname);

            strcat(nicknames_list, double_nicknames);
        }

        g_list_free(list);

        if (strlen(nicknames_list) > 0) {
            nicknames_list[strlen(nicknames_list) - 1] = '\0';
        }

        char nicknames_msg[MAX_BUFF_SIZE] = "";
        sprintf(nicknames_msg, "%s::%s:{\"name\":\"nicknames\",\"args\":[{%s}]}", msg_fields->message_type, msg_fields->endpoint, nicknames_list);
        broadcast_room(NULL, nicknames_msg);
    } else if (!strcmp(event_msg.event_name, "user message") || !strcmp(event_msg.event_name, "user+message")) {
        char user_msg[MAX_BUFF_SIZE] = "";
        char *nickname_str = session_nickname_lookup(sessionid);
        sprintf(user_msg, "%s::%s:{\"name\":\"user message\",\"args\":[\"%s\",\"%s\"]}", msg_fields->message_type, msg_fields->endpoint, nickname_str, event_msg.event_args);
        broadcast_room(sessionid, user_msg);
    } else {
        fprintf(stderr, "invalid ori_data is %s\n", msg_fields->ori_data);
    }
}

static void on_disconnect(const char *sessionid, const message_fields *msg_fields) {
    printf("%s has been disconnect now\n", sessionid);

    char *dis_username = session_nickname_lookup(sessionid);
    if (!dis_username) {
        return;
    }

    char connect_msg[MAX_BUFF_SIZE] = "";
    sprintf(connect_msg, "%s::%s:{\"name\":\"announcement\",\"args\":[\"%s diconnected\"]}", "5", endpoint_name, dis_username);
    broadcast_room(NULL, connect_msg);

    // add nickname into queue
    session_nickname_remove(sessionid);

    char nicknames_list[MAX_BUFF_SIZE] = "";
    // foreach all nicknames
    GList *list = g_hash_table_get_values(session_nickname_hash);
    GList *it = NULL;
    for (it = list; it; it = it->next) {
        char *nickname = it->data;
        if (nickname == NULL)
            continue;

        char double_nicknames[strlen(nickname) * 2 + 7];
        sprintf(double_nicknames, "\"%s\":\"%s\",", nickname, nickname);

        strcat(nicknames_list, double_nicknames);
    }

    g_list_free(list);

    if (strlen(nicknames_list) > 0) {
        nicknames_list[strlen(nicknames_list) - 1] = '\0';
    }

    char nicknames_msg[MAX_BUFF_SIZE] = "";
    sprintf(nicknames_msg, "%s::%s:{\"name\":\"nicknames\",\"args\":[{%s}]}", "5", endpoint_name, nicknames_list);
    broadcast_room(NULL, nicknames_msg);
}

static void on_destroy(const char *endpoint) {
    session_nickname_destroy();
    printf("%s has been destroy now\n", endpoint);
}

extern endpoint_implement *init_chat_endpoint_implement(char *chat_endpoint_name) {
    return init_default_endpoint_implement(chat_endpoint_name);
}