#ifndef SERVER_HPP
#define SERVER_HPP

#include "Event/hpp/EpollManager.hpp"
#include "Event/hpp/Client.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/ResponseBuilder.hpp"
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <exception>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

class Epoller;
class ResponseBuilder;
class ClientManager;
class Server
{
public:
    Server(int port);
    ~Server();
    bool init_sockets();
    void run();
    void set_non_block_fd(int fd);
    bool handle_connection();

    void handle_pipe_error(int fd);
    void handle_socket_error(int fd);
    void handle_error_event(int fd);

    void handle_cgi_read(Client &c, int pipe_fd);
    void handle_cgi_read_error(Client &c, int pipe_fd);

    bool do_read(Client &c);
    bool do_write(Client &c);

private:
    // class de tous les configuration de server
    int port_nbr;
    int socketfd;
    Epoller _epoller;
    ClientManager _manager;
};

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