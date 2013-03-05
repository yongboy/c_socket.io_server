c-websocket
===========
Copyright (C) 2012
Marcin Kelar (marcin.kelar@gmail.com)

WebSocket implementation for C.

With this package you have access to set of functions that can get your network code to work with WebSockets:

WEBSOCKET_generate_handshake - generate the full response that you must send to client.
WEBSOCKET_set_content - encode your data to WebSocket frame.
WEBSOCKET_get_content - decode received WebSocket frame to char array.
WEBSOCKET_valid_connection - check if received data is WebSocket protocol handshake.
WEBSOCKET_client_version - check client's protocol version.

Currently supported WebSocket versions: 13.