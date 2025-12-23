#include "Event/hpp/Server.hpp"
#include <cstring>

#define Timeout 50 // 50-100

//这里端口-1，然后 htons(port_nbr)。这可能会导致 bind 失败（或者绑定到不可预期端口），服务器根本起不来。或许可以先给一个固定值（比如 8080）？
Server::Server() : port_nbr(-1), socketfd(-1)
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
        _manager.add_client(connect_fd); // client state is read_line
    }
    return true;
}

//暂时还不做业务 handle，所以 process_request 的最小实现就返回一个固定 body，同时把 keep-alive 从 request 带过来。
static HTTPResponse process_request(const HTTPRequest& req)
{
    if (req.bad_request)
        return buildErrorResponse(400);

    HTTPResponse resp;
    resp.statusCode = 200;
    resp.statusText = "OK";
    resp.body = "OK\n";
    resp.headers["content-type"] = "text/plain; charset=utf-8";
    resp.headers["content-length"] = toString(resp.body.size());
    resp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
    return (resp);
}

bool Server::do_read(Client &c)
{
    char tmp[4096];
    bool is_reading = true;

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
                HTTPResponse err = buildErrorResponse(400);//暂时先放400，后续可以根据具体的error_code来修改
                //HTTPResponse err = buildErrorResponse(c.parser.getRequest().error_code);
                c.is_keep_alive = false;
                c.write_buffer = ResponseBuilder::build(err);
                c.write_pos = 0;
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
    // while (is_reading)
    // {
    //     if (c._state == RD_LINE)
    //         is_reading = read_request_line(c);
    //     else if (c._state == RD_HEADER)
    //         is_reading = read_request_header(c);
    //     else if (c._state == RD_BODY)
    //         is_reading = read_request_body(c);
    //     else
    //         is_reading = false;
    // }
    // return (c._state == RD_DONE);
}

void Server::run()
{
    set_non_block_fd(socketfd);
    _epoller.add_event(socketfd, EPOLLIN | EPOLLET);
    while (true)
    {
        int nfds = _epoller.wait(Timeout);
        for (int i = 0; i < nfds; i++)
        {
            int fd = _epoller.get_event_fd(i);
            uint32_t events = _epoller.get_event_type(i);
            if (fd == socketfd)
            {
                handle_connection();
                continue;
            }
            //这里还不确定，因为可能还存在同一轮里你先读/写了，再关
            if (fd != socketfd && (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)))
            {
                _epoller.del_event(fd);
                _manager.remove_client(fd);
                close(fd);
                continue;
            }
            // else if (events & EPOLLIN) // read fd
            // {
            //     Client &c = _manager.get_client(fd);
            //     if (do_read(c))
            //     {
            //         c._state = PROCESS;
            //         process_request(c);
            //         _epoller.modif_event(fd, EPOLLOUT | EPOLLET);
            //     }
            // }
            // else if (events & EPOLLOUT) // write fd
            // {
            //     Client &c = _manager.get_client(fd);
            //     c._state = WRITING;
            //     if (do_write(c))
            //     {
            //         if (c.is_keep_alive)
            //         {
            //             c.reset();
            //             _epoller.modif_event(fd, EPOLLIN | EPOLLET);
            //         }
            //         else
            //         {
            //             _manager.remove_client(fd);
            //             _epoller.del_event(fd);
            //             close(fd);
            //         }
            //     }
            // }
            //用两个if,因为同一轮 events 可能同时包含 IN 和 OUT，用 else if会漏处理其中一个
            if (events & EPOLLIN)
            {
                Client &c = _manager.get_client(fd);
                if (do_read(c))
                {
                    if (c._state == PROCESS)
                    {
                        HTTPResponse resp = process_request(c.parser.getRequest());
                        c.is_keep_alive = c.parser.getRequest().keep_alive;
                        c.write_buffer = ResponseBuilder::build(resp);
                        c.write_pos = 0;
                    }
                    c._state = WRITING;
                    _epoller.modif_event(fd, EPOLLOUT | EPOLLET);
                }
            }
            if (events & EPOLLOUT)
            {
                Client &c = _manager.get_client(fd);
                if (do_write(c))
                {
                    if (c.is_keep_alive)
                    {
                        c.reset();
                        _epoller.modif_event(fd, EPOLLIN | EPOLLET);
                    }
                    else
                    {
                        _manager.remove_client(fd);
                        _epoller.del_event(fd);
                        close(fd);
                    }
                }
            }
            // else if () // error?
            // {
            //     _epoller.del_event(fd);
            //     close(fd);
            // }
        }
    }
}

bool Server::do_write(Client &c)
{
    while (c.write_pos < c.write_buffer.size())
    {
        ssize_t n = send(c.get_fd(), c.write_buffer.data() + c.write_pos, c.write_buffer.size() - c.write_pos, 0);
        if (n > 0) c.write_pos += n;
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return (false);
            c._state = ERROR;
            return (true); //让上层走 close
        }
    }
    return (true);//写完
}
