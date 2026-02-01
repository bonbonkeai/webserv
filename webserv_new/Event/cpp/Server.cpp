#include "Event/hpp/Server.hpp"
#include "Method_Handle/hpp/FileUtils.hpp"
#include "Method_Handle/hpp/RedirectHandle.hpp"
#include <cstring>
#include <sstream>
#include <sys/wait.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>

#define Timeout 50 // 50-100
#define ALL_TIMEOUT_MS 5000ULL
#define CGI_TIMEOUT_MS 10000ULL
#define MAX_ERROR_REDIRECTS 10

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

static bool endsWith(const std::string &s, const std::string &suffix)
{
    if (suffix.empty())
        return true;
    if (s.size() < suffix.size())
        return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static void splitUriQuery(const std::string &uri, std::string &path, std::string &query)
{
    std::size_t q = uri.find('?');
    if (q == std::string::npos)
    {
        path = uri;
        query.clear();
    }
    else
    {
        path = uri.substr(0, q);
        query = uri.substr(q + 1);
    }
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

Server::Server(const std::vector<ServerRuntimeConfig>& servers)
    : _servers(servers), _routing(_servers)
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
}

bool Server::init_sockets()
{
    if (!_epoller->init(128))
        return false;

    std::map<std::string, int> seen;
    for (size_t i = 0; i < _servers.size(); ++i)
    {
        const ServerRuntimeConfig& s = _servers[i];
        std::string bind_host = s.host.empty() ? "0.0.0.0" : s.host;
        std::ostringstream key;
        key << bind_host << ":" << s.port;
        if (seen.find(key.str()) != seen.end())
            continue;

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            throw std::runtime_error("Socket create failed");

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in serveraddr;
        std::memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(s.port);
        if (bind_host == "0.0.0.0")
        {
            serveraddr.sin_addr.s_addr = INADDR_ANY;
        }
        else
        {
            std::string ip = bind_host;
            if (bind_host == "localhost")
                ip = "127.0.0.1";
            if (inet_pton(AF_INET, ip.c_str(), &serveraddr.sin_addr) != 1)
            {
                struct addrinfo hints;
                std::memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_STREAM;
                struct addrinfo* res = 0;
                std::ostringstream port_ss;
                port_ss << s.port;
                int gai = getaddrinfo(bind_host.c_str(), port_ss.str().c_str(), &hints, &res);
                if (gai != 0 || !res)
                    throw std::runtime_error("Invalid listen host: " + bind_host);
                std::memcpy(&serveraddr, res->ai_addr, sizeof(serveraddr));
                freeaddrinfo(res);
            }
        }

        if (bind(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
            throw std::runtime_error("Socket bind failed");
        if (listen(fd, 256) < 0)
            throw std::runtime_error("Listen socket failed");

        set_non_block_fd(fd);
        _listen_fds.push_back(fd);
        _listen_fd_to_port[fd] = s.port;
        if (_default_max_body_by_port.find(s.port) == _default_max_body_by_port.end())
            _default_max_body_by_port[s.port] = s.client__max_body_size;
        seen[key.str()] = fd;
    }
    return true;
}

void Server::set_non_block_fd(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl get flags failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl set flags failed");
}

bool Server::is_listen_fd(int fd) const
{
    return _listen_fd_to_port.find(fd) != _listen_fd_to_port.end();
}

int Server::listen_port_for_fd(int fd) const
{
    std::map<int, int>::const_iterator it = _listen_fd_to_port.find(fd);
    if (it == _listen_fd_to_port.end())
        return 0;
    return it->second;
}

size_t Server::max_body_size_for_port(int port) const
{
    std::map<int, size_t>::const_iterator it = _default_max_body_by_port.find(port);
    if (it == _default_max_body_by_port.end())
        return 10 * 1024 * 1024;
    return it->second;
}

bool Server::handle_connection(int listen_fd)
{
    while (true)
    {
        struct sockaddr_in clientaddr;
        socklen_t client_len = sizeof(clientaddr);
        int connect_fd = accept(listen_fd, (struct sockaddr *)&clientaddr, &client_len);
        if (connect_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return true;
            return false;
        }
        Server::set_non_block_fd(connect_fd);
        _epoller->add_event(connect_fd, EPOLLIN | EPOLLET);
        _manager->add_socket_client(connect_fd, listen_port_for_fd(listen_fd)); // client state is read_line
        // 这里面更新时间没有必要，因为add socket client的时候，client construit的时候就已经有一个时间设置了
        // Client *c = _manager->get_socket_client_by_fd(connect_fd);
        // if (c)
        //     c->last_activity_ms = Client::now_ms();
        //
    }
    return true;
}

HTTPResponse Server::process_request(const HTTPRequest &req, const EffectiveConfig &cfg)
{
    IRequest *h = RequestFactory::create(req);
    HTTPResponse resp = h->handle(cfg);
    delete h;
    return resp;
}

bool Server::validate_request(const HTTPRequest &req, const EffectiveConfig &cfg, HTTPResponse &resp) const
{
    if (!cfg.allowed_methods.empty())
    {
        bool allowed = false;
        for (size_t i = 0; i < cfg.allowed_methods.size(); ++i)
        {
            if (cfg.allowed_methods[i] == req.method)
            {
                allowed = true;
                break;
            }
        }
        if (!allowed)
        {
            resp = buildErrorResponse(405);
            std::string allow;
            for (size_t i = 0; i < cfg.allowed_methods.size(); ++i)
            {
                if (i)
                    allow += ", ";
                allow += cfg.allowed_methods[i];
            }
            if (!allow.empty())
                resp.headers["allow"] = allow;
            return false;
        }
    }
    if (cfg.max_body_size > 0 && req.body.size() > cfg.max_body_size)
    {
        resp = buildErrorResponse(413);
        return false;
    }
    return true;
}

bool Server::handle_request_flow(Client &c, const HTTPRequest &req, const EffectiveConfig &cfg, HTTPResponse &resp, bool &started_cgi)
{
    started_cgi = false;
    if (!validate_request(req, cfg, resp))
        return true;
    if (cfg.has_return)
    {
        resp = RedirectHandle::buildRedirect(req, cfg.return_code, cfg.return_url);
        return true;
    }
    if (should_handle_cgi(req, cfg))
    {
        HTTPResponse err;
        if (!start_cgi(c, req, cfg, err))
        {
            resp = err;
            return true;
        }
        started_cgi = true;
        return true;
    }
    resp = process_request(req, cfg);
    return true;
}

bool Server::apply_error_page(Client &c, const HTTPRequest &req, const EffectiveConfig &cfg, int listen_port, HTTPResponse &resp, bool &started_cgi)
{
    started_cgi = false;
    if (resp.statusCode < 400)
        return false;

    if (c.error_redirects >= MAX_ERROR_REDIRECTS)
        return false;

    std::map<int, ErrorPageRule>::const_iterator it = cfg.error_pages.find(resp.statusCode);
    if (it == cfg.error_pages.end())
        return false;

    const ErrorPageRule &rule = it->second;
    std::string uri = rule.uri;
    if (uri.empty())
        return false;

    if (uri.compare(0, 7, "http://") == 0 || uri.compare(0, 8, "https://") == 0)
    {
        int code = rule.override_set && rule.override_code > 0 ? rule.override_code : 302;
        resp = RedirectHandle::buildRedirect(req, code, uri);
        return true;
    }

    c.error_redirects++;

    HTTPRequest err_req = req;
    err_req.uri = uri;
    splitUriQuery(uri, err_req.path, err_req.query);
    if (err_req.method != "GET" && err_req.method != "HEAD")
    {
        err_req.method = "GET";
        err_req.body.clear();
        err_req.has_body = false;
        err_req.has_content_length = false;
        err_req.contentLength = 0;
    }
    EffectiveConfig err_cfg = _routing.resolve(err_req, listen_port);
    HTTPResponse err_resp;
    bool err_cgi = false;
    handle_request_flow(c, err_req, err_cfg, err_resp, err_cgi);
    if (err_cgi)
    {
        started_cgi = true;
        return true;
    }

    int final_code = resp.statusCode;
    if (rule.override_set && rule.override_code > 0)
        final_code = rule.override_code;
    HTTPResponse tmp = buildErrorResponse(final_code);
    err_resp.statusCode = final_code;
    err_resp.statusText = tmp.statusText;
    resp = err_resp;
    return true;
}

bool Server::handle_ready_request(Client &c)
{
    HTTPRequest req = c.parser.getRequest();
    EffectiveConfig cfg = _routing.resolve(req, c.listen_port);
    HTTPResponse resp;
    bool started_cgi = false;
    handle_request_flow(c, req, cfg, resp, started_cgi);
    if (started_cgi)
        return true;

    bool ep_cgi = false;
    apply_error_page(c, req, cfg, c.listen_port, resp, ep_cgi);
    if (ep_cgi)
        return true;

    bool ka = computeKeepAlive(req, resp.statusCode);
    c.is_keep_alive = ka;
    applyConnectionHeader(resp, ka);
    c.write_buffer = ResponseBuilder::build(resp);
    c.write_pos = 0;
    c._state = WRITING;
    _epoller->modif_event(c.client_fd, EPOLLOUT | EPOLLET);
    return true;
}

bool Server::should_handle_cgi(const HTTPRequest &req, const EffectiveConfig &cfg) const
{
    if (!cfg.is_cgi)
        return false;
    if (cfg.cgi_exec.empty())
        return false;
    for (std::map<std::string, std::string>::const_iterator it = cfg.cgi_exec.begin();
         it != cfg.cgi_exec.end(); ++it)
    {
        const std::string &ext = it->first;
        if (ext.empty())
            continue;
        if (endsWith(req.path, ext))
            return true;
        if (ext[0] != '.' && endsWith(req.path, "." + ext))
            return true;
    }
    return false;
}

bool Server::start_cgi(Client &c, const HTTPRequest &req, const EffectiveConfig &cfg, HTTPResponse &err)
{
    if (!FileUtils::isSafePath(req.path))
    {
        err = buildErrorResponse(400);
        return false;
    }

    std::string root = cfg.root.empty() ? "./www" : cfg.root;
    std::string script_path = FileUtils::joinPath(root, req.path);
    if (!FileUtils::exists(script_path))
    {
        err = buildErrorResponse(404);
        return false;
    }
    if (FileUtils::isDirectory(script_path))
    {
        err = buildErrorResponse(403);
        return false;
    }

    std::string exec_path;
    for (std::map<std::string, std::string>::const_iterator it = cfg.cgi_exec.begin();
         it != cfg.cgi_exec.end(); ++it)
    {
        const std::string &ext = it->first;
        if (ext.empty())
            continue;
        if (endsWith(req.path, ext) || (ext[0] != '.' && endsWith(req.path, "." + ext)))
        {
            exec_path = it->second;
            break;
        }
    }

    c._cgi = new CGI_Process();
    if (!c._cgi->execute(script_path, exec_path, const_cast<HTTPRequest &>(req)))
    {
        delete c._cgi;
        c._cgi = NULL;
        err = buildErrorResponse(500);
        return false;
    }
    c.is_cgi = true;
    _epoller->add_event(c._cgi->_read_fd, EPOLLIN | EPOLLET);
    _manager->bind_cgi_fd(c._cgi->_read_fd, c.client_fd);
    return true;
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
            c.parser.setMaxBodySize(max_body_size_for_port(c.listen_port));
            bool ok = c.parser.dejaParse(std::string(tmp, n));
            if (!ok && c.parser.getRequest().bad_request)
            {
                c._state = ERROR;
                const HTTPRequest &req = c.parser.getRequest();
                int code = req.error_code > 0 ? req.error_code : 400;
                HTTPResponse err = buildErrorResponse(code);
                EffectiveConfig cfg = _routing.resolve(req, c.listen_port);
                bool ep_cgi = false;
                apply_error_page(c, req, cfg, c.listen_port, err, ep_cgi);
                if (ep_cgi)
                {
                    c._state = PROCESS;
                    return (true);
                }
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
    EffectiveConfig cfg = _routing.resolve(req, c.listen_port);
    bool ep_cgi = false;
    apply_error_page(c, req, cfg, c.listen_port, err, ep_cgi);
    if (ep_cgi)
    {
        c._state = PROCESS;
        return;
    }
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
    EffectiveConfig cfg = _routing.resolve(c->parser.getRequest(), c->listen_port);
    bool ep_cgi = false;
    apply_error_page(*c, c->parser.getRequest(), cfg, c->listen_port, err, ep_cgi);
    if (ep_cgi)
    {
        c->_state = PROCESS;
        return;
    }
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
            EffectiveConfig cfg = _routing.resolve(c->parser.getRequest(), c->listen_port);
            bool ep_cgi = false;
            apply_error_page(*c, c->parser.getRequest(), cfg, c->listen_port, err, ep_cgi);
            if (ep_cgi)
            {
                c->_state = PROCESS;
                continue;
            }
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
            if (!c->parser.hasReceivedData())
            {
                timed_out.push_back(c->client_fd);
                continue;
            }
            HTTPResponse err = buildErrorResponse(408);
            EffectiveConfig cfg = _routing.resolve(c->parser.getRequest(), c->listen_port);
            bool ep_cgi = false;
            apply_error_page(*c, c->parser.getRequest(), cfg, c->listen_port, err, ep_cgi);
            if (ep_cgi)
            {
                c->_state = PROCESS;
                continue;
            }
            err.headers["connection"] = "close";
            if (err.headers.find("content-length") == err.headers.end())
                err.headers["content-length"] = toString(err.body.size());
            c->is_keep_alive = false;
            c->write_buffer = ResponseBuilder::build(err);
            c->write_pos = 0;
            c->_state = WRITING;
            _epoller->modif_event(c->client_fd, EPOLLOUT | EPOLLET);
        }
    }
    for (size_t i = 0; i < timed_out.size(); ++i)
        close_client(timed_out[i]);
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
    for (size_t i = 0; i < _listen_fds.size(); ++i)
        _epoller->add_event(_listen_fds[i], EPOLLIN | EPOLLET);
    while (true)
    {
        int nfds = _epoller->wait(Timeout);
        check_cgi_timeout();
        check_timeout();
        for (int i = 0; i < nfds; i++)
        {
            int fd = _epoller->get_event_fd(i);
            uint32_t events = _epoller->get_event_type(i);
            if (is_listen_fd(fd))
            {
                handle_connection(fd);
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
                            handle_ready_request(*c);
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
                            if (c->parser.hasBufferedData())
                            {
                                c->parser.dejaParse("");
                                if (c->parser.getRequest().complet)
                                {
                                    c->_state = PROCESS;
                                    handle_ready_request(*c);
                                    continue;
                                }
                            }
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
