#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "socket_io.h"
#include "endpoint.h"
#include "safe_mem.h"

#define POLLING_FRAMEING_DELIM "\ufffd"

int handle_body_cb_one(client_t *client, char *post_msg, void (*close_fn)(client_t *client), bool need_close_fn);

int on_body_cb(http_parser *parser, const char *at, size_t length) {
    client_t *client = parser->data;
    char post_msg[(int)length + 1];
    sprintf(post_msg, "%.*s", (int)length, at);
    if (post_msg[0] == 'd') {
        char *unescape_string = g_uri_unescape_string(post_msg, NULL);
        if (unescape_string == NULL) {
            log_warn("unescape_string is NULL! post_msg is %s", post_msg);

            char http_msg[200];
            sprintf(http_msg, RESPONSE_PLAIN, 1, "1");
            write_output(client, http_msg, on_close);

            return 0;
        }

        gchar *result = g_strcompress(unescape_string);
        if (result) {
            char target[strlen(result) - 4];
            strncpy(target, result + 3, strlen(result));
            target[strlen(target) - 1] = '\0';

            memset(post_msg, '\0', (int)length + 1);
            strncpy(post_msg, target, strlen(target));
            free(result);
        }
    }

    //check multiple messages framing, eg: `\ufffd` [message lenth] `\ufffd`
    if (g_str_has_prefix(post_msg, POLLING_FRAMEING_DELIM)) {
        char *str = strtok(post_msg, POLLING_FRAMEING_DELIM);
        bool is_str = false;
        while (str != NULL) {
            if (is_str) {
                if (g_str_has_suffix (str, "]}")) {
                    handle_body_cb_one(client, str, NULL, false);
                } else {
                    log_warn("half package str is %s", str);
                    transport_info *trans_info = &client->trans_info;
                    log_warn("the str is half package with ori url is %s & data %s", trans_info->oriurl, client->data);
                }
            }
            is_str = !is_str;
            str = strtok(NULL, POLLING_FRAMEING_DELIM);
        }

        char http_msg[200];
        sprintf(http_msg, RESPONSE_PLAIN, 1, "1");
        write_output(client, http_msg, on_close);
    } else {
        handle_body_cb(client, post_msg, on_close);
    }

    return 0;
}

int handle_body_cb_one(client_t *client, char *post_msg, void (*close_fn)(client_t *client), bool need_close_fn) {
    transport_info *trans_info = &client->trans_info;
    if (trans_info == NULL) {
        log_warn("handle_body_cb_one's trans_info is NULL!");
        write_output(client, RESPONSE_400, on_close);

        return 0;
    }

    message_fields msg_fields;
    body_2_struct(post_msg, &msg_fields);
    if (msg_fields.endpoint == NULL) {
        log_warn("msg_fields.endpoint is NULL and the post_msg is %s", post_msg);
        /*return 0;*/
    }

    transports_fn *trans_fn = get_transport_fn(client);
    if (trans_fn == NULL) {
        if (need_close_fn) {
            log_warn("handle_body_cb_one's trans_fn is NULL and the post_msg is %s !", post_msg);
            write_output(client, RESPONSE_400, on_close);
        }
        return 0;
    }

    char *sessionid = trans_info->sessionid;
    session_t *session = store_lookup(sessionid);
    if (session == NULL) {
        log_warn("handle_body_cb_one's session is NULL and the post_msg is %s !", post_msg);
        write_output(client, RESPONSE_400, on_close);

        return 0;
    }

    int num = atoi(msg_fields.message_type);

    endpoint_implement *endpoint_impl = endpoints_get(msg_fields.endpoint);
    if (endpoint_impl == NULL && num != 2) { // just for debug invalid request ...
        if (client->data) {
            log_warn("client->date srlen is %d", (int)strlen(client->data));
        }
        if (need_close_fn) {
            log_warn("handle_body_cb_one's endpoint_impl is NULL and the post_msg is %s !", post_msg);
            write_output(client, RESPONSE_400, on_close);
        }
        return 0;
    }

    switch (num) {
    case 0:
        endpoint_impl->on_disconnect(trans_info->sessionid, &msg_fields);
        notice_disconnect(&msg_fields, trans_info->sessionid);
        break;
    case 1:
        if (session->state != CONNECTED_STATE) {
            /*printf("endpoint is %s, sessionid is %s, post_msg is %s\n", msg_fields.endpoint, trans_info->sessionid, post_msg);*/
            notice_connect(&msg_fields, trans_info->sessionid, post_msg);
            endpoint_impl->on_connect(trans_info->sessionid);
        } else {
            log_warn("invalid state is %d endpoint is %s, sessionid is %s, post_msg is %s", session->state, msg_fields.endpoint, trans_info->sessionid, post_msg);
        }
        break;
    case 2:
        trans_fn->heartbeat_callback(client, trans_info->sessionid);
        break;
    case 3:
        endpoint_impl->on_message(trans_info->sessionid, &msg_fields);
        break;
    case 4:
        endpoint_impl->on_json_message(trans_info->sessionid, &msg_fields);
        break;
    case 5:
        endpoint_impl->on_event(trans_info->sessionid, &msg_fields);
        break;
    default:
        endpoint_impl->on_other(trans_info->sessionid, &msg_fields);
        break;
    }

    if (need_close_fn) {
        char http_msg[200];
        if (close_fn) {
            sprintf(http_msg, RESPONSE_PLAIN, 1, "1");
            write_output(client, http_msg, close_fn);
        } else { // just for websocket, output message eg: "5::/chat:1";
            if (msg_fields.endpoint) {
                sprintf(http_msg, "5::%s:1", msg_fields.endpoint);
            } else {
                sprintf(http_msg, "5::%s:1", session->endpoint);
            }

            trans_fn->output_body(client, http_msg);
        }
    }

    return 0;
}

int handle_body_cb(client_t *client, char *post_msg, void (*close_fn)(client_t *client)) {
    return handle_body_cb_one(client, post_msg, close_fn, true);
}