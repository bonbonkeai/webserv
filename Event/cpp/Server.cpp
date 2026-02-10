#include "Event/hpp/Server.hpp"
#include <cstring>
#include <sys/wait.h>
#include <sys/time.h>
#include <vector>

#define Timeout 50 // 50-100
#define ALL_TIMEOUT_MS 5000ULL

#define TRACE() std::cout << "[] " << __FILE__ << ":" << __LINE__ << std::endl;
volatile sig_atomic_t Server::g_running = 1;

// ajouter le cas ->keep-alive
static bool shouldCloseByStatus(int statusCode)
{
    // 400/413/408 要 close
    if (statusCode == 400 || statusCode == 411 || statusCode == 413 || statusCode == 408 || statusCode == 431 || statusCode == 414 || statusCode == 501)
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

// static void terminateCgiOnce(CGI_Process *cgi)
//{
//     if (!cgi)
//         return;
//     if (cgi->_pid > 0)
//     {
//         kill(cgi->_pid, SIGKILL);
//         waitpid(cgi->_pid, NULL, 0);
//         cgi->_pid = -1; // 关键：防止后续任何路径重复 kill
//     }
// }

// 这里端口-1，然后 htons(port_nbr)。这可能会导致 bind 失败（或者绑定到不可预期端口），服务器根本起不来。或许可以先给一个固定值（比如 8080）？
Server::Server(int port) : port_nbr(port), socketfd(-1), _routing(NULL)
{
    _epoller = new Epoller();
    _manager = new ClientManager();
    _session_cookie = new Session_manager();
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGCHLD, SIG_DFL);
    g_running = 1;
}
Server::~Server()
{
    cleanup();
}

void Server::signal_handler(int sig)
{
    std::cout << "\n[Signal] shutdown\n";
    if (sig == SIGINT)
        g_running = 0;
}

void Server::cleanup()
{
    // clean clients
    std::map<int, Client *> clients = _manager->get_all_socket_clients();
    for (std::map<int, Client *>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        if (it->first >= 0)
        {
            if (it->second && it->second->_cgi)
            {
                it->second->_cgi->terminate();
            }
            _epoller->del_event(it->first);
            close(it->first);
        }
    }
    _manager->clear_all_clients();

    // clean cgi clients
    std::map<int, Client *> cgi_clients = _manager->get_all_cgi_clients();
    for (std::map<int, Client *>::iterator it = cgi_clients.begin(); it != cgi_clients.end(); ++it)
    {
        if (it->first >= 0)
        {
            _epoller->del_event(it->first);
            close(it->first);
        }
    }
    _manager->clear_all_cgi_clients();

    if (socketfd >= 0)
    {
        _epoller->del_event(socketfd);
        close(socketfd);
        socketfd = -1;
    }
    if (_epoller)
    {
        delete _epoller;
        _epoller = NULL;
    }
    if (_manager)
    {
        delete _manager;
        _manager = NULL;
    }
    if (_session_cookie)
    {
        delete _session_cookie;
        _session_cookie = NULL;
    }
    if (_routing)
    {
        delete _routing;
        _routing = NULL;
    }
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
    return _epoller->init(128);
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
            return true;
        Server::set_non_block_fd(connect_fd);
        _epoller->add_event(connect_fd, EPOLLIN | EPOLLET);
        _manager->add_socket_client(connect_fd); // client state is read_line
        // 这里面更新时间没有必要，因为add socket client的时候，client construit的时候就已经有一个时间设置了
        // Client *c = _manager->get_socket_client_by_fd(connect_fd);
        // if (c)
        //     c->last_activity_ms = Client::now_ms();
        //
    }
    return true;
}
// bool Server::handle_connection()
//{
//     while (true)
//     {
//         struct sockaddr_in clientaddr;
//         socklen_t client_len = sizeof(clientaddr);
//         int connect_fd = accept(socketfd, (struct sockaddr *)&clientaddr, &client_len);
//         if (connect_fd < 0)
//         {
//             if (errno == EAGAIN || errno == EWOULDBLOCK)
//                 return true;
//             return false;
//         }
//         Server::set_non_block_fd(connect_fd);
//         _epoller->add_event(connect_fd, EPOLLIN | EPOLLET);
//         _manager->add_socket_client(connect_fd); // client state is read_line
//         // 这里面更新时间没有必要，因为add socket client的时候，client construit的时候就已经有一个时间设置了
//         // Client *c = _manager->get_socket_client_by_fd(connect_fd);
//         // if (c)
//         //     c->last_activity_ms = Client::now_ms();
//         //
//     }
//     return true;
// }

