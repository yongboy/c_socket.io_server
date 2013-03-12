#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "../endpoint_impl.h"

typedef struct {
    char *event_name;
    char *event_args;
} event_message;

static char *event_message_reg = "{\"name\":\"(.*?)\",\"args\":\\[([^\\]]*?)\\]}";

static gchar *get_match_result(GMatchInfo *match_info, gint index) {
    gchar *match = g_match_info_fetch(match_info, index);
    gchar *result = g_strdup(match);
    g_free(match);

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

static GHashTable *hashtable;

static void hashtable_init(void) {
    hashtable = g_hash_table_new(g_str_hash, g_str_equal);
}

static void hashtable_add(const char *key, void *value) {
    g_hash_table_insert(hashtable, g_strdup(key), value);
}

static gboolean hashtable_remove(const char *key) {
    return g_hash_table_remove(hashtable, key);
}

static void *hashtable_lookup(const char *key) {
    return g_hash_table_lookup(hashtable, key);
}

/*static gint hashtable_size(void) {
    return g_hash_table_size(hashtable);
}*/

static void hashtable_destroy(void) {
    g_hash_table_destroy(hashtable);
}

/**
** use the struct to warpper the demo implment
**/
static char *endpoint_name;
static void on_init(const char *endpoint) {
    hashtable_init();
    printf("%s has been inited now\n", endpoint);
    endpoint_name = g_strdup(endpoint);
}

static void on_connect(const char *sessionid) {
    char messages[strlen(sessionid) + 50];
    sprintf(messages, "5::%s:{\"name\":\"clientId\",\"args\":[{\"id\":\"%s\"}]}", endpoint_name, sessionid);

    send_msg(sessionid, messages);
}

static void send_it(char *session_id, char *messaage) {
    send_msg(session_id, messaage);
}

static void on_event(const char *sessionid, const message_fields *msg_fields) {
    event_message event_msg;
    if (!message_2_struct(msg_fields->message_data, &event_msg)) {
        fprintf(stderr, "%s Parse Message Error !\n", msg_fields->ori_data);
        return;
    }

    if (!strcmp(event_msg.event_name, "roomNotice")) {
        char target_room_id[] = "myRoom";
        GPtrArray *list = (GPtrArray *)hashtable_lookup(target_room_id);
        if (list == NULL) {
            list = g_ptr_array_new();
            hashtable_add(target_room_id, list);
        }
        g_ptr_array_add(list, g_strdup(sessionid));

        int room_count = list->len;
        char messages[strlen(sessionid) + 200];
        sprintf(messages, "5::%s:{\"name\":\"roomCount\",\"args\":[{\"room\":\"%s\",\"num\":%d}]}", endpoint_name, target_room_id, room_count);

        hashtable_add(g_strdup(sessionid), g_strdup(target_room_id));
        g_ptr_array_foreach(list, (GFunc)send_it, messages);
        return;
    }

    char messages[strlen(msg_fields->ori_data) + 200];
    sprintf(messages, "5::%s:{\"name\":\"%s\",\"args\":[%s]}", endpoint_name, event_msg.event_name, event_msg.event_args);
    char *target_room_id = (char *)hashtable_lookup(sessionid);
    GPtrArray *list = (GPtrArray *)hashtable_lookup(target_room_id);
    int i;
    for (i = 0; i < list->len; i++) {
        char *session_id = g_ptr_array_index(list, i);
        if (strcmp(session_id, sessionid) == 0)
            continue;

        send_msg(session_id, messages);
    }
}

static void on_disconnect(const char *sessionid, const message_fields *msg_fields) {
    char *room_id = (char *)hashtable_lookup(sessionid);
    if (room_id == NULL) {
        fprintf(stderr, "the room_id is NULL\n");
        return;
    }

    char notice_msg[strlen(endpoint_name) + strlen(room_id) + 70];
    GPtrArray *list = (GPtrArray *)hashtable_lookup(room_id);
    sprintf(notice_msg, "5::%s:{\"name\":\"roomCount\",\"args\":[{\"room\":\"%s\",\"num\":%d}]}", endpoint_name, room_id, list->len - 1);
    int i, remove_index;
    for (i = 0; i < list->len; i++) {
        char *session_id = g_ptr_array_index(list, i);
        if (strcmp(session_id, sessionid) == 0) {
            remove_index = i;
            continue;
        }

        send_msg(session_id, notice_msg);
    }
    g_ptr_array_remove_index(list, remove_index);

    hashtable_remove(sessionid);
    free(room_id);
}

static void on_destroy(const char *endpoint) {
    printf("%s has been destroy now\n", endpoint);
    hashtable_destroy();
    free(endpoint_name);
}

extern endpoint_implement *init_whiteboard_endpoint_implement(char *endpoint_name) {
    return init_default_endpoint_implement(endpoint_name);
}