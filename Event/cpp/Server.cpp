#include "Event/hpp/Server.hpp"
#include <cstring>
#include <sys/wait.h>

#define Timeout 50 // 50-100
#define ALL_TIMEOUT 5
#define CGI_TIMEOUT 5

// 这里端口-1，然后 htons(port_nbr)。这可能会导致 bind 失败（或者绑定到不可预期端口），服务器根本起不来。或许可以先给一个固定值（比如 8080）？
Server::Server(int port) : port_nbr(port), socketfd(-1)
{
}
Server::~Server()
{
}

bool Server::init_sockets()
{
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0)
        throw std::runtime_error("Socket create failed");
    // struct sockaddr_in serveraddr{};//98不让这样写
    struct sockaddr_in serveraddr;
    std::memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port_nbr);
    serveraddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        throw std::runtime_error("Socket bind failed");
    if (listen(socketfd, 256) < 0)
        throw std::runtime_error("Listen socket failed");
    return _epoller.init(128);
}

void Server::set_non_block_fd(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl get flags failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl set flags failed");
}

bool Server::handle_connection()
{
    while (true)
    {
        struct sockaddr_in clientaddr;
        socklen_t client_len = sizeof(clientaddr);
        int connect_fd = accept(socketfd, (struct sockaddr *)&clientaddr, &client_len);
        if (connect_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return true;
            return false;
        }
        set_non_block_fd(connect_fd);
        _epoller.add_event(connect_fd, EPOLLIN | EPOLLET);
        _manager.add_socket_client(connect_fd); // client state is read_line
    }
    return true;
}

// 暂时还不做业务 handle，所以 process_request 的最小实现就返回一个固定 body，同时把 keep-alive 从 request 带过来。
//  static HTTPResponse process_request(const HTTPRequest& req)
//  {
//      if (req.bad_request)
//          return buildErrorResponse(400);

//     HTTPResponse resp;
//     resp.statusCode = 200;
//     resp.statusText = "OK";
//     resp.body = "OK\n";
//     resp.headers["content-type"] = "text/plain; charset=utf-8";
//     resp.headers["content-length"] = toString(resp.body.size());
//     resp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
//     return (resp);
// }

// static HTTPResponse process_request(const HTTPRequest &req)
//{
//     IRequest *h = RequestFactory::create(req);
//     HTTPResponse resp = h->handle();
//     delete h;
//     return (resp);
// }

bool Server::do_read(Client &c)
{
    char tmp[4096];
    // bool is_reading = true;

    while (true)
    {
        ssize_t n = recv(c.get_fd(), tmp, sizeof(tmp), 0);
        if (n > 0)
        {

            bool ok = c.parser.dejaParse(std::string(tmp, n));
            if (!ok && c.parser.getRequest().bad_request)
            {
                c._state = ERROR;
                // 生成错误响应//
                HTTPResponse err = buildErrorResponse(400); // 暂时先放400，后续可以根据具体的error_code来修改
                // HTTPResponse err = buildErrorResponse(c.parser.getRequest().error_code);
                c.is_keep_alive = false;
                c.write_buffer = ResponseBuilder::build(err);
                c.write_pos = 0;
                c._state = WRITING;
                return (true); // 让上层切到 WRITING
            }
        }
        else if (n == 0)
        {

            c._state = CLOSED;
            return (false);
        }
        else
        {

            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            c._state = ERROR;
            return (false);
        }
    }
    if (c.parser.getRequest().complet)
    {

        c._state = PROCESS;
        return (true);
    }
    return (false);
}
void Server::handle_cgi_read_error(Client &c, int pipe_fd)
{
    _epoller.del_event(pipe_fd);
    _manager.del_cgi_fd(pipe_fd);

    kill(c._cgi->_pid, SIGKILL);
    waitpid(c._cgi->_pid, NULL, 0);
    c._cgi->reset();

    HTTPResponse err = buildErrorResponse(500);
    c.write_buffer = ResponseBuilder::build(err);
    c.write_pos = 0;
    c._state = WRITING;
    c.is_keep_alive = false;
    _epoller.modif_event(pipe_fd, EPOLLOUT | EPOLLET);
}

void Server::handle_cgi_read(Client &c, int pipe_fd)
{
    char buf[4096];
    while (true)
    {
        ssize_t n = read(pipe_fd, buf, sizeof(buf));
        if (n > 0)
            c._cgi->append_output(buf, n);
        else if (n == 0) // cgi finish
        {
            _epoller.del_event(pipe_fd);
            _manager.del_cgi_fd(pipe_fd);
            waitpid(c._cgi->_pid, NULL, WNOHANG);

            // client prepare la reponse
            HTTPResponse resp;
            resp = resp.buildResponseFromCGIOutput(c._cgi->_output_buffer,
                                                   c.parser.getRequest().keep_alive);
            c.write_buffer = ResponseBuilder::build(resp);
            c.write_pos = 0;

            // c._cgi->reset();
            c.is_cgi = false;

            c._state = WRITING;
            _epoller.modif_event(c.client_fd, EPOLLOUT | EPOLLET);
            break;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            handle_cgi_read_error(c, pipe_fd);
            break;
        }
    }
}