HTTPResponse Server::process_request(const HTTPRequest &req)
{
    IRequest *h = RequestFactory::create(req);
    HTTPResponse resp = h->handle();
    delete h;
    return (resp);
}
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
            continue;
        }
        else if (n == 0)
        {
            c._state = CLOSED;
            return (false);
        }
        break;
    }
    if (c.parser.getRequest().complet)
    {
        c._state = PROCESS;
        return (true);
    }
    return (false);
}

// bool Server::do_read(Client &c)
//{
//     char tmp[4096];
//     while (true)
//     {
//         ssize_t n = recv(c.get_fd(), tmp, sizeof(tmp), 0);
//         if (n > 0)
//         {
//             c.last_activity_ms = Client::now_ms();
//             bool ok = c.parser.dejaParse(std::string(tmp, n));
//             if (!ok && c.parser.getRequest().bad_request)
//             {
//                 c._state = ERROR;
//                 const HTTPRequest &req = c.parser.getRequest();
//                 int code = req.error_code > 0 ? req.error_code : 400;
//                 HTTPResponse err = buildErrorResponse(code);
//                 bool ka = computeKeepAlive(req, code);
//                 c.is_keep_alive = ka;
//                 applyConnectionHeader(err, ka);
//                 c.write_buffer = ResponseBuilder::build(err);
//                 c.write_pos = 0;
//                 return (true);
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
// }

// void Server::handle_cgi_read_error(Client &c, int pipe_fd)
//{
//     // 1) 终止 CGI（只做一次）
//     terminateCgiOnce(c._cgi);
//     // 2) 清理 pipe（只通过 del_cgi_fd 关闭 fd/归零 is_cgi）
//     _epoller->del_event(pipe_fd);
//     _manager->del_cgi_fd(pipe_fd);
//     // 3) 构造错误响应并切 WRITING
//     const HTTPRequest &req = c.parser.getRequest();
//     HTTPResponse err = buildErrorResponse(500);
//     bool ka = computeKeepAlive(req, 500);
//     c.is_keep_alive = ka;
//     applyConnectionHeader(err, ka);
//     c.write_buffer = ResponseBuilder::build(err);
//     c.write_pos = 0;
//     c._state = WRITING;
//     _epoller->modif_event(c.client_fd, EPOLLOUT | EPOLLET);
// }

