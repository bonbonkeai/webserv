#include "Event/hpp/Server.hpp"

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

#define TRACE() std::cout << "[] " << __FILE__ << ":" << __LINE__ << std::endl;

volatile sig_atomic_t Server::g_running = 1;

// --------------------
// keep-alive policy
// --------------------

static bool shouldCloseByStatus(int statusCode)
{
    if (statusCode == 400 || statusCode == 411 || statusCode == 413 || statusCode == 408 ||
        statusCode == 431 || statusCode == 414 || statusCode == 501)
        return true;
    return false;
}

static bool computeKeepAlive(const HTTPRequest &req, int statusCode)
{
    if (!req.keep_alive)
        return false;
    if (shouldCloseByStatus(statusCode))
        return false;
    return true;
}

static void applyConnectionHeader(HTTPResponse &resp, bool keepAlive)
{
    resp.headers["connection"] = keepAlive ? "keep-alive" : "close";
}

static bool isMethodAllowed(const std::string &m, const std::vector<std::string> &allow)
{
    for (size_t i = 0; i < allow.size(); ++i)
        if (allow[i] == m)
            return true;
    return false;
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
    if (socketfd >= 0)
    {
        if (_epoller)
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

    int yes = 1;
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

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
        set_non_block_fd(connect_fd);
        _epoller->add_event(connect_fd, EPOLLIN | EPOLLET);
        _manager->add_socket_client(connect_fd);
    }
    return true;
}

// --------------------
// Close helpers (important for avoiding segfault)
// --------------------

void Server::cleanup_client_cgi(Client* c)
{
    if (!c)
        return;
    CGI_Process* proc = c->_cgi;
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
    return resp;
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

                HTTPResponse err = buildErrorResponse(code);
                bool ka = computeKeepAlive(req, code);
                c.is_keep_alive = ka;
                applyConnectionHeader(err, ka);

                c.write_buffer = ResponseBuilder::build(err);
                c.write_pos = 0;
                return true; // ready to write error response
            }
            continue;
        }
        if (n == 0)
        {
            c._state = CLOSED;
            return false;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;

        c._state = ERROR;
        c.is_keep_alive = false;
        return false;
    }

    if (c.parser.getRequest().complet)
    {
        c._state = PROCESS;
        return true;
    }
    return false;
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
            return true;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return false;

        c.is_keep_alive = false;
        return true;
    }
    return true;
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
    std::vector<CGI_Process*> timed_out;
    std::vector<CGI_Process*>& procs = _cgi_manager.all_processes();
    for (size_t i = 0; i < procs.size(); ++i)
    {
        CGI_Process* proc = procs[i];
        if (!proc || !proc->is_running()) continue;
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
        CGI_Process* proc = timed_out[i];
        proc->_state = CGI_Process::TIMEOUT;
        finish_cgi_process(proc);
    }
}
// --------------------
// Build response
// --------------------

void Server::start_cgi_for_client(Client* c, const HTTPRequest& req)
{
    CGI_Process* proc = new CGI_Process();
    if (!proc->execute(req.effective, req, c))
    {
        delete proc;
        HTTPResponse err = buildErrorResponse(500);
        err.headers["connection"] = "close";
        c->is_keep_alive = false;
        c->write_buffer = ResponseBuilder::build(err);
        c->write_pos = 0;
        c->_state = WRITING;
        _epoller->modif_event(c->client_fd, EPOLLOUT | EPOLLET);
        return;
    }
    _cgi_manager.add_process(proc);
    if (proc->_read_fd >= 0)
        _epoller->add_event(proc->_read_fd, EPOLLIN | EPOLLET);
    if (proc->_write_fd >= 0 && req.method == "POST" && req.has_body)
        _epoller->add_event(proc->_write_fd, EPOLLOUT | EPOLLET);
    if (!(req.method == "POST" && req.has_body) && proc->_write_fd >= 0)
    {
        close(proc->_write_fd);
        proc->_write_fd = -1;
    }
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
        req.effective = _routing->resolve(req, port_nbr, req._rout);
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
        HTTPResponse err = buildErrorResponse(405);
        bool ka = computeKeepAlive(req, 405);
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
    // normal
    HTTPResponse resp = process_request(req);
    bool ka = computeKeepAlive(req, resp.statusCode);
    c.is_keep_alive = ka;
    applyConnectionHeader(resp, ka);

    c.write_buffer = ResponseBuilder::build(resp);
    c.write_pos = 0;
    c._state = WRITING;
    _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
    return true;
}

