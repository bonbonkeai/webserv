#ifndef NEW_SERVER_HPP
#define NEW_SERVER_HPP

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
// #include "Method_Handle/hpp/CGIRequestHandle.hpp"
#include "CGI/hpp/CGIManager.hpp"

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

#include <vector>
#include <map>

class Epoller;
class ClientManager;
class Session_manager;

class Server
{
public:
    Server(int port);
    ~Server();

    bool init_sockets();
    void run();

    static void set_non_block_fd(int fd);

    // bool handle_connection();
    bool handle_connection_on(int listen_fd, int port);
    void handle_socket_error(int fd);
    void close_client(int fd);

    bool do_read(Client &c);
    bool do_write(Client &c);

    HTTPResponse process_request(const HTTPRequest &req);
    bool buildRespForCompletedReq(Client &c, int fd);

    // run helpers
    void run_init_listeners();
    void run_handle_event(int fd, uint32_t ev);
    void run_handle_read(Client &c, int fd);
    void run_handle_write(Client &c, int fd);
    void run_process_keep_alive_pipeline(Client &c, int fd);

    // timeouts
    void check_timeout();
    void check_cgi_timeout();

    bool load_config(const std::string &path);

    void cleanup();
    static void signal_handler(int sig);

    //
    std::vector<std::string> getListenAddresses() const;
    void printStartupInfo(const std::string &configPath) const;

private:
    int port_nbr;
    int socketfd;

    // multi_server
    std::vector<int> _listen_fds;
    std::map<int, int> _fd_to_port;

    //
    std::map<int, std::string> _fd_to_host;
    //
    static volatile sig_atomic_t g_running;

    Epoller *_epoller;
    ClientManager *_manager;

    Session_manager *_session_cookie;

    // routing/config
    std::vector<ServerRuntimeConfig> _rt_servers;
    Routing *_routing;
    EffectiveConfig _default_cfg;

    // CGI manager + fd dispatch tables
    CGIManager _cgi_manager;

    void start_cgi_for_client(Client *c, const HTTPRequest &req);
    void handle_cgi_event(int fd, uint32_t ev);
    void finish_cgi_process(CGI_Process *proc);
    void cleanup_client_cgi(Client *c);
    void valide_server_names();

    // void finalize_cgi_response(Client& c, int pipe_fd);
};

#endif