void Server::handle_cgi_read(Client &c, int pipe_fd)
{
    if (!c._cgi)
    {
        handle_pipe_error(pipe_fd);
        return;
    }
    bool finished = c._cgi->handle_output();
    if (finished)
    {
        c._cgi->get_exit_status();
        finalize_cgi_response(c, pipe_fd);
    }
    // char buf[4096];
    // while (true)
    //{
    //     ssize_t n = read(pipe_fd, buf, sizeof(buf));
    //
    //    if (n > 0 && c._cgi)
    //    {
    //        c._cgi->append_output(buf, n);
    //        c._cgi->last_output_ms = Client::now_ms();
    //    }
    //    else if (n == 0) // cgi finish
    //    {
    //        // 1) 尽量回收子进程（不阻塞）
    //        if (c._cgi && c._cgi->_pid > 0)
    //        {
    //            pid_t r = waitpid(c._cgi->_pid, NULL, WNOHANG);
    //            if (r > 0)
    //                c._cgi->_pid = -1;
    //            // 不要在这里 kill；正常结束不kill
    //            // 也不要在这里把 pid=-1（可选），避免破坏其他逻辑
    //        }
    //        // 2) 先把 CGI 输出拷贝出来
    //        std::string cgi_out;
    //        if (c._cgi)
    //            cgi_out = c._cgi->_output_buffer;
    //        // 3) 再清理 pipe（这一步会清 buffer，所以必须放在拷贝之后）
    //        _epoller->del_event(pipe_fd);
    //        _manager->del_cgi_fd(pipe_fd);
    //
    //        // 4) build response from copied output
    //        HTTPResponse resp;
    //        resp = resp.buildResponseFromCGIOutput(cgi_out, c.parser.getRequest().keep_alive);
    //        bool ka = computeKeepAlive(c.parser.getRequest(), resp.statusCode);
    //        c.is_keep_alive = ka;
    //        applyConnectionHeader(resp, ka);
    //        c.write_buffer = ResponseBuilder::build(resp);
    //        c.write_pos = 0;
    //        c._state = WRITING;
    //        _epoller->modif_event(c.client_fd, EPOLLOUT | EPOLLET);
    //        break;
    //    }
    //    else
    //    {
    //        if (errno == EAGAIN || errno == EWOULDBLOCK)
    //            break;
    //        handle_cgi_read_error(c, pipe_fd);
    //        break;
    //    }
    //}
}

void Server::handle_pipe_error(int fd)
{
    Client *c = _manager->get_client_by_cgi_fd(fd);
    if (!c || !c->_cgi)
        return;
    c->_cgi->handle_pipe_error();
    c->_cgi->_state = CGI_Process::ERROR_CGI;
    finalize_cgi_response(*c, fd);
    // if (!c)
    //     return;
    //// 1) 终止 CGI（只做一次）
    // terminateCgiOnce(c->_cgi);
    //// 2) 清理 pipe
    //_epoller->del_event(fd);
    //_manager->del_cgi_fd(fd);
    //// 3) 构造错误响应
    // HTTPResponse err = buildErrorResponse(500);
    // bool ka = computeKeepAlive(c->parser.getRequest(), 500);
    // c->is_keep_alive = ka;
    // applyConnectionHeader(err, ka);
    // c->write_buffer = ResponseBuilder::build(err);
    // c->write_pos = 0;
    // c->_state = WRITING;
    //_epoller->modif_event(c->client_fd, EPOLLOUT | EPOLLET);
}
void Server::finalize_cgi_response(Client &c, int pipe_fd)
{
    if (c._cgi)
    {
        c._cgi->handle_output();
        c._cgi->get_exit_status();
    }
    bool keep_alive = c.parser.getRequest().keep_alive;
    HTTPResponse resp;
    if (c._cgi)
        resp = c._cgi->build_response(keep_alive);
    else
        resp = buildErrorResponse(500);

    _epoller->del_event(pipe_fd);
    _manager->del_cgi_fd(pipe_fd);

    if (c._cgi)
    {
        c._cgi->terminate();
        delete c._cgi;
        c._cgi = NULL;
    }
    c.is_cgi = false;

    // 4. 设置 keep-alive
    bool ka = computeKeepAlive(c.parser.getRequest(), resp.statusCode);
    c.is_keep_alive = ka;
    applyConnectionHeader(resp, ka);

    // 5. 切换到写状态
    c.write_buffer = ResponseBuilder::build(resp);
    c.write_pos = 0;
    c._state = WRITING;
    _epoller->modif_event(c.client_fd, EPOLLOUT | EPOLLET);
}

void Server::handle_socket_error(int fd)
{
    Client *c = _manager->get_socket_client_by_fd(fd);
    if (!c)
        return;

    if (c->is_cgi)
        c->_cgi->terminate();

    _epoller->del_event(fd);
    _manager->remove_socket_client(fd);
    close(fd);
}

void Server::close_client(int fd)
{
    _manager->remove_socket_client(fd);
    _epoller->del_event(fd);
    close(fd);
}

