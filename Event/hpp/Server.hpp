#ifndef SERVER_HPP
#define SERVER_HPP

#include "Event/hpp/EpollManager.hpp"
#include "Event/hpp/Client.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "HTTP/hpp/ResponseBuilder.hpp"
#include "HTTP/hpp/HTTPResponse.hpp"
#include "HTTP/hpp/RequestFactory.hpp"
#include "HTTP/hpp/Session.hpp"
#include "Config/hpp/Routing.hpp"
#include "Config/hpp/EffectiveConfig.hpp"
#include "Config/hpp/ServerConfig.hpp"
#include "Method_Handle/hpp/FileUtils.hpp"
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <exception>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>
#include <string>
#include <signal.h>
struct Client;
class Epoller;
class ResponseBuilder;
class ClientManager;
class HTTPResponse;
class Session_manager;

class Server
{
public:
    Server(int port);
    ~Server();
    bool init_sockets();
    void run();
    static void set_non_block_fd(int fd);
    bool handle_connection();

    void handle_pipe_error(int fd);
    void handle_socket_error(int fd);

    void handle_cgi_read(Client &c, int pipe_fd);
    // void handle_cgi_read_error(Client &c, int pipe_fd);

    bool do_read(Client &c);
    bool do_write(Client &c);

    // check timeout
    void close_client(int fd);

    void check_timeout();
    void check_cgi_timeout();

    HTTPResponse process_request(const HTTPRequest &req);

    void cleanup();
    // void process_request(Client &c);

    bool buildRespForCompletedReq(Client &c, int fd);
    bool load_config(const std::string &path);
    void finalize_cgi_response(Client &c, int pipe_fd);
    static void signal_handler(int sig);

private:
    // class de tous les configuration de server
    int port_nbr;
    int socketfd;
    static volatile sig_atomic_t g_running;
    Epoller *_epoller;
    ClientManager *_manager;
    Session_manager *_session_cookie;

    std::vector<ServerRuntimeConfig> _rt_servers;
    Routing *_routing;
    EffectiveConfig _default_cfg;
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