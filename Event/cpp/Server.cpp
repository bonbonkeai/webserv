#include "Event/hpp/Server.hpp"
#include <cstring>
#include <sys/wait.h>
#include <sys/time.h>

#define Timeout 50 // 50-100
#define ALL_TIMEOUT_MS 5000ULL
#define CGI_TIMEOUT_MS 10000ULL

// ajouter le cas ->keep-alive
static bool shouldCloseByStatus(int statusCode)
{
    // 400/413/408 要 close
    if (statusCode == 400 || statusCode == 413 || statusCode == 408 || statusCode == 431 || statusCode == 414 || statusCode == 501)
        return (true);
    return (false);
}

static bool computeKeepAlive(const HTTPRequest &req, int statusCode)
{
    // 客户端显式 close -> 永远 close（ parser 已经把 req.keep_alive 置 false）
    if (!req.keep_alive)
        return (false);

    // 解析错误阶段:先稳妥统一 close，这里就按状态码关
    if (shouldCloseByStatus(statusCode))
        return (false);

    // 403/404/405/500 走这里 -> keep-alive
    return (true);
}

static void applyConnectionHeader(HTTPResponse &resp, bool keepAlive)
{
    resp.headers["connection"] = keepAlive ? "keep-alive" : "close";
}

static void terminateCgiOnce(CGI_Process *cgi)
{
    if (!cgi)
        return;
    if (cgi->_pid > 0)
    {
        kill(cgi->_pid, SIGKILL);
        waitpid(cgi->_pid, NULL, 0);
        cgi->_pid = -1; // 关键：防止后续任何路径重复 kill
    }
}

#define TRACE() std::cout << "[TRACE] " << __FILE__ << ":" << __LINE__ << std::endl;

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
        // 这里面更新时间没有必要，因为add socket client的时候，client construit的时候就已经有一个时间设置了
       // Client *c = _manager.get_socket_client_by_fd(connect_fd);
       // if (c)
       //     c->last_activity_ms = Client::now_ms();
       // 
    }
    return true;
}

// static HTTPResponse process_request(const HTTPRequest &req)
// {
//     IRequest *h = RequestFactory::create(req);
//     HTTPResponse resp = h->handle();
//     delete h;
//     return (resp);
// }

HTTPResponse Server::process_request(const HTTPRequest &req)
{
    IRequest *h = RequestFactory::create(req);
    HTTPResponse resp = h->handle();
    delete h;
    return (resp);
}

// bool Server::do_read(Client &c)
// {
//     char tmp[4096];
//     // bool is_reading = true;

//     while (true)
//     {
//         ssize_t n = recv(c.get_fd(), tmp, sizeof(tmp), 0);
//         if (n > 0)
//         {
//             bool ok = c.parser.dejaParse(std::string(tmp, n));
//             if (!ok && c.parser.getRequest().bad_request)
//             {
//                 c._state = ERROR;
//                 // 生成错误响应//
//                 HTTPResponse err = buildErrorResponse(400); // 暂时先放400，后续可以根据具体的error_code来修改
//                 // HTTPResponse err = buildErrorResponse(c.parser.getRequest().error_code);
//                 c.is_keep_alive = false;
//                 c.write_buffer = ResponseBuilder::build(err);
//                 c.write_pos = 0;
//                 return (true); // 让上层切到 WRITING
//             }
//         }
//         else if (n == 0)
//         {
//             c._state = CLOSED;
//             return (false);
//         }
//         else
//         {
//             if (errno == EAGAIN || errno == EWOULDBLOCK)
//                 break;
//             c._state = ERROR;
//             return (false);
//         }
//     }
//     if (c.parser.getRequest().complet)
//     {
//         c._state = PROCESS;
//         return (true);
//     }
//     return (false);
//     // while (is_reading)
//     // {
//     //     if (c._state == RD_LINE)
//     //         is_reading = read_request_line(c);
//     //     else if (c._state == RD_HEADER)
//     //         is_reading = read_request_header(c);
//     //     else if (c._state == RD_BODY)
//     //         is_reading = read_request_body(c);
//     //     else
//     //         is_reading = false;
//     // }
//     // return (c._state == RD_DONE);
// }

