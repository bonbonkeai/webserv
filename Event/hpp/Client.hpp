#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include "HTTP/hpp/HTTPRequest.hpp"
#include "HTTP/hpp/HTTPRequestParser.hpp"
#include "HTTP/hpp/HTTPResponse.hpp"
#include "CGI/hpp/CGIProcess.hpp"
#include "Config/hpp/Routing.hpp"
#include <iostream>
#include <map>
#include <iterator>
#include <exception>
#include <unistd.h>
#include <sys/time.h>

class CGI_Process;
/*表示一个客户端连接：
成员包括：
readBuffer / writeBuffer
HTTPRequestParser 实例
是否 headerComplete / bodyComplete
keepAlive 状态
lastActive 时间戳
与 CGI 交互时的 cgiFd*/
//class CGI_Process;

enum Clientstate
{
    // RD_LINE,//is reading request line
    // RD_HEADER, //is reading request header
    // RD_BODY, //is reading request body
    // RD_DONE, //reading finish
    // 这个是由HTTP parser来管的，不要再Client外层来管
    READING,
    PROCESS, // do the request
    WRITING,
    CLOSED,
    ERROR
};

struct Client
{
    int client_fd;

    Clientstate _state;
    std::string read_buffer; // 仅用于暂存recv;parser会自带_buffer
    HTTPRequestParser parser;

    std::string write_buffer;
    size_t write_pos;
    bool is_keep_alive;

    // cgi
    CGI_Process *_cgi;
    bool is_cgi;

    // timeout
    bool timeout;
    time_t last_active;
    unsigned long long last_activity_ms;
    bool is_timeout(unsigned long long now_ms, unsigned long long timeout_ms) const
    {
        return (now_ms - last_activity_ms) > timeout_ms;
    }

    Client(int fd = -1)
        : client_fd(fd),
          _state(READING),
          read_buffer(),
          parser(),
          write_buffer(),
          write_pos(0),
          is_keep_alive(false),
          _cgi(NULL),
          is_cgi(false),
          last_activity_ms(0)
    {
        read_buffer.reserve(4096);
        last_activity_ms = now_ms();
    }
    void    reset();

    int get_fd()
    {
        return client_fd;
    }

    static unsigned long long now_ms()
    {
        struct timeval tv;
        gettimeofday(&tv, 0);
        return (unsigned long long)tv.tv_sec * 1000ULL + (unsigned long long)tv.tv_usec / 1000ULL;
    }
};

class ClientManager
{
public:
    ClientManager();
    ~ClientManager();

    void add_socket_client(int fd);
    Client *get_socket_client_by_fd(int fd);
    void remove_socket_client(int fd);

    // gestion cgi client
    Client *get_client_by_cgi_fd(int pipe_fd);
    bool is_cgi_pipe(int pipe_fd);
    void del_cgi_fd(int pipe_fd);
    void bind_cgi_fd(int pipe_fd, int client_fd);

    std::map<int, Client *> &get_all_socket_clients()
    {
        return _clients;
    }
    std::map<int, Client *> &get_all_cgi_clients()
    {
        return _cgi_manager;
    }
    void    clear_all_clients();
    void    clear_all_cgi_clients();
private:
    std::map<int, Client *> _clients;     // socket_fd -> client*
    std::map<int, Client *> _cgi_manager; // pipe_fd -> client*
};

#endif

// enum	ClientState
// {
// 	CLIENT_READING,
// 	CLIENT_READY,
// 	CLIENT_WRITING,
// 	CLIENT_CLOSED
// };

// class	Client
// {
// public:
// 		Client();
// 		Client(int fd);
// 		Client(const Client& copy);
// 		Client& operator=(const Client& copy);
// 		~Client();

// 		int getFd() const;
// 		bool isClosed() const;
// 		bool isReady() const;

// 		/*I/O*/
// 		void readBuffer();
// 		void writeBuffer();

// 		/*state control*/
// 		void markClosed();
// 		void restForNextReq() const;

// 		/*access parsed data*/
// 		const HTTPRequest& getRequest() const;

// 		/*set response*/
// 		void setResponse(const HTTPResponse& resq);
// private:
// 		int _clientFd;
// 		ClientState _state;
// 		/*parse http*/
// 		HTTPRequestParser _httpParser;
// 		bool	_reqComplet;
// 		/*response buffer*/
// 		std::string	_respBuffer;
// 		size_t	_respSent;
// };