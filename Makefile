CPPFLAGS = socket_io.c socket.io_handle_request.c socket.io_handle_post.c socket.io_base.c parseurl.c handle_static.c \
		transports.c xhr-polling.c jsonp-polling.c htmlfile.c websocket.c flashsocket.c store.c endpoints.c client_session.c \
		 ./include/c-websocket/*.c ./include/libev.a ./include/http-parser/http_parser.o \
		  `pkg-config --cflags --libs glib-2.0` -lrt -lm -luuid -Wall \
		   -g -rdynamic

example: http-parser/http_parser.o
	gcc -o socket.io_server example/socket.io_server.c example/chatroom.c example/whiteboard.c $(CPPFLAGS)
	#gcc -o socket.io_server -DMEMWATCH -DMEMWATCH_STDIO example/socket.io_server.c example/chat_demo.c example/whiteboard.c memwatch/memwatch.c $(CPPFLAGS)

http-parser/http_parser.o:
	make -C include/http-parser http_parser.o

clean:
	make -C include/http-parser clean
	rm socket.io_server