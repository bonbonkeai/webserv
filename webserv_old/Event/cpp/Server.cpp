#include "Event/hpp/Server.hpp"
#include <cstring>
#include <sys/wait.h>
#include <sys/time.h>

#define Timeout 50 // 50-100
#define ALL_TIMEOUT_MS 5000ULL
#define CGI_TIMEOUT_MS 10000ULL

#define TRACE() std::cout << "[] " << __FILE__ << ":" << __LINE__ << std::endl;

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

// 这里端口-1，然后 htons(port_nbr)。这可能会导致 bind 失败（或者绑定到不可预期端口），服务器根本起不来。或许可以先给一个固定值（比如 8080）？
Server::Server(int port) : port_nbr(port), socketfd(-1), _routing(NULL)
{
    _epoller = new Epoller();
    _manager = new ClientManager();
    _session_cookie = new Session_manager();
}
Server::~Server()
{
    delete _epoller;
    _epoller = NULL;
    delete _manager;
    _manager = NULL;
    delete _session_cookie;
    _session_cookie = NULL;
    delete _routing;
    _routing = NULL;
}

void Server::cleanup()
{
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
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return true;
            return false;
        }
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
    // 1) 终止 CGI（只做一次）
    terminateCgiOnce(c._cgi);
    // 2) 清理 pipe（只通过 del_cgi_fd 关闭 fd/归零 is_cgi）
    _epoller->del_event(pipe_fd);
    _manager->del_cgi_fd(pipe_fd);
    // 3) 构造错误响应并切 WRITING
    const HTTPRequest &req = c.parser.getRequest();
    HTTPResponse err = buildErrorResponse(500);
    bool ka = computeKeepAlive(req, 500);
    c.is_keep_alive = ka;
    applyConnectionHeader(err, ka);
    c.write_buffer = ResponseBuilder::build(err);
    c.write_pos = 0;
    c._state = WRITING;
    _epoller->modif_event(c.client_fd, EPOLLOUT | EPOLLET);
}

