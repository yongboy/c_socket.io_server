#include "../socket.io_server.h"

int main(int argc, char const *argv[]) {
    server_init();

    server_register_endpoint(init_chat_endpoint_implement("/chat"));
    
    int port = 8000;
    server_run(port);

    return 0;
}