void Server::handle_pipe_error(int fd)
{
    Client *c = _manager.get_client_by_cgi_fd(fd);

    if (!c)
        return;
    _epoller.del_event(fd);
    _manager.del_cgi_fd(fd);

    if (c->_cgi->_pid > 0)
    {
        kill(c->_cgi->_pid, SIGKILL);
        waitpid(c->_cgi->_pid, NULL, WNOHANG);
    }
    c->_cgi->reset();
    HTTPResponse err = buildErrorResponse(500);
    c->write_buffer = ResponseBuilder::build(err);
    c->write_pos = 0;
    c->_state = WRITING;
    c->is_keep_alive = false;

    _epoller.modif_event(c->client_fd, EPOLLOUT | EPOLLET);
}

void Server::handle_socket_error(int fd)
{
    Client *c = _manager.get_socket_client_by_fd(fd);

    if (!c)
        return;
    if (c->is_cgi && c->_cgi->_pid > 0)
    {
        kill(c->_cgi->_pid, SIGKILL);
        waitpid(c->_cgi->_pid, NULL, 0);
        int cgi_fd = c->_cgi->_read_fd;
        if (cgi_fd >= 0)
        {
            _epoller.del_event(cgi_fd);
            _manager.del_cgi_fd(cgi_fd);
        }
    }
    _epoller.del_event(fd);
    _manager.remove_socket_client(fd);
    close(fd);
}

void Server::handle_error_event(int fd)
{
    if (_manager.is_cgi_pipe(fd))
        handle_pipe_error(fd);
    else
        handle_socket_error(fd);
}

void Server::close_client(int fd)
{
    _manager.remove_socket_client(fd);
    _epoller.del_event(fd);
    close(fd);
}

void Server::check_cgi_timeout()
{
    time_t now = std::time(0);
    std::vector<int> to_close;

    std::map<int, Client *> &cgi_clients = _manager.get_all_cgi_clients();
    for (std::map<int, Client *>::iterator it = cgi_clients.begin(); it != cgi_clients.end(); ++it)
    {
        Client *c = it->second;
        CGI_Process *cgi = c->_cgi;

        if (!cgi || !c->is_cgi)
            continue;
        if ((now - c->_cgi->start_time) > CGI_TIMEOUT)
        {
            std::cout << "TIMEOUT: " << c->client_fd << "|is_cgi: " << c->is_cgi << std::endl;
            to_close.push_back(cgi->_read_fd);
            if (cgi->_pid > 0)
            {
                //可能需要一个reponse的输出
                kill(cgi->_pid, SIGKILL);
                waitpid(cgi->_pid, NULL, 0);
            }
            if (cgi->_read_fd >= 0)
                close(cgi->_read_fd);
            if (cgi->_write_fd >= 0)
                close(cgi->_write_fd);
            cgi->reset();
            c->is_cgi = false;

            HTTPResponse err = buildErrorResponse(504); // Gateway Timeout
            c->write_buffer = ResponseBuilder::build(err);
            c->write_pos = 0;
            c->_state = WRITING;
            c->is_keep_alive = false;

            // 修改客户端事件为可写
            _epoller.modif_event(c->client_fd, EPOLLOUT | EPOLLET);
            to_close.push_back(it->first);
        }
    }

    for (size_t i = 0; i < to_close.size(); i++)
    {
        _epoller.del_event(to_close[i]);
        _manager.del_cgi_fd(to_close[i]);
    }
}

void Server::check_timeout()
{
    time_t now = std::time(0);

    std::vector<int> to_close;
    std::map<int, Client *> &clients = _manager.get_all_socket_clients();
    for (std::map<int, Client *>::iterator it = clients.begin(); it != clients.end(); it++)
    {
        Client *c = it->second;
        if (c->is_timeout(now, ALL_TIMEOUT) && !c->is_cgi)
        {
            std::cout << "TIMEOUT: " << c->client_fd << "|is_cgi: " << c->is_cgi << std::endl;
            to_close.push_back(c->client_fd);
        }
    }
    for (size_t i = 0; i < to_close.size(); i++)
        close_client(to_close[i]);
}

/**
 * run 中epoller的event的fd进行检查
 *  1. 此fd是socketfd ->handle connection
 *  2. 此fd是cgi pipe的output
 *  3. 此fd是client socket ->
 *  4. 什么也不是，error处理
 *  5. error处理当中，需要对fd的来源进行检查，如果是cgi那边的问题，需要kill结束进程
 */