bool Server::do_write(Client &c)
{
    while (c.write_pos < c.write_buffer.size())
    {
        ssize_t n = send(c.get_fd(), c.write_buffer.data() + c.write_pos, c.write_buffer.size() - c.write_pos, 0);
        if (n > 0)
        {
            c.write_pos += n;
            continue;
        }
        return false;
    }
    return (true); // 写完
}
// bool Server::do_write(Client &c)
//{
//     while (c.write_pos < c.write_buffer.size())
//     {
//         ssize_t n = send(c.get_fd(), c.write_buffer.data() + c.write_pos, c.write_buffer.size() - c.write_pos, 0);
//         if (n > 0)
//             c.write_pos += n;
//         else
//         {
//             if (errno == EAGAIN || errno == EWOULDBLOCK)
//                 return (false);
//             c.is_keep_alive = false; // <- 强制关
//             // c._state = ERROR;
//             return (true);
//         }
//     }
//     return (true); // 写完
// }

void Server::check_cgi_timeout()
{
    if (!_manager)
        return;
    unsigned long long now = Client::now_ms();
    std::vector<int> to_close;
    std::vector<Client *> timeout_client;

    std::map<int, Client *> &cgi_clients = _manager->get_all_cgi_clients();
    for (std::map<int, Client *>::iterator it = cgi_clients.begin(); it != cgi_clients.end(); ++it)
    {
        int pipe_fd = it->first; // 约定key就是read pipe fd
        Client *c = it->second;
        if (!c || !c->_cgi || !c->is_cgi)
            continue;
        if (c->_cgi->check_timeout(now))
        {
            c->_cgi->handle_timeout();
            to_close.push_back(pipe_fd);
            timeout_client.push_back(c);
        }
    }
    // 统一清理：从 epoll 移除 + manager 清理（close/erase/reset 在 del_cgi_fd 里做）
    for (size_t i = 0; i < timeout_client.size(); ++i)
    {
        finalize_cgi_response(*timeout_client[i], to_close[i]);
    }
}

void Server::check_timeout()
{
    if (!_manager)
        return;
    unsigned long long now = Client::now_ms();
    std::vector<int> timed_out;

    std::map<int, Client *> &clients = _manager->get_all_socket_clients();
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
            timed_out.push_back(it->first);
        }
    }
    for (size_t i = 0; i < timed_out.size(); ++i)
    {
        int fd = timed_out[i];
        Client *c = _manager->get_socket_client_by_fd(fd);
        if (!c)
            continue;

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
        _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
    }
}
/**
 * run 中epoller的event的fd进行检查
 *  1. 此fd是socketfd ->handle connection
 *  2. 此fd是cgi pipe的output
 *  3. 此fd是client socket ->
 *  4. 什么也不是，error处理
 *  5. error处理当中，需要对fd的来源进行检查，如果是cgi那边的问题，需要kill结束进程
 */

static bool isMethodAllowed(const std::string &m, const std::vector<std::string> &allow)
{
    for (size_t i = 0; i < allow.size(); ++i)
        if (allow[i] == m)
            return true;
    return false;
}

bool Server::buildRespForCompletedReq(Client &c, int fd)
{
    HTTPRequest req = c.parser.getRequest();

    // resolve effective config
    if (_routing)
    {
        req.effective = _routing->resolve(req, port_nbr);
        req.max_body_size = req.effective.max_body_size;
        req.has_effective = true;
    }
    else
    {
        req.effective = _default_cfg;
        req.has_effective = true;
    }
    // 405 global
    if (!isMethodAllowed(req.method, req.effective.allowed_methods))
    {
        HTTPResponse err = buildErrorResponse(405);
        bool ka = computeKeepAlive(req, 405);
        c.is_keep_alive = ka;
        applyConnectionHeader(err, ka);
        c.write_buffer = ResponseBuilder::build(err);
        c.write_pos = 0;
        c._state = WRITING;
        _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
        return (true);
    }
    // CGI
    if (req.is_cgi_request())
    {
        c._cgi = new CGI_Process();
        // std::string scriptPath = FileUtils::joinPath(req.effective.root, req.path);
        if (!c._cgi->execute(req.effective, req))
        {
            HTTPResponse err = buildErrorResponse(500);
            bool ka = computeKeepAlive(req, 500);
            c.is_keep_alive = ka;
            applyConnectionHeader(err, ka);
            c.write_buffer = ResponseBuilder::build(err);
            c.write_pos = 0;
            c._state = WRITING;
            _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
            delete c._cgi;
            c._cgi = NULL;
            return true;
        }
        c.is_cgi = true;
        _epoller->add_event(c._cgi->_read_fd, EPOLLIN | EPOLLET);
        _manager->bind_cgi_fd(c._cgi->_read_fd, c.client_fd);
        return (true);
    }
    // normal
    HTTPResponse resp = process_request(req);
    bool ka = computeKeepAlive(req, resp.statusCode);
    c.is_keep_alive = ka;
    applyConnectionHeader(resp, ka);
    c.write_buffer = ResponseBuilder::build(resp);
    c.write_pos = 0;
    c._state = WRITING;
    _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
    return (true);
}

