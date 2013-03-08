#include "../socket.io_server.h"

int main(int argc, char const *argv[]) {
    server_init();

    server_register_endpoint(init_chat_endpoint_implement("/chat"));
    server_register_endpoint(init_whiteboard_endpoint_implement("/whiteboard"));
    
    int port = 8000;
    server_run(port);

    return 0;
}