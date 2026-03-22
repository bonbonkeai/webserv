#include "Event/hpp/new_Server.hpp"
#include <arpa/inet.h>  // inet_ntop
#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <sys/time.h>
#include <sys/wait.h>
#include <vector>

#define Timeout 50 // 50-100
#define ALL_TIMEOUT_MS 5000ULL
#define EXECUTION_TIMEOUT 10000ULL
#define START_TIMEOUT 5000ULL

#define TRACE() std::cout << "[] " << __FUNCTION__ << ":" << __LINE__ << std::endl;

volatile sig_atomic_t Server::g_running = 1;

// --------------------
// keep-alive policy
// --------------------

// static bool shouldCloseByStatus(int statusCode)
// {
//     if (statusCode == 400 || statusCode == 411 || statusCode == 413 || statusCode == 408 ||
//         statusCode == 431 || statusCode == 414 || statusCode == 501 || statusCode == 500)
//         return (true);
//     return (false);
// }
static bool shouldCloseByStatus(int statusCode)
{
    if (statusCode == 400 || statusCode == 403 || statusCode == 408 ||
        statusCode == 411 || statusCode == 413 || statusCode == 414 ||
        statusCode == 431 || statusCode == 500 || statusCode == 501)
        return true;
    return false;
}

static bool computeKeepAlive(const HTTPRequest &req, int statusCode)
{
    if (!req.keep_alive)
        return (false);
    if (shouldCloseByStatus(statusCode))
        return (false);
    return (true);
}

static void applyConnectionHeader(HTTPResponse &resp, bool keepAlive)
{
    resp.headers["connection"] = keepAlive ? "keep-alive" : "close";
}

static bool isMethodAllowed(const std::string &m, const std::vector<std::string> &allow)
{
    for (size_t i = 0; i < allow.size(); ++i)
        if (allow[i] == m)
            return (true);
    return (false);
}

// --------------------
// Server lifecycle
// --------------------

Server::Server(int port) : port_nbr(port), socketfd(-1), _routing(NULL)
{
    _epoller = new Epoller();
    _manager = new ClientManager();
    _session_cookie = new Session_manager();

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    // signal(SIGCHLD, SIG_DFL);
    signal(SIGCHLD, SIG_IGN);
    g_running = 1;
}

Server::~Server()
{
    cleanup();
}

void Server::signal_handler(int sig)
{
    (void)sig;
    std::cout << "\n[Signal] shutdown\n";
    g_running = 0;
}