bool Server::do_read(Client &c)
{
    char tmp[4096];
    while (true)
    {
        ssize_t n = recv(c.get_fd(), tmp, sizeof(tmp), 0);
        if (n > 0)
        {
            c.last_activity_ms = Client::now_ms();
            bool ok = c.parser.dejaParse(std::string(tmp, n));
            if (!ok && c.parser.getRequest().bad_request)
            {
                c._state = ERROR;
                const HTTPRequest &req = c.parser.getRequest();
                int code = req.error_code > 0 ? req.error_code : 400;
                HTTPResponse err = buildErrorResponse(code);
                bool ka = computeKeepAlive(req, code);
                c.is_keep_alive = ka;
                applyConnectionHeader(err, ka);
                c.write_buffer = ResponseBuilder::build(err);
                c.write_pos = 0;
                return (true);
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

// void Server::handle_cgi_read_error(Client &c, int pipe_fd)
// {
//     _epoller.del_event(pipe_fd);
//     _manager.del_cgi_fd(pipe_fd);

//     kill(c._cgi->get_pid(), SIGKILL);
//     waitpid(c._cgi->get_pid(), NULL, 0);
//     c._cgi->reset();

//     const HTTPRequest& req = c.parser.getRequest();
//     HTTPResponse err = buildErrorResponse(500);
//     bool ka = computeKeepAlive(req, 500);
//     c.is_keep_alive = ka;
//     applyConnectionHeader(err, ka);

//     c.write_buffer = ResponseBuilder::build(err);
//     c.write_pos = 0;
//     c._state = WRITING;
//     _epoller.modif_event(c.client_fd, EPOLLOUT | EPOLLET);
// }
void Server::handle_cgi_read_error(Client &c, int pipe_fd)
{
    // 1) 终止 CGI（只做一次）
    terminateCgiOnce(c._cgi);
    // 2) 清理 pipe（只通过 del_cgi_fd 关闭 fd/归零 is_cgi）
    _epoller.del_event(pipe_fd);
    _manager.del_cgi_fd(pipe_fd);
    // 3) 构造错误响应并切 WRITING
    const HTTPRequest &req = c.parser.getRequest();
    HTTPResponse err = buildErrorResponse(500);
    bool ka = computeKeepAlive(req, 500);
    c.is_keep_alive = ka;
    applyConnectionHeader(err, ka);
    c.write_buffer = ResponseBuilder::build(err);
    c.write_pos = 0;
    c._state = WRITING;
    _epoller.modif_event(c.client_fd, EPOLLOUT | EPOLLET);
}

void Server::handle_cgi_read(Client &c, int pipe_fd)
{
    char buf[4096];
    while (true)
    {
        ssize_t n = read(pipe_fd, buf, sizeof(buf));
        // if (n > 0)
        //     c._cgi->append_output(buf, n);
        if (n > 0)
        {
            if (c._cgi)
                c._cgi->append_output(buf, n);
        }
        // else if (n == 0) // cgi finish
        // {
        //     _epoller.del_event(pipe_fd);
        //     _manager.del_cgi_fd(pipe_fd);
        //     waitpid(c._cgi->get_pid(), NULL, WNOHANG);

        //     // client prepare la reponse
        //     HTTPResponse resp;
        //     resp = resp.buildResponseFromCGIOutput(c._cgi->get_output(),
        //                                            c.parser.getRequest().keep_alive);
        //     //keep-alive
        //     bool ka = computeKeepAlive(c.parser.getRequest(), resp.statusCode);
        //     c.is_keep_alive = ka;
        //     applyConnectionHeader(resp, ka);
        //     c.write_buffer = ResponseBuilder::build(resp);
        //     c.write_pos = 0;

        //     // c._cgi->reset();
        //     c.is_cgi = false;

        //     c._state = WRITING;
        //     _epoller.modif_event(c.client_fd, EPOLLOUT | EPOLLET);
        //     break;
        // }
        else if (n == 0) // cgi finish
        {
            // 1) 尽量回收子进程（不阻塞）
            if (c._cgi && c._cgi->_pid > 0)
            {
                pid_t r = waitpid(c._cgi->_pid, NULL, WNOHANG);
                if (r > 0)
                    c._cgi->_pid = -1;
                // 不要在这里 kill；正常结束不kill
                // 也不要在这里把 pid=-1（可选），避免破坏其他逻辑
            }
            // 2) 先把 CGI 输出拷贝出来
            std::string cgi_out;
            if (c._cgi)
                cgi_out = c._cgi->_output_buffer;
            // 3) 再清理 pipe（这一步会清 buffer，所以必须放在拷贝之后）
            _epoller.del_event(pipe_fd);
            _manager.del_cgi_fd(pipe_fd);

            // 4) build response from copied output
            HTTPResponse resp;
            resp = resp.buildResponseFromCGIOutput(cgi_out, c.parser.getRequest().keep_alive);
            bool ka = computeKeepAlive(c.parser.getRequest(), resp.statusCode);
            c.is_keep_alive = ka;
            applyConnectionHeader(resp, ka);
            c.write_buffer = ResponseBuilder::build(resp);
            c.write_pos = 0;
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

// void Server::handle_pipe_error(int fd)
// {
//     Client *c = _manager.get_socket_client_by_fd_by_cgi_fd(fd);

//     if (!c)
//         return;
//     _epoller.del_event(fd);
//     _manager.del_cgi_fd(fd);

//     if (c->_cgi->get_pid() > 0)
//     {
//         kill(c->_cgi->get_pid(), SIGKILL);
//         waitpid(c->_cgi->get_pid(), NULL, WNOHANG);
//     }
//     c->_cgi->reset();
//     // HTTPResponse err = buildErrorResponse(500);

//     //keep-alive
//     HTTPResponse err = buildErrorResponse(500);
//     bool ka = computeKeepAlive(c->parser.getRequest(), 500);
//     c->is_keep_alive = ka;
//     applyConnectionHeader(err, ka);

//     c->write_buffer = ResponseBuilder::build(err);
//     c->write_pos = 0;
//     c->_state = WRITING;
//     // c->is_keep_alive = false;

//     _epoller.modif_event(c->client_fd, EPOLLOUT | EPOLLET);
// }
void Server::handle_pipe_error(int fd)
{
    Client *c = _manager.get_client_by_cgi_fd(fd);
    if (!c)
        return;
    // 1) 终止 CGI（只做一次）
    terminateCgiOnce(c->_cgi);
    // 2) 清理 pipe
    _epoller.del_event(fd);
    _manager.del_cgi_fd(fd);
    // 3) 构造错误响应
    HTTPResponse err = buildErrorResponse(500);
    bool ka = computeKeepAlive(c->parser.getRequest(), 500);
    c->is_keep_alive = ka;
    applyConnectionHeader(err, ka);
    c->write_buffer = ResponseBuilder::build(err);
    c->write_pos = 0;
    c->_state = WRITING;
    _epoller.modif_event(c->client_fd, EPOLLOUT | EPOLLET);
}

// void Server::handle_socket_error(int fd)
// {
//     Client *c = _manager.get_socket_client_by_fd(fd);

//     if (!c)
//         return;
//     if (c->is_cgi && c->_cgi->get_pid() > 0)
//     {
//         kill(c->_cgi->get_pid(), SIGKILL);
//         waitpid(c->_cgi->get_pid(), NULL, 0);
//         int cgi_fd = c->_cgi->get_read_fd();
//         if (cgi_fd >= 0)
//         {
//             _epoller.del_event(cgi_fd);
//             _manager.del_cgi_fd(cgi_fd);
//         }
//     }
//     _epoller.del_event(fd);
//     _manager.remove_client(fd);
//     close(fd);
// }
void Server::handle_socket_error(int fd)
{
    Client *c = _manager.get_socket_client_by_fd(fd);
    if (!c)
        return;

    if (c->is_cgi)
        terminateCgiOnce(c->_cgi);

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
            if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                // CGI pipe 的错误，直接走 pipe error（它会生成 500 并写回 client）
                if (_manager.is_cgi_pipe(fd))
                {
                    handle_pipe_error(fd);
                    continue;
                }
                // client socket 的错误：如果已经有待发送响应，优先尝试发送，不要立刻 close 覆盖掉 504/500
                Client *c = _manager.get_socket_client_by_fd(fd);
                if (c && c->_state == WRITING && !c->write_buffer.empty())
                {
                    c->is_keep_alive = false;
                    // 尝试直接写一次（即使没有 EPOLLOUT）
                    if (do_write(*c))
                    {
                        _manager.remove_socket_client(fd);
                        _epoller.del_event(fd);
                        close(fd);
                        continue;
                    }
                    // 写到 EAGAIN -> 继续等 EPOLLOUT
                    _epoller.modif_event(fd, EPOLLOUT | EPOLLET);
                    continue;
                }
                // 没有待发送响应 -> 正常错误清理
                handle_socket_error(fd);
                continue;
            }
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
                // if (do_read(*c))
                // {
                //     c->_state = PROCESS;
                //     process_request(*c);
                //     _epoller.modif_event(fd, EPOLLOUT | EPOLLET);
                // }
                if (do_read(*c))
                {
                    if (c->_state == PROCESS)
                    {
                        HTTPRequest req = c->parser.getRequest();
                        HTTPResponse resp = process_request(req);
                        // c->is_keep_alive = req.keep_alive;
                        // keep-alive
                        bool ka = computeKeepAlive(req, resp.statusCode);
                        c->is_keep_alive = ka;
                        applyConnectionHeader(resp, ka);

                        c->write_buffer = ResponseBuilder::build(resp);
                        c->write_pos = 0;
                    }
                    c->_state = WRITING;
                    _epoller.modif_event(fd, EPOLLOUT | EPOLLET);   
                }
            }
            if (events & EPOLLOUT)
            {
                if (do_write(*c))
                {
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
        // else
        // {
        //     if (errno == EAGAIN || errno == EWOULDBLOCK)
        //         return (false);
        //     c._state = ERROR;
        //     return (true); // 让上层走 close
        // }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return (false);
            c.is_keep_alive = false; // <- 强制关
            // c._state = ERROR;
            return (true);
        }
    }
    return (true); // 写完
}

// 处理timeout
//  void Server::check_cgi_timeout()
//  {
//      time_t now = std::time(0);
//      std::vector<int> to_close;

//     std::map<int, Client *> &cgi_clients = _manager.get_all_cgi_clients();
//     for (std::map<int, Client *>::iterator it = cgi_clients.begin(); it != cgi_clients.end(); ++it)
//     {
//         Client *c = it->second;
//         CGI_Process *cgi = c->_cgi;

//         if (!cgi || !c->is_cgi)
//             continue;
//         if ((now - c->_cgi->start_time) > CGI_TIMEOUT)
//         {
//             std::cout << "TIMEOUT: " << c->client_fd << "|is_cgi: " << c->is_cgi << std::endl;
//             to_close.push_back(cgi->_read_fd);
//             if (cgi->_pid > 0)
//             {
//                 //可能需要一个reponse的输出
//                 kill(cgi->_pid, SIGKILL);
//                 waitpid(cgi->_pid, NULL, 0);
//             }
//             if (cgi->_read_fd >= 0)
//                 close(cgi->_read_fd);
//             if (cgi->_write_fd >= 0)
//                 close(cgi->_write_fd);
//             cgi->reset();
//             c->is_cgi = false;

//            HTTPResponse err = buildErrorResponse(504);
//            err.headers["connection"] = "close";
//            if (err.headers.find("content-length") == err.headers.end())
//                 err.headers["content-length"] = toString(err.body.size());
//             // 先把连接层状态设为 close
//             c->is_keep_alive = false;
//             // 再生成最终要发送的字节串
//             c->write_buffer = ResponseBuilder::build(err);
//             c->write_pos = 0;
//             c->_state = WRITING;
//             // 监听写事件
//             // 修改客户端事件为可写
//             _epoller.modif_event(c->client_fd, EPOLLOUT | EPOLLET);
//             to_close.push_back(it->first);
//         }
//     }

//     for (size_t i = 0; i < to_close.size(); i++)
//     {
//         _epoller.del_event(to_close[i]);
//         _manager.del_cgi_fd(to_close[i]);
//     }
// }

void Server::check_cgi_timeout()
{
    unsigned long long now = Client::now_ms();
    std::vector<int> to_close;

    std::map<int, Client *> &cgi_clients = _manager.get_all_cgi_clients();
    for (std::map<int, Client *>::iterator it = cgi_clients.begin(); it != cgi_clients.end(); ++it)
    {
        int pipe_fd = it->first; // 约定key就是read pipe fd
        Client *c = it->second;
        CGI_Process *cgi = c ? c->_cgi : 0;

        if (!c || !cgi || !c->is_cgi)
            continue;
        if (cgi->start_time_ms > 0 && (now - cgi->start_time_ms) > CGI_TIMEOUT_MS)
        {
            // 1) 只 kill/waitpid 一次（不在 del_cgi_fd 再做）
            terminateCgiOnce(cgi);

            // 2) 准备 504 响应（header在build前写死）
            HTTPResponse err = buildErrorResponse(504);
            err.headers["connection"] = "close";
            if (err.headers.find("content-length") == err.headers.end())
                err.headers["content-length"] = toString(err.body.size());
            c->is_keep_alive = false;
            c->write_buffer = ResponseBuilder::build(err);
            c->write_pos = 0;
            c->_state = WRITING;
            // 3) 修改 client fd 监听写
            _epoller.modif_event(c->client_fd, EPOLLOUT | EPOLLET);
            // 4) 记录需要清理的 pipe fd（只记录一次）
            to_close.push_back(pipe_fd);
        }
    }
    // 统一清理：从 epoll 移除 + manager 清理（close/erase/reset 在 del_cgi_fd 里做）
    for (size_t i = 0; i < to_close.size(); ++i)
    {
        int fd = to_close[i];
        _epoller.del_event(fd);
        _manager.del_cgi_fd(fd);
    }
}

// void Server::check_timeout()
// {
//     // time_t now = std::time(0);
//     unsigned long long now = now_ms();

//     std::vector<int> to_close;
//     std::map<int, Client *> &clients = _manager.get_all_socket_clients();
//     for (std::map<int, Client *>::iterator it = clients.begin(); it != clients.end(); it++)
//     {
//         Client *c = it->second;
//         // if (c->is_timeout(now, ALL_TIMEOUT) && !c->is_cgi)
//         if (c->is_timeout(now, ALL_TIMEOUT_MS) && !c->is_cgi)
//         {
//             std::cout << "TIMEOUT: " << c->client_fd << "|is_cgi: " << c->is_cgi << std::endl;
//             to_close.push_back(c->client_fd);
//         }
//     }
//     for (size_t i = 0; i < to_close.size(); i++)
//         close_client(to_close[i]);
// }

void Server::check_timeout()
{
    unsigned long long now = Client::now_ms();
    std::vector<int> timed_out;

    std::map<int, Client *> &clients = _manager.get_all_socket_clients();
    for (std::map<int, Client *>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        Client *c = it->second;
        if (!c)
            continue;
        // 只对“正在读请求但未完成”的连接做 408
        if (c->_state == READING &&
            !c->is_cgi &&
            !c->parser.getRequest().complet &&
            c->is_timeout(now, ALL_TIMEOUT_MS))
        {
            HTTPResponse err = buildErrorResponse(408);
            err.headers["connection"] = "close"; // 强制 close，语义与 B 一致
            // content-length 一般 buildErrorResponse/ResponseBuilder 会补，但写死也无害
            if (err.headers.find("content-length") == err.headers.end())
                err.headers["content-length"] = toString(err.body.size());
            c->is_keep_alive = false;
            c->write_buffer = ResponseBuilder::build(err);
            c->write_pos = 0;
            c->_state = WRITING;
            // 让它进入写流程
            _epoller.modif_event(c->client_fd, EPOLLOUT | EPOLLET);
        }
    }
}