void Server::handle_cgi_read(Client &c, int pipe_fd)
{
    char buf[4096];
    while (true)
    {
        ssize_t n = read(pipe_fd, buf, sizeof(buf));

        if (n > 0 && c._cgi)
        {
            c._cgi->append_output(buf, n);
            c._cgi->last_output_ms = Client::now_ms();
        }
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
            _epoller->del_event(pipe_fd);
            _manager->del_cgi_fd(pipe_fd);

            // 4) build response from copied output
            HTTPResponse resp;
            resp = resp.buildResponseFromCGIOutput(cgi_out, c.parser.getRequest().keep_alive);
            bool ka = computeKeepAlive(c.parser.getRequest(), resp.statusCode);
            c.is_keep_alive = ka;
            applyConnectionHeader(resp, ka);
            c.write_buffer = ResponseBuilder::build(resp);
            c.write_pos = 0;
            c._state = WRITING;
            _epoller->modif_event(c.client_fd, EPOLLOUT | EPOLLET);
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
    Client *c = _manager->get_client_by_cgi_fd(fd);
    if (!c)
        return;
    // 1) 终止 CGI（只做一次）
    terminateCgiOnce(c->_cgi);
    // 2) 清理 pipe
    _epoller->del_event(fd);
    _manager->del_cgi_fd(fd);
    // 3) 构造错误响应
    HTTPResponse err = buildErrorResponse(500);
    bool ka = computeKeepAlive(c->parser.getRequest(), 500);
    c->is_keep_alive = ka;
    applyConnectionHeader(err, ka);
    c->write_buffer = ResponseBuilder::build(err);
    c->write_pos = 0;
    c->_state = WRITING;
    _epoller->modif_event(c->client_fd, EPOLLOUT | EPOLLET);
}

void Server::handle_socket_error(int fd)
{
    Client *c = _manager->get_socket_client_by_fd(fd);
    if (!c)
        return;

    if (c->is_cgi)
        terminateCgiOnce(c->_cgi);

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
            c.write_pos += n;
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

void Server::check_cgi_timeout()
{
    unsigned long long now = Client::now_ms();
    std::vector<int> to_close;

    std::map<int, Client *> &cgi_clients = _manager->get_all_cgi_clients();
    for (std::map<int, Client *>::iterator it = cgi_clients.begin(); it != cgi_clients.end(); ++it)
    {

        int pipe_fd = it->first; // 约定key就是read pipe fd
        Client *c = it->second;
        CGI_Process *cgi = c ? c->_cgi : 0;

        bool is_timeout = false;
        if (!c || !cgi || !c->is_cgi)
            continue;

        if (!c->_cgi->_output_buffer.empty())
        {
            if ((now - cgi->last_output_ms) > CGI_TIMEOUT_MS)
                is_timeout = true;
        }
        else
        {
            if ((now - cgi->start_time_ms) > CGI_TIMEOUT_MS)
                is_timeout = true;
        }
        if (is_timeout)
        { // 1) 只 kill/waitpid 一次（不在 del_cgi_fd 再做）
            terminateCgiOnce(cgi);
            delete cgi;
            c->_cgi = NULL;
            c->is_cgi = false;
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
            _epoller->modif_event(c->client_fd, EPOLLOUT | EPOLLET);
            // 4) 记录需要清理的 pipe fd（只记录一次）
            to_close.push_back(pipe_fd);
        }
    }
    // 统一清理：从 epoll 移除 + manager 清理（close/erase/reset 在 del_cgi_fd 里做）
    for (size_t i = 0; i < to_close.size(); ++i)
    {
        int fd = to_close[i];
        _epoller->del_event(fd);
        _manager->del_cgi_fd(fd);
    }
}

void Server::check_timeout()
{
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
            _epoller->modif_event(c->client_fd, EPOLLOUT | EPOLLET);
        }
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

 static bool isMethodAllowed(const std::string& m, const std::vector<std::string>& allow)
{
    for (size_t i = 0; i < allow.size(); ++i)
        if (allow[i] == m) return true;
    return false;
}

void Server::run()
{
    set_non_block_fd(socketfd);
    _epoller->add_event(socketfd, EPOLLIN | EPOLLET);
    while (true)
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
                        // if (c->_state == PROCESS)
                        // {
                        //     HTTPRequest req = c->parser.getRequest();
                        //     HTTPResponse resp = process_request(req);

                        //     // session a tester
                        //     // std::string session_id;
                        //     //// check if is session
                        //     // if (req.headers.find("Cookie") != req.headers.end())
                        //     //{
                        //     //     session_id = req.headers["Cookie"];
                        //     //     size_t pos = session_id.find("session_id=");
                        //     //     if (pos != std::string::npos)
                        //     //     {
                        //     //         size_t pos_end = session_id.find(";", pos);
                        //     //         session_id = session_id.substr(pos + 11, pos_end - (pos + 11));
                        //     //     }
                        //     // }
                        //     // Session *session;
                        //     // bool is_new;
                        //     // session = _session_cookie->get_session(session_id, is_new);
                        //     // if (is_new == true)
                        //     //     resp.headers["Set-Cookie"] = "session_id=" + session->_id + "; Path=/; HttpOnly";
                        //     //  cgi process request
                        //     if (req.is_cgi_request())
                        //     {
                        //         TRACE();
                        //         c->_cgi = new CGI_Process();
                        //         c->_cgi->execute("./www" + req.path, const_cast<HTTPRequest &>(req));
                        //         c->is_cgi = true;

                        //         _epoller->add_event(c->_cgi->_read_fd, EPOLLIN | EPOLLET);
                        //         _manager->bind_cgi_fd(c->_cgi->_read_fd, c->client_fd);
                        //         continue;
                        //     }
                        //     // c->is_keep_alive = req.keep_alive;
                        //     // keep-alive
                        //     bool ka = computeKeepAlive(req, resp.statusCode);
                        //     c->is_keep_alive = ka;
                        //     applyConnectionHeader(resp, ka);

                        //     c->write_buffer = ResponseBuilder::build(resp);
                        //     c->write_pos = 0;
                        // }
                        if (c->_state == PROCESS)
                        {
                            HTTPRequest req = c->parser.getRequest();
                            // 1) resolve effective config
                            if (_routing)
                            {
                                req.effective = _routing->resolve(req);
                                req.max_body_size = req.effective.max_body_size;
                                req.has_effective = true;
                            }
                            else
                            {
                                req.effective = _default_cfg;
                                req.has_effective = true;
                            }
                            // 2) 统一做 allowed_methods -> 405（避免每个 handler 重复写）
                            if (!isMethodAllowed(req.method, req.effective.allowed_methods))
                            {
                                HTTPResponse err = buildErrorResponse(405);
                                bool ka = computeKeepAlive(req, 405);
                                c->is_keep_alive = ka;
                                applyConnectionHeader(err, ka);
                                c->write_buffer = ResponseBuilder::build(err);
                                c->write_pos = 0;
                                c->_state = WRITING;
                                _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
                                continue;
                            }

                            // 3) CGI：用 cfg.root 拼真实路径
                            if (req.is_cgi_request())
                            {
                                c->_cgi = new CGI_Process();
                                std::string scriptPath = FileUtils::joinPath(req.effective.root, req.path);
                                c->_cgi->execute(scriptPath, req); 
                                c->is_cgi = true;

                                _epoller->add_event(c->_cgi->_read_fd, EPOLLIN | EPOLLET);
                                _manager->bind_cgi_fd(c->_cgi->_read_fd, c->client_fd);
                                continue;
                            }

                            // 4) 普通 handler
                            HTTPResponse resp = process_request(req);

                            bool ka = computeKeepAlive(req, resp.statusCode);
                            c->is_keep_alive = ka;
                            applyConnectionHeader(resp, ka);

                            c->write_buffer = ResponseBuilder::build(resp);
                            c->write_pos = 0;
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
                    TRACE();
                    if (do_write(*c))
                    {
                        if (c->is_keep_alive)
                        {
                            c->reset();
                            _epoller->modif_event(fd, EPOLLIN | EPOLLET);
                        }
                        else
                        {
                            _manager->remove_socket_client(fd);
                            _epoller->del_event(fd);
                            close(fd);
                        }
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
                            TRACE();
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

bool Server::load_config(const std::string& path)
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
    _default_cfg.root = _rt_servers[0].root;
    _default_cfg.index = _rt_servers[0].index;
    _default_cfg.autoindex = _rt_servers[0].autoindex;
    _default_cfg.allowed_methods = _rt_servers[0].allowed_methods;
    _default_cfg.error_pages = _rt_servers[0].error_page;
    
    _default_cfg.max_body_size = _rt_servers[0].client__max_body_size;
    return true;
}