void Server::cleanup()
{
    // close all socket clients
    if (_manager && _epoller)
    {
        std::map<int, Client *> clients = _manager->get_all_socket_clients();
        for (std::map<int, Client *>::iterator it = clients.begin(); it != clients.end(); ++it)
        {
            int fd = it->first;
            Client *c = it->second;
            if (c)
                cleanup_client_cgi(c);
            if (fd >= 0)
            {
                _epoller->del_event(fd);
                close(fd);
            }
        }
        _manager->clear_all_clients();
    }
    // if (socketfd >= 0)
    //{
    //     if (_epoller)
    //         _epoller->del_event(socketfd);
    //     close(socketfd);
    //     socketfd = -1;
    // }
    if (socketfd >= 0)
    {
        for (size_t i = 0; i < _listen_fds.size(); ++i)
        {
            _epoller->del_event(_listen_fds[i]);
            close(_listen_fds[i]);
        }
        _listen_fds.clear();
        _fd_to_port.clear();
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

// bool Server::init_sockets()
//{
//     socketfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (socketfd < 0)
//         throw std::runtime_error("Socket create failed");
//     int yes = 1;
//     setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
//     struct sockaddr_in serveraddr;
//     std::memset(&serveraddr, 0, sizeof(serveraddr));
//     serveraddr.sin_family = AF_INET;
//     serveraddr.sin_port = htons(port_nbr);
//     serveraddr.sin_addr.s_addr = INADDR_ANY;
//     if (bind(socketfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
//         throw std::runtime_error("Socket bind failed");
//     if (listen(socketfd, 256) < 0)
//         throw std::runtime_error("Listen socket failed");
//     return (_epoller->init(128));
// }

bool Server::init_sockets()
{
    std::set<int> ports;
    for (size_t i = 0; i < _rt_servers.size(); i++)
        ports.insert(_rt_servers[i].port);
    for (std::set<int>::iterator it = ports.begin(); it != ports.end(); ++it)
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            throw std::runtime_error("Socket create failed");
        int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        struct sockaddr_in serveraddr;
        std::memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(*it);
        serveraddr.sin_addr.s_addr = INADDR_ANY;
        if (bind(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
            throw std::runtime_error("Socket bind failed");
        if (listen(fd, 256) < 0)
            throw std::runtime_error("Listen socket failed");
        _listen_fds.push_back(fd);
        _fd_to_port[fd] = *it;
    }
    if (!_listen_fds.empty())
    {
        socketfd = _listen_fds[0];
        port_nbr = _fd_to_port[socketfd];
    }
    return (_epoller->init(128));
}

void Server::set_non_block_fd(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl get flags failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl set flags failed");
}

// bool Server::handle_connection()
// {
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
//         set_non_block_fd(connect_fd);
//         _epoller->add_event(connect_fd, EPOLLIN | EPOLLET);
//         _manager->add_socket_client(connect_fd);
//     }
//     return true;
// }
// bool Server::handle_connection()
//{
//    while (true)
//    {
//        struct sockaddr_in clientaddr;
//        socklen_t client_len = sizeof(clientaddr);
//        int connect_fd = accept(socketfd, (struct sockaddr *)&clientaddr, &client_len);
//        if (connect_fd < 0)
//        {
//            if (errno == EAGAIN || errno == EWOULDBLOCK)
//                return (true);
//            return (false);
//        }
//        set_non_block_fd(connect_fd);
//        _epoller->add_event(connect_fd, EPOLLIN | EPOLLET);
//        _manager->add_socket_client(connect_fd);
//        Client *c = _manager->get_socket_client_by_fd(connect_fd);
//        if (c)
//        {
//            char ip[INET_ADDRSTRLEN];
//            const char *p = inet_ntop(AF_INET, &clientaddr.sin_addr, ip, sizeof(ip));
//            c->remote_addr = (p ? std::string(ip) : std::string("")); // fallback empty if fail
//        }
//    }
//    return (true);
//}

// multi server
bool Server::handle_connection_on(int listen_fd, int port)
{
    while (true)
    {
        struct sockaddr_in clientaddr;
        socklen_t client_len = sizeof(clientaddr);
        int connect_fd = accept(listen_fd, (struct sockaddr *)&clientaddr, &client_len);
        if (connect_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return (true);
            return (false);
        }
        set_non_block_fd(connect_fd);
        _epoller->add_event(connect_fd, EPOLLIN | EPOLLET);
        _manager->add_socket_client(connect_fd);
        Client *c = _manager->get_socket_client_by_fd(connect_fd);
        if (c)
        {
            char ip[INET_ADDRSTRLEN];
            const char *p = inet_ntop(AF_INET, &clientaddr.sin_addr, ip, sizeof(ip));
            c->remote_addr = (p ? std::string(ip) : std::string("")); // fallback empty if fail
            c->port = port;
        }
    }
    return (true);
}

// --------------------
// Close helpers (important for avoiding segfault)
// --------------------
void Server::cleanup_client_cgi(Client *c)
{
    if (!c)
        return;
    CGI_Process *proc = c->_cgi;
    c->_cgi = NULL;
    c->is_cgi = false;
    if (!proc)
        return;
    if (proc->_read_fd >= 0)
        _epoller->del_event(proc->_read_fd);
    if (proc->_write_fd >= 0)
        _epoller->del_event(proc->_write_fd);
    _cgi_manager.kill_and_remove(proc);
}

void Server::close_client(int fd)
{
    Client *c = _manager->get_socket_client_by_fd(fd);
    if (c)
        cleanup_client_cgi(c);
    _epoller->del_event(fd);
    _manager->remove_socket_client(fd);
    close(fd);
}

void Server::handle_socket_error(int fd)
{
    // IMPORTANT: error events can arrive with EPOLLOUT/EPOLLIN together
    // so we must close+continue immediately to avoid using freed Client*
    close_client(fd);
}

// --------------------
// HTTP process
// --------------------

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
                c._state = WRITING;
                const HTTPRequest &req = c.parser.getRequest();
                int code = (req.error_code > 0) ? req.error_code : 400;
                // HTTPResponse err = buildErrorResponse(code);
                // HTTPResponse err = buildConfiguredErrorResponse(code, req.effective);
                HTTPResponse err = buildConfiguredErrorResponse(code, _default_cfg);
                bool ka = computeKeepAlive(req, code);
                c.is_keep_alive = ka;
                applyConnectionHeader(err, ka);
                c.write_buffer = ResponseBuilder::build(err);
                c.write_pos = 0;
                return (true);
            }
            // headers 已经结束，parser 正在等 body 时，先 resolve config 再检查 Content-Length
            const HTTPRequest &parsed = c.parser.getRequest();
            if (ok &&
                c.parser.isWaitingBody() &&
                !parsed.has_effective)
            {
                HTTPRequest tmpReq = parsed;
                if (_routing)
                {
                    tmpReq.effective = _routing->resolve(tmpReq, c.port, tmpReq._rout);
                    tmpReq.max_body_size = tmpReq.effective.max_body_size;
                    tmpReq.has_effective = true;
                    // std::cerr << "[DBG] path=" << tmpReq.path
                    //         << " cl=" << tmpReq.contentLength
                    //         << " limit=" << tmpReq.max_body_size
                    //         << " waitingBody=" << c.parser.isWaitingBody()
                    //         << std::endl;
                    if (tmpReq.has_content_length && tmpReq.contentLength > tmpReq.max_body_size)
                    {
                        // std::cerr << "[DBG] early 413 triggered" << std::endl;
                        // HTTPResponse err = buildErrorResponse(413);
                        HTTPResponse err = buildConfiguredErrorResponse(413, tmpReq.effective);
                        bool ka = computeKeepAlive(tmpReq, 413);
                        c.is_keep_alive = ka;
                        applyConnectionHeader(err, ka);
                        c.write_buffer = ResponseBuilder::build(err);
                        c.write_pos = 0;
                        c._state = WRITING;
                        return (true);
                    }
                }
            }
            continue;
        }
        if (n == 0)
        {
            c._state = CLOSED;
            return (false);
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;

        c._state = ERROR;
        c.is_keep_alive = false;
        return (false);
    }
    if (c.parser.getRequest().complet)
    {
        c._state = PROCESS;
        return (true);
    }
    return (false);
}

