#ifndef SERVER_HPP
#define SERVER_HPP




/*初始化 listen sockets (根据 ServerConfig)
设置为 non-blocking
将监听 fd 放入 PollManager
创建与管理 Client 对象
主循环：
while (running):
    poll()
    if listening fd readable → accept
    if client fd readable → recv
    if client fd writable → send
    if CGI pipe readable → read CGI output*/


    
#endif