void Server::run()
{
    set_non_block_fd(socketfd);
    _epoller.add_event(socketfd, EPOLLIN | EPOLLET);
    while (true)
    {
        int nfds = _epoller.wait(Timeout);
        check_cgi_timeout();
        check_timeout();
        for (int i = 0; i < nfds; i++)
        {
            int fd = _epoller.get_event_fd(i);
            uint32_t events = _epoller.get_event_type(i);
            if (fd == socketfd)
            {
                handle_connection();
                continue;
            }
            // if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            //{
            //     handle_error_event(fd);
            //     continue;
            // }

            if (_manager.is_cgi_pipe(fd))
            {
                Client *c = _manager.get_client_by_cgi_fd(fd);
                if (c && (events & EPOLLIN))
                    handle_cgi_read(*c, fd);
                continue;
            }
            Client *c = _manager.get_socket_client_by_fd(fd);
            if (!c)
                continue;
            if (events & EPOLLIN)
            {
                if (do_read(*c))
                {
                    c->last_active = std::time(0); // refresh for timeout
                    c->_state = PROCESS;
                    process_request(*c);
                    if (!c->is_cgi)
                    {
                        _epoller.modif_event(fd, EPOLLOUT | EPOLLET);
                    }
                }
            }
            if (events & EPOLLOUT)
            {

                if (do_write(*c))
                {
                    c->last_active = std::time(0);

                    if (c->is_keep_alive)
                    {
                        c->reset();
                        _epoller.modif_event(fd, EPOLLIN | EPOLLET);
                    }
                    else
                    {
                        _manager.remove_socket_client(fd);
                        _epoller.del_event(fd);
                        close(fd);
                    }
                }
            }
        }
    }
}

// void Server::run()
// {
//     set_non_block_fd(socketfd);
//     _epoller.add_event(socketfd, EPOLLIN | EPOLLET);
//     while (true)
//     {
//         int nfds = _epoller.wait(Timeout);
//         for (int i = 0; i < nfds; i++)
//         {
//             int fd = _epoller.get_event_fd(i);
//             uint32_t events = _epoller.get_event_type(i);
//             if (fd == socketfd)
//             {
//                 handle_connection();
//                 continue;
//             }
//             //这里还不确定，因为可能还存在同一轮里你先读/写了，再关
//             if (fd != socketfd && (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)))
//             {
//                 _epoller.del_event(fd);
//                 _manager.remove_socket_client(fd);
//                 close(fd);
//                 continue;
//             }
//             // else if (events & EPOLLIN) // read fd
//             // {
//             //     Client &c = _manager.get_socket_client_by_fd(fd);
//             //     if (do_read(c))
//             //     {
//             //         c._state = PROCESS;
//             //         process_request(c);
//             //         _epoller.modif_event(fd, EPOLLOUT | EPOLLET);
//             //     }
//             // }
//             // else if (events & EPOLLOUT) // write fd
//             // {
//             //     Client &c = _manager.get_socket_client_by_fd(fd);
//             //     c._state = WRITING;
//             //     if (do_write(c))
//             //     {
//             //         if (c.is_keep_alive)
//             //         {
//             //             c.reset();
//             //             _epoller.modif_event(fd, EPOLLIN | EPOLLET);
//             //         }
//             //         else
//             //         {
//             //             _manager.remove_socket_client(fd);
//             //             _epoller.del_event(fd);
//             //             close(fd);
//             //         }
//             //     }
//             // }
//             //用两个if,因为同一轮 events 可能同时包含 IN 和 OUT，用 else if会漏处理其中一个
//             if (events & EPOLLIN)
//             {
//                 Client &c = _manager.get_socket_client_by_fd(fd);
//                 if (do_read(c))
//                 {
//                     if (c._state == PROCESS)
//                     {
//                         HTTPResponse resp = process_request(c.parser.getRequest());
//                         c.is_keep_alive = c.parser.getRequest().keep_alive;
//                         c.write_buffer = ResponseBuilder::build(resp);
//                         c.write_pos = 0;
//                     }
//                     c._state = WRITING;
//                     _epoller.modif_event(fd, EPOLLOUT | EPOLLET);
//                 }
//             }
//             if (events & EPOLLOUT)
//             {
//                 Client &c = _manager.get_socket_client_by_fd(fd);
//                 if (do_write(c))
//                 {
//                     if (c.is_keep_alive)
//                     {
//                         c.reset();
//                         _epoller.modif_event(fd, EPOLLIN | EPOLLET);
//                     }
//                     else
//                     {
//                         _manager.remove_socket_client(fd);
//                         _epoller.del_event(fd);
//                         close(fd);
//                     }
//                 }
//             }
//             // else if () // error?
//             // {
//             //     _epoller.del_event(fd);
//             //     close(fd);
//             // }
//         }
//     }
// }

bool Server::do_write(Client &c)
{
    while (c.write_pos < c.write_buffer.size())
    {
        ssize_t n = send(c.get_fd(), c.write_buffer.data() + c.write_pos, c.write_buffer.size() - c.write_pos, 0);
        if (n > 0)
            c.write_pos += n;
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return (false);
            c._state = ERROR;
            return (true); // 让上层走 close
        }
    }
    return (true); // 写完
}