void Server::handle_cgi_event(int fd, uint32_t ev)
{
    CGI_Process* proc = _cgi_manager.get_process_by_fd(fd);
    if (!proc)
    {
        _epoller->del_event(fd);
        return;
    }
    // pipe 错误优先
    if (ev & (EPOLLERR | EPOLLRDHUP | EPOLLHUP))
    {
        // 先从 epoll 摘掉
        _epoller->del_event(fd);
        proc->_state = CGI_Process::ERROR;
        finish_cgi_process(proc);
        return;
    }
    Client* c = proc->client;
    if (!c)
    {
        // 没 client 了，直接回收
        if (proc->_read_fd >= 0) _epoller->del_event(proc->_read_fd);
        if (proc->_write_fd >= 0) _epoller->del_event(proc->_write_fd);
        _cgi_manager.kill_and_remove(proc);
        return;
    }
    // 写 stdin（POST body）
    if ((ev & EPOLLOUT) && proc->_write_fd == fd)
    {
        const HTTPRequest& req = c->parser.getRequest();
        bool done = proc->write_body(req.body);
        if (done)
            _epoller->del_event(fd); // 写端完成就不监听了
    }
    // 读 stdout
    if ((ev & EPOLLIN) && proc->_read_fd == fd)
    {
        for (;;)
        {
            std::size_t before = proc->_output_buffer.size();
            std::string tmp;
            bool ok = proc->read_output(tmp);
            if (!ok)
                break;
            if (proc->_output_buffer.size() == before)
                break;
        }
        if (proc->_read_fd < 0)
            _epoller->del_event(fd);
    }
    if (proc->_state == CGI_Process::FINISHED ||
        proc->_state == CGI_Process::ERROR ||
        proc->_state == CGI_Process::TIMEOUT)
    {
        finish_cgi_process(proc);
    }
}

void Server::finish_cgi_process(CGI_Process* proc)
{
    Client* c = proc->client;

    // 先从 epoll 摘掉两端（防幽灵事件）
    if (proc->_read_fd >= 0) 
        _epoller->del_event(proc->_read_fd);
    if (proc->_write_fd >= 0)
        _epoller->del_event(proc->_write_fd);
    if (!c)
    {
        proc->terminate();
        _cgi_manager.remove_and_delete(proc);
        return;
    }
    HTTPResponse resp;
   if (proc->_state == CGI_Process::TIMEOUT)
    {
        proc->terminate();
        resp = buildErrorResponse(504);
    }
    else if (proc->_state == CGI_Process::ERROR)
    {
        proc->terminate();
        resp = buildErrorResponse(500);
    }
    else
        resp = HTTPResponse::buildResponseFromCGIOutput(proc->_output_buffer, true);
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
// Main loop
// --------------------

void Server::run()
{
    set_non_block_fd(socketfd);
    _epoller->add_event(socketfd, EPOLLIN | EPOLLET);

    while (g_running)
    {
        int nfds = _epoller->wait(Timeout);

        check_cgi_timeout();
        check_timeout();
        for (int i = 0; i < nfds; ++i)
        {
            int fd = _epoller->get_event_fd(i);
            uint32_t ev = _epoller->get_event_type(i);
            // 1) listen
            if (fd == socketfd)
            {
                handle_connection();
                continue;
            }
            // 2) CGI fds
           if (_cgi_manager.is_cgi_fd(fd))
            {
                handle_cgi_event(fd, ev);
                continue;
            }
            // 3) socket client
            Client *c = _manager->get_socket_client_by_fd(fd);
            if (!c)
            {
                _epoller->del_event(fd);
                continue;
            }

            // 4) error first (critical for avoiding use-after-free)
            if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                handle_socket_error(fd);
                continue;
            }

            // 5) read
            if (ev & EPOLLIN)
            {
                bool ok = do_read(*c);
                if (!ok)
                {
                    if (c->_state == CLOSED || c->_state == ERROR)
                    {
                        close_client(fd);
                        continue;
                    }
                }
                else
                {
                    if (c->_state == PROCESS)
                    {
                        buildRespForCompletedReq(*c, fd);
                        continue;
                    }
                    if (c->_state == WRITING && !c->is_cgi)
                        _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
                }
            }

            // 6) write
            if (ev & EPOLLOUT)
            {
                if (!do_write(*c))
                    continue;

                if (!c->is_keep_alive)
                {
                    close_client(fd);
                    continue;
                }

                // keep-alive
                c->reset();

                // pipelined (best-effort)
                while (!c->is_cgi && c->parser.hasBufferedData())
                {
                    bool ok = c->parser.dejaParse(std::string());
                    if (!ok && c->parser.getRequest().bad_request)
                    {
                        const HTTPRequest &rq = c->parser.getRequest();
                        int code = rq.error_code > 0 ? rq.error_code : 400;

                        HTTPResponse err = buildErrorResponse(code);
                        bool ka2 = computeKeepAlive(rq, code);
                        c->is_keep_alive = ka2;
                        applyConnectionHeader(err, ka2);

                        c->write_buffer = ResponseBuilder::build(err);
                        c->write_pos = 0;
                        c->_state = WRITING;
                        _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
                        break;
                    }
                    if (!c->parser.getRequest().complet)
                        break;
                    c->_state = PROCESS;
                    buildRespForCompletedReq(*c, fd);
                    break;
                }

                if (!c->is_cgi && (c->_state != WRITING || c->write_buffer.empty()))
                    _epoller->modif_event(fd, EPOLLIN | EPOLLET);
                else
                    _epoller->modif_event(fd, EPOLLOUT | EPOLLET);
            }
        }
    }
}

// --------------------
// Compatibility stub
// (Server.hpp still declares this; keep it linked.)
// --------------------

void Server::finalize_cgi_response(Client &c, int pipe_fd)
{
    (void)c;
    (void)pipe_fd;
    // Your codebase moved CGI handling to CGIRequestHandle.
    // This function is intentionally left as a no-op.
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

    if (_rt_servers.empty())
        throw std::runtime_error("config: no server block found");

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

    return true;
}