bool Server::do_write(Client &c)
{
    while (c.write_pos < c.write_buffer.size())
    {
        ssize_t n = send(c.get_fd(),
                         c.write_buffer.data() + c.write_pos,
                         c.write_buffer.size() - c.write_pos,
                         0);
        if (n > 0)
        {
            c.write_pos += static_cast<size_t>(n);
            continue;
        }
        if (n == 0)
        {
            c.is_keep_alive = false;
            return (true);
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return (false);

        c.is_keep_alive = false;
        return (true);
    }
    return (true);
}

// --------------------
// Timeouts
// --------------------

void Server::check_timeout()
{
    if (!_manager)
        return;
    unsigned long long now = Client::now_ms();
    std::vector<int> timed_out;
    std::vector<int> close_idle;
    std::map<int, Client *> &clients = _manager->get_all_socket_clients();
    for (std::map<int, Client *>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        Client *c = it->second;
        if (!c)
            continue;
        if (c->_state == READING &&
            !c->is_cgi &&
            !c->parser.getRequest().complet &&
            c->is_timeout(now, ALL_TIMEOUT_MS))
        {
        //     timed_out.push_back(it->first);
              // 空闲 keep-alive，静默关闭
            if (!c->parser.hasBufferedData())
            {
                close_idle.push_back(it->first);
            }
            else
            {
                // 真正读到一半超时，发 408/400
                timed_out.push_back(it->first);
            }
        }
    }
    for (size_t i = 0; i < close_idle.size(); ++i)
        close_client(close_idle[i]);
    for (size_t i = 0; i < timed_out.size(); ++i)
    {
        int fd = timed_out[i];
        Client *c = _manager->get_socket_client_by_fd(fd);
        if (!c)
            continue;
        const HTTPRequest &req = c->parser.getRequest();
        int code = 408;
        // incomplete chunked body 超时，按坏请求处理
        if (!req.complet && req.chunked)
            code = 400;
        // HTTPResponse err = buildErrorResponse(code);
        // HTTPResponse err = buildConfiguredErrorResponse(code, req.effective);
        HTTPResponse err = buildConfiguredErrorResponse(code, _default_cfg);
        err.headers["connection"] = "close";
        if (err.headers.find("content-length") == err.headers.end())
            err.headers["content-length"] = toString(err.body.size());
        c->is_keep_alive = false;
        c->write_buffer = ResponseBuilder::build(err);
        c->write_pos = 0;
        c->_state = WRITING;
        _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
    }
}

void Server::check_cgi_timeout()
{
    unsigned long long now = Client::now_ms();
    std::vector<CGI_Process *> timed_out;
    std::vector<CGI_Process *> &procs = _cgi_manager.all_processes();
    for (size_t i = 0; i < procs.size(); ++i)
    {
        CGI_Process *proc = procs[i];
        if (!proc || !proc->is_running())
            continue;
        unsigned long long diff = now - proc->start_time_ms;
        bool timeout = false;
        if (!proc->has_output && diff > START_TIMEOUT)
            timeout = true;
        else if (proc->has_output && (now - proc->last_output_ms) > EXECUTION_TIMEOUT)
            timeout = true;
        else if (diff > EXECUTION_TIMEOUT * 2)
            timeout = true;
        if (timeout)
        {
            proc->_state = CGI_Process::TIMEOUT;
            timed_out.push_back(proc);
        }
    }
    for (size_t i = 0; i < timed_out.size(); ++i)
    {
        CGI_Process *proc = timed_out[i];
        proc->_state = CGI_Process::TIMEOUT;
        finish_cgi_process(proc);
    }
}
// --------------------
// Build response
// --------------------

void Server::start_cgi_for_client(Client *c, const HTTPRequest &req)
{
    CGI_Process *proc = new CGI_Process();
    if (!proc->execute(req.effective, req, c))
    {
        int code = proc->_error_code;
        delete proc;
        // HTTPResponse err = buildErrorResponse(code);
        HTTPResponse err = buildConfiguredErrorResponse(code, req.effective);
        err.headers["connection"] = "close";
        c->is_keep_alive = false;
        c->write_buffer = ResponseBuilder::build(err);
        c->write_pos = 0;
        c->_state = WRITING;
        _epoller->modif_event(c->client_fd, EPOLLOUT | EPOLLET);
        return;
    }

    if (!(req.method == "POST" && req.has_body) && proc->_write_fd >= 0)
    {
        close(proc->_write_fd);
        proc->_write_fd = -1;
    }

    _cgi_manager.add_process(proc);
    if (proc->_read_fd >= 0)
        _epoller->add_event(proc->_read_fd, EPOLLIN | EPOLLET);
    if (proc->_write_fd >= 0 && req.method == "POST" && req.has_body)
        _epoller->add_event(proc->_write_fd, EPOLLOUT | EPOLLET);

    c->_cgi = proc;
    c->is_cgi = true;
    c->_state = CGI_RUNNING;
}

bool Server::buildRespForCompletedReq(Client &c, int fd)
{
    HTTPRequest req = c.parser.getRequest();
    // resolve effective config
    if (_routing)
    {
        req.effective = _routing->resolve(req, c.port, req._rout);
        std::cerr << "[DBG] req.path=" << req.path
                  << " rout.action=" << req._rout.action
                  << " fs_path=" << req._rout.fs_path
                  << std::endl;
        std::cerr << "[DBG] effective.error_pages.size=" << req.effective.error_pages.size() << std::endl;
        for (std::map<int, ErrorPageRule>::const_iterator it = req.effective.error_pages.begin();
            it != req.effective.error_pages.end(); ++it)
        {
            std::cerr << "[DBG] eff code=" << it->first
                    << " uri=" << it->second.uri << std::endl;
        }
        req.max_body_size = req.effective.max_body_size;
        req.has_effective = true;
    }
    else
    {
        req.effective = _default_cfg;
        req.has_effective = true;
    }
    // 405
    if (!isMethodAllowed(req.method, req.effective.allowed_methods))
    {
        // HTTPResponse err = buildErrorResponse(405);
        HTTPResponse err = buildConfiguredErrorResponse(405, req.effective);
        bool ka = computeKeepAlive(req, 405);
        c.is_keep_alive = ka;
        applyConnectionHeader(err, ka);
        c.write_buffer = ResponseBuilder::build(err);
        c.write_pos = 0;
        c._state = WRITING;
        _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
        return (true);
    }
    // 413 body size
    if (req.has_body && req.body.size() > req.effective.max_body_size)
    {
        // HTTPResponse err = buildErrorResponse(413);
        HTTPResponse err = buildConfiguredErrorResponse(413, req.effective);
        bool ka = computeKeepAlive(req, 413);
        c.is_keep_alive = ka;
        applyConnectionHeader(err, ka);
        c.write_buffer = ResponseBuilder::build(err);
        c.write_pos = 0;
        c._state = WRITING;
        _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
        return true;
    }

    // CGI
    if (req._rout.action == ACTION_CGI)
    {
        start_cgi_for_client(&c, req);
        return true;
    }
    if (req.effective.forbidden)
    {
        // HTTPResponse err = buildErrorResponse(403);
        HTTPResponse err = buildConfiguredErrorResponse(403, req.effective);
        bool ka = computeKeepAlive(req, 403);
        c.is_keep_alive = ka;
        applyConnectionHeader(err, ka);
        c.write_buffer = ResponseBuilder::build(err);
        c.write_pos = 0;
        c._state = WRITING;
        _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
        return true;
    }
    // normal
    HTTPResponse resp = process_request(req);
    // 读请求里的 cookie
    std::string session_id;
    std::map<std::string, std::string>::const_iterator it = req.headers.find("cookie");
    if (it != req.headers.end())
    {
        // 解析 "session_id=XXXX"
        std::string cookie = it->second;
        std::size_t pos = cookie.find("session_id=");
        if (pos != std::string::npos)
            session_id = cookie.substr(pos + 11, 16);
    }

    bool is_new = false;
    Session *sess = _session_cookie->get_session(session_id, is_new);

    // 如果是新 session，在响应里加 Set-Cookie
    if (is_new)
        resp.headers["set-cookie"] = "session_id=" + sess->_id + "; Path=/; HttpOnly";

    bool ka = computeKeepAlive(req, resp.statusCode);
    c.is_keep_alive = ka;
    applyConnectionHeader(resp, ka);
    c.write_buffer = ResponseBuilder::build(resp);
    c.write_pos = 0;
    c._state = WRITING;
    _epoller->modif_event(fd, EPOLLOUT | EPOLLET);


    return (true);
}

void Server::handle_cgi_event(int fd, uint32_t ev)
{
    CGI_Process *proc = _cgi_manager.get_process_by_fd(fd);
    if (!proc)
    {
        _epoller->del_event(fd);
        return;
    }
    // pipe 错误优先
    // if (ev & (EPOLLERR | EPOLLRDHUP | EPOLLHUP))
    // {
    //     // 先从 epoll 摘掉
    //     _epoller->del_event(fd);
    //     proc->_state = CGI_Process::ERROR;
    //     finish_cgi_process(proc);
    //     return;
    // }
    // pipe: only EPOLLERR is a hard error
    if (ev & EPOLLERR)
    {
        _epoller->del_event(fd);
        proc->_state = CGI_Process::ERROR;
        finish_cgi_process(proc);
        return;
    }
    Client *c = proc->client;
    if (!c)
    {
        // 没 client 了，直接回收
        if (proc->_read_fd >= 0)
            _epoller->del_event(proc->_read_fd);
        if (proc->_write_fd >= 0)
            _epoller->del_event(proc->_write_fd);
        _cgi_manager.kill_and_remove(proc);
        return;
    }
    // 写 stdin（POST body）
    if ((ev & EPOLLOUT) && proc->_write_fd == fd)
    {
        const HTTPRequest &req = c->parser.getRequest();
        bool done = proc->write_body(req.body);
        if (done)
            _epoller->del_event(fd); // 写端完成就不监听了
    }
    if (!_cgi_manager.is_known(proc))
        return;
    // 读 stdout
    // if ((ev & EPOLLIN) && proc->_read_fd == fd)
    // {
    //     for (;;)
    //     {
    //         std::size_t before = proc->_output_buffer.size();
    //         std::string tmp;
    //         bool ok = proc->read_output(tmp);
    //         if (!ok)
    //             break;
    //         if (proc->_output_buffer.size() == before)
    //             break;
    //     }
    //     if (proc->_read_fd < 0)
    //         _epoller->del_event(fd);
    // }
    if (proc->_read_fd == fd)
    {
        for (;;)
        {
            std::string tmp;
            bool ok = proc->read_output(tmp);
            if (!ok)
                break;
            if (tmp.empty())
                break;
        }

        if (proc->_read_fd < 0)
            _epoller->del_event(fd);
    }
    if (!_cgi_manager.is_known(proc))
        return;
    if (proc->_state == CGI_Process::FINISHED ||
        proc->_state == CGI_Process::ERROR ||
        proc->_state == CGI_Process::TIMEOUT)
    {
        finish_cgi_process(proc);
        return;
    }
}

void Server::finish_cgi_process(CGI_Process *proc)
{
    if (!_cgi_manager.is_known(proc))
        return;
    Client *c = proc->client;
    const HTTPRequest &req = c->parser.getRequest();
    if (proc->_read_fd >= 0)
    {
        _epoller->del_event(proc->_read_fd);
        _cgi_manager.unregiste_fd(proc->_read_fd);
        proc->_read_fd = -1;
    }
    if (proc->_write_fd >= 0)
    {
        _epoller->del_event(proc->_write_fd);
        _cgi_manager.unregiste_fd(proc->_write_fd);
        proc->_write_fd = -1;
    }

    proc->terminate();
    if (c)
    {
        c->_cgi = NULL;
        c->is_cgi = false;
    }
    if (!c)
    {
        _cgi_manager.remove_and_delete(proc);
        return;
    }
    HTTPResponse resp;
    if (proc->_state == CGI_Process::TIMEOUT)
    {
        proc->terminate();
        // resp = buildErrorResponse(504);
        resp = buildConfiguredErrorResponse(504, req.effective);
    }
    else if (proc->_state == CGI_Process::ERROR)
    {
        proc->terminate();
        // resp = buildErrorResponse(500);
        resp = buildConfiguredErrorResponse(500, req.effective);
    }
    else
        resp = resp.buildResponseFromCGIOutput(proc->_output_buffer, true);
    bool ka = computeKeepAlive(c->parser.getRequest(), resp.statusCode);
    c->is_keep_alive = ka;
    applyConnectionHeader(resp, ka);
    c->write_buffer = ResponseBuilder::build(resp);
    c->write_pos = 0;
    c->_state = WRITING;
    c->is_cgi = false;
    c->_cgi = NULL;
    _epoller->modif_event(c->client_fd, EPOLLOUT | EPOLLET);
    _cgi_manager.remove_and_delete(proc); // 唯一 delete 发生处
}
// --------------------
// Run helpers
// --------------------

void Server::run_init_listeners()
{
    for (size_t i = 0; i < _listen_fds.size(); ++i)
    {
        set_non_block_fd(_listen_fds[i]);
        _epoller->add_event(_listen_fds[i], EPOLLIN | EPOLLET);
    }
}

void Server::run_process_keep_alive_pipeline(Client &c, int fd)
{
    while (!c.is_cgi && c.parser.hasBufferedData())
    {
        bool ok = c.parser.dejaParse(std::string());
        if (!ok && c.parser.getRequest().bad_request)
        {
            const HTTPRequest &rq = c.parser.getRequest();
            int code = rq.error_code > 0 ? rq.error_code : 400;
            // HTTPResponse err = buildErrorResponse(code);
            // HTTPResponse err = buildConfiguredErrorResponse(code, rq.effective);
            HTTPResponse err = buildConfiguredErrorResponse(code, _default_cfg);
            bool ka2 = computeKeepAlive(rq, code);
            c.is_keep_alive = ka2;
            applyConnectionHeader(err, ka2);
            c.write_buffer = ResponseBuilder::build(err);
            c.write_pos = 0;
            c._state = WRITING;
            _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
            return;
        }
        if (!c.parser.getRequest().complet)
            return;

        c._state = PROCESS;
        buildRespForCompletedReq(c, fd);
        return;
    }
}

void Server::run_handle_read(Client &c, int fd)
{
    bool ok = do_read(c);
    if (!ok)
    {
        if (c._state == CLOSED || c._state == ERROR)
            close_client(fd);
        return;
    }

    if (c._state == PROCESS)
    {
        buildRespForCompletedReq(c, fd);
        return;
    }

    if (c._state == WRITING && !c.is_cgi)
        _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
}

void Server::run_handle_write(Client &c, int fd)
{
    if (!do_write(c))
        return;

    if (!c.is_keep_alive)
    {
        close_client(fd);
        return;
    }

    c.reset();
    run_process_keep_alive_pipeline(c, fd);

    if (!c.is_cgi && (c._state != WRITING || c.write_buffer.empty()))
        _epoller->modif_event(fd, EPOLLIN | EPOLLET);
    else
        _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
}

void Server::run_handle_event(int fd, uint32_t ev)
{
    if (_fd_to_port.count(fd))
    {
        handle_connection_on(fd, _fd_to_port[fd]);
        return;
    }

    if (_cgi_manager.is_cgi_fd(fd))
    {
        handle_cgi_event(fd, ev);
        return;
    }

    Client *c = _manager->get_socket_client_by_fd(fd);
    if (!c)
    {
        _epoller->del_event(fd);
        return;
    }

    if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
    {
        handle_socket_error(fd);
        return;
    }

    if (ev & EPOLLIN)
        run_handle_read(*c, fd);
    if (ev & EPOLLOUT)
        run_handle_write(*c, fd);
}

// --------------------
// Main loop
// --------------------

void Server::run()
{
    run_init_listeners();
    while (g_running)
    {
        int nfds = _epoller->wait(Timeout);
        check_cgi_timeout();
        check_timeout();
        for (int i = 0; i < nfds; ++i)
        {
            int fd = _epoller->get_event_fd(i);
            uint32_t ev = _epoller->get_event_type(i);
            run_handle_event(fd, ev);
        }
    }
}






// --------------------
// Compatibility stub
// (Server.hpp still declares this; keep it linked.)
// --------------------

// void Server::finalize_cgi_response(Client &c, int pipe_fd)
// {
//     (void)c;
//     (void)pipe_fd;
//     // Your codebase moved CGI handling to CGIRequestHandle.
//     // This function is intentionally left as a no-op.
// }
void Server::valide_server_names()
{
    std::map<int, std::set<std::string> > port_to_names;

    for (size_t i = 0; i < _rt_servers.size(); ++i)
    {
        const ServerRuntimeConfig &srv = _rt_servers[i];
        int port = srv.port;
        const std::string &name = srv.server_name;

        // 空 server_name 是允许的（作为 default server）
        if (name.empty())
            continue;

        // 检查这个端口上是否已经有同名 server
        if (port_to_names[port].count(name))
        {
            throw std::runtime_error(
                "Duplicate server_name '" + name +
                "' on port " + toString(port));
        }

        port_to_names[port].insert(name);
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
    if (_rt_servers.empty())
        throw std::runtime_error("config: no server block found");
    valide_server_names();

    if (_routing)
    {
        delete _routing;
        _routing = NULL;
    }

    _routing = new Routing(_rt_servers);

    // default cfg: used as fallback before host/route is resolved
    const ServerRuntimeConfig &first = _rt_servers[0];
    _default_cfg.server_port = first.port;
    _default_cfg.server_name = first.server_name;
    _default_cfg.root = first.root;
    _default_cfg.index = first.index;
    _default_cfg.autoindex = first.autoindex;
    _default_cfg.allowed_methods = first.allowed_methods;
    _default_cfg.error_pages = first.error_page;
    _default_cfg.max_body_size = first.client__max_body_size;

    // safe defaults
    _default_cfg.alias = "";
    _default_cfg.location_path = "";
    _default_cfg.has_return = false;
    _default_cfg.return_code = 302;
    _default_cfg.return_url = "";
    _default_cfg.is_cgi = false;
    _default_cfg.upload_path = "";
    return (true);
}