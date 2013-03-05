#ifndef _ENDPOINT_H
#define _ENDPOINT_H

//[message type] ':' [message id ('+')] ':' [message endpoint] (':' [message data]) 
typedef struct{
  char *message_type;
  char *message_id;
  char *endpoint;
  char *message_data;
  char *ori_data;
} message_fields;

typedef struct {
    char *endpoint;
    void (*on_init)(const char *endpoint);
    void (*on_connect)(const char *sessionid, const message_fields *msg_fields);
    void (*on_message)(const char *sessionid, const message_fields *msg_fields);
    void (*on_disconnect)(const char *sessionid, const message_fields *msg_fields);
    void (*on_destroy)(const char *endpoint);
} endpoint_implement;

char *format_message(message_fields *msg_fields);

#endif