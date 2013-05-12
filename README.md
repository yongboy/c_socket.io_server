#The C socket.io server
------

##NOTES
The socket.io Linux C server, now just in developing at moment.  
The server base on [libev](http://libev.schmorp.de/) and [glib](https://developer.gnome.org/glib/2.34/), and run in linux systems now.  
Before run the socket.io server, you have to install some dependencies before:  
> sudo apt-get install uuid-dev  
> sudo apt-get install libglib2.0-dev   
> git clone https://github.com/HardySimpson/zlog *then* make & make install it

The socket.io server may contains some hidden bugs, if you find it, please notice me :))

##How to use
1. write you implemetion, eg:chatroom.c
2. put your implemetion code(chatroom.c) in the example folder
3. create your static html files put them into static folder
3. edit the **Makefile** file
4. open the console, type #make
5. type ./socket.io_server
6. access your webpages in browser now, eg: http://localhost:8000/chatroom.html
7. enjoy it~

##The API NOTES
You have include the head file **endpoint_impl.h** within your code,which extends **endpoint.h**, the whole interface define, you can find [here](https://gist.github.com/yongboy/5168005).   
There are two demos in example folder, [chatroom](example/chatroom.c) and [whiteboard](example/whiteboard.c) examples. 

------
##说明
这是一个纯C语言版本的socket.io服务器端实现，目前仅支持linux系统，严重依赖[libev](http://libev.schmorp.de/) and [glib](https://developer.gnome.org/glib/2.34/)等基础库。  
在运行socket.io_server之前，需要安装以下依赖：   
> sudo apt-get install uuid-dev  
> sudo apt-get install libglib2.0-dev    
> git clone https://github.com/HardySimpson/zlog *then* make & make install it

##如何运行
1. 编写实现代码（eg:chatroom.c），需要包含头文件 **endpoint_impl.h**
2. 把实现代码(eg:chatroom.c)放入examples目录
3. 编写对应的html文件，放入static目录
4. 编辑**Makefile**文件
5. 终端下运行make命令
6. 然后敲入 ./socket.io_server 接口运行
7. 打开浏览器即可访问 (eg:http://localhost:8000/chatroom.html)

##API说明
对外的API，可以在头文件 **endpoint_impl.h** 看到其定义，其继承了另外一个公用的头文件 **endpoint.h**, 完整的面向实现代码的头文件定义见 [完整定义](https://gist.github.com/yongboy/5168005).   
在example目录中，你可以看到聊天室演示 [chatroom](example/chatroom.c) 和在线白板示范 [whiteboard](example/whiteboard.c) . 