void Server::run()
{
    set_non_block_fd(socketfd);
    _epoller->add_event(socketfd, EPOLLIN | EPOLLET);
    while (g_running)
    {
        int nfds = _epoller->wait(Timeout);
        check_cgi_timeout();
        check_timeout();
        for (int i = 0; i < nfds; i++)
        {
            int fd = _epoller->get_event_fd(i);
            uint32_t events = _epoller->get_event_type(i);
            if (fd == socketfd)
            {
                handle_connection();
                continue;
            }
            if (_manager->is_cgi_pipe(fd))
            {
                Client *c = _manager->get_client_by_cgi_fd(fd);
                if (c && (events & (EPOLLIN | EPOLLHUP)))
                {
                    handle_cgi_read(*c, fd);
                    continue;
                }
                if (c && (events & (EPOLLERR | EPOLLRDHUP)))
                {
                    handle_pipe_error(fd);
                    continue;
                }
            }
            else // socket client
            {
                Client *c = _manager->get_socket_client_by_fd(fd);
                if (!c)
                    continue;
                if (events & EPOLLIN)
                {
                    if (do_read(*c))
                    {
                        if (c->_state == PROCESS)
                        {
                            buildRespForCompletedReq(*c, fd);
                            continue;
                        }
                        if (!c->is_cgi)
                        {
                            c->_state = WRITING;
                            _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
                        }
                    }
                }
                if (events & EPOLLOUT)
                {
                    // do_write == false: 只是 EAGAIN，还没写完，继续等下一次 EPOLLOUT
                    if (!do_write(*c))
                        continue;
                    // do_write == true: 本次 response 已经全部写完
                    if (!c->is_keep_alive)
                    {
                        _manager->remove_socket_client(fd);
                        _epoller->del_event(fd);
                        close(fd);
                        continue;
                    }
                    // keep-alive: reset 当前 request 状态，但必须保留 parser 内部 buffer
                    // c->parser.resetForNextRequest(); // 前提：不清 _buffer，只清 request/state
                    c->reset();
                    // 如果 buffer 里已经有 pipelined 数据，尝试直接解析下一条
                    while (!c->is_cgi && c->parser.hasBufferedData())
                    {
                        bool ok = c->parser.dejaParse(std::string());
                        if (!ok && c->parser.getRequest().bad_request)
                        {
                            const HTTPRequest &rq = c->parser.getRequest();
                            int code = rq.error_code > 0 ? rq.error_code : 400;
                            HTTPResponse err = buildErrorResponse(code);
                            bool ka = computeKeepAlive(rq, code);
                            c->is_keep_alive = ka;
                            applyConnectionHeader(err, ka);
                            c->write_buffer = ResponseBuilder::build(err);
                            c->write_pos = 0;
                            c->_state = WRITING;
                            _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
                            break; // 下次 EPOLLOUT 发送 err
                        }
                        if (!c->parser.getRequest().complet)
                            break; // 还缺字节，回去等 EPOLLIN
                        c->_state = PROCESS;
                        buildRespForCompletedReq(*c, fd); // 这个函数内部应该把 state=WRITING 并 modif_event(EPOLLOUT)
                        break;                            // 一次只准备一个 response，保持顺序
                    }
                    // 没准备出新 response，就回去读
                    if (!c->is_cgi && (c->_state != WRITING || c->write_buffer.empty()))
                        _epoller->modif_event(fd, EPOLLIN | EPOLLET);
                    else
                    {
                        _manager->remove_socket_client(fd);
                        _epoller->del_event(fd);
                        close(fd);
                    }
                }
                if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
                {
                    // std::cout << "errno: " << errno << std::endl;
                    // // CGI pipe 的错误，直接走 pipe error（它会生成 500 并写回 client）
                    // if (_manager->is_cgi_pipe(fd))
                    // {
                    //     TRACE();
                    //     handle_pipe_error(fd);
                    //     continue;
                    // }
                    // // client socket 的错误：如果已经有待发送响应，优先尝试发送，不要立刻 close 覆盖掉 504/500
                    // Client *c = _manager->get_socket_client_by_fd(fd);
                    if (c && c->_state == WRITING && !c->write_buffer.empty())
                    {
                        c->is_keep_alive = false;
                        // 尝试直接写一次（即使没有 EPOLLOUT）
                        if (do_write(*c))
                        {
                            _manager->remove_socket_client(fd);
                            _epoller->del_event(fd);
                            close(fd);
                            continue;
                        }
                        // 写到 EAGAIN -> 继续等 EPOLLOUT
                        _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
                        continue;
                    }
                    // 没有待发送响应 -> 正常错误清理
                    handle_socket_error(fd);
                    continue;
                }
            }
        }
    }
}

