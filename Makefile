webserver: socket.io_server.c http-parser/http_parser.o
	gcc -o socket.io_server socket.io_server.c socket_io.c handle_request.c parseurl.c handle_static.c transports.c xhr-polling.c jsonp-polling.c htmlfile.c websocket.c flashsocket.c store.c endpoints.c client_session.c chat_demo.c ./include/c-websocket/*.c ./include/libev.a ./include/http-parser/http_parser.o `pkg-config --cflags --libs glib-2.0` -lrt -lm -luuid -g

http-parser/http_parser.o:
	make -C include/http-parser http_parser.o

clean:
	make -C include/http-parser clean
	rm socket.io_server