bool Server::load_config(const std::string &path)
{
    ConfigTokenizer tok;
    if (!tok.read_file(path))
        throw std::runtime_error("config: cannot read file");
    ConfigParser parser(tok.getTokens());
    std::vector<ServerConfig> raw = parser.parse();
    _rt_servers.clear();
    for (size_t i = 0; i < raw.size(); ++i)
    {
        ServerRuntimeConfig srv = buildServer(raw[i]);
        for (size_t j = 0; j < raw[i].locations.size(); ++j)
        {
            LocationRuntimeConfig loc = buildLocation(srv, raw[i].locations[j]);
            srv.locations.push_back(loc);
        }
        _rt_servers.push_back(srv);
    }
    if (_routing)
    {
        delete _routing;
        _routing = NULL;
    }
    if (!_rt_servers.empty())
        _routing = new Routing(_rt_servers);
    if (_rt_servers.empty())
        throw std::runtime_error("config: no server block found");
    // 默认 cfg：用于“还没解析 Host 时”的兜底
    // 这里用第一个 server 的 defaults
    //_default_cfg.root = _rt_servers[0].root;
    //_default_cfg.index = _rt_servers[0].index;
    //_default_cfg.autoindex = _rt_servers[0].autoindex;
    //_default_cfg.allowed_methods = _rt_servers[0].allowed_methods;
    //_default_cfg.error_pages = _rt_servers[0].error_page;
    //_default_cfg.max_body_size = _rt_servers[0].client__max_body_size;
    if (!_rt_servers.empty())
    {
        const ServerRuntimeConfig &first_server = _rt_servers[0];
        _default_cfg.server_port = first_server.port;
        _default_cfg.server_name = first_server.server_name;
        _default_cfg.root = first_server.root;
        _default_cfg.index = first_server.index;
        _default_cfg.autoindex = first_server.autoindex;
        _default_cfg.allowed_methods = first_server.allowed_methods;
        _default_cfg.error_pages = first_server.error_page;
        _default_cfg.max_body_size = first_server.client__max_body_size;

        // 设置默认值
        _default_cfg.alias = "";
        _default_cfg.location_path = "";
        _default_cfg.has_return = false;
        _default_cfg.return_code = 302;
        _default_cfg.return_url = "";
        _default_cfg.is_cgi = false;
        _default_cfg.upload_path = "";
    }
    return (true);
}
