#include <iostream>
#include "Event/hpp/Server.hpp"
#include "HTTP/hpp/HTTPRequest.hpp"
#include "HTTP/hpp/HTTPResponse.hpp"
#include <csignal>
#include <cstdlib>
#include <ctime>

#define TRACE() std::cout << "[TRACE] " << __FILE__ << ":" << __LINE__ << std::endl;

class Server;
struct Client;
class HTTPRequest;

static void signal_handler(int)
{
    std::cout << "\n[Signal] shutdown\n";
    exit(0);
}

bool HTTPRequest::is_cgi_request() const
{
    if (path.find("/cgi-bin/") == 0)
        return true;
    // 2. 或者根据文件扩展名判断
    size_t dot = path.rfind('.');
    if (dot != std::string::npos)
    {
        std::string ext = path.substr(dot);
        if (ext == ".sh" || ext == ".py" || ext == ".php")
            return true;
    }

    // 3. 其他情况认为不是 CGI
    return false;
}

//void Server::handle_cgi_read(Client &c, int pipe_fd)
//{
//    char buf[4096];
//    while (true)
//    {
//        ssize_t n = read(pipe_fd, buf, sizeof(buf));
//        if (n > 0)
//        {
//            TRACE();
//            if (c.is_cgi)
//            {
//                TRACE();
//                c._cgi->append_output(buf, n);
//
//                c._cgi->last_output_ms = Client::now_ms();
//            }
//        }
//        else if (n == 0)
//        {
//            TRACE();
//            if (c.is_cgi && c._cgi->_pid > 0) // 子进程还在继续
//            {
//                pid_t r = waitpid(c._cgi->_pid, NULL, WNOHANG);
//                if (r > 0)
//                {
//                    TRACE();
//                    c._cgi->_pid = -1;
//                }
//            }
//            TRACE();
//
//            std::string cgi_out;
//            if (c._cgi)
//                cgi_out = c._cgi->_output_buffer;
//            // 3) 再清理 pipe（这一步会清 buffer，所以必须放在拷贝之后）
//            _epoller.del_event(pipe_fd);
//            _manager->del_cgi_fd(pipe_fd);
//
//            // 4) build response from copied output
//            TRACE();
//            HTTPResponse resp;
//            resp = resp.buildResponseFromCGIOutput(cgi_out, c.parser.getRequest().keep_alive);
//            bool ka = computeKeepAlive(c.parser.getRequest(), resp.statusCode);
//            c.is_keep_alive = ka;
//            applyConnectionHeader(resp, ka);
//            c.write_buffer = ResponseBuilder::build(resp);
//            c.write_pos = 0;
//            c._state = WRITING;
//            _epoller.modif_event(c.client_fd, EPOLLOUT | EPOLLET);
//            break;
//        }
//        else
//        {
//            if (errno == EAGAIN || errno == EWOULDBLOCK)
//                break;
//            handle_cgi_read_error(c, pipe_fd);
//            break;
//        }
//    }
//}
//
//#define Timeout 50 // 50-100
//
//void Server::run()
//{
//    set_non_block_fd(socketfd);
//    _epoller.add_event(socketfd, EPOLLIN | EPOLLET);
//    while (true)
//    {
//        int nfds = _epoller.wait(Timeout);
//        check_cgi_timeout();
//        check_timeout();
//        for (int i = 0; i < nfds; i++)
//        {
//            TRACE();
//            int fd = _epoller.get_event_fd(i);
//            uint32_t events = _epoller.get_event_type(i);
//            if (fd == socketfd)
//            {
//                handle_connection();
//                continue;
//            }
//            if (_manager->is_cgi_pipe(fd))
//            {
//                TRACE();
//                Client *c = _manager->get_client_by_cgi_fd(fd);
//                if (c && (events & (EPOLLIN | EPOLLHUP)))
//                {
//                    TRACE();
//                    handle_cgi_read(*c, fd);
//                    continue;
//                }
//                if (c && (events & (EPOLLERR | EPOLLRDHUP)))
//                {
//                    handle_pipe_error(fd);
//                    continue;
//                }
//            }
//            else // socket client
//            {
//                TRACE();
//                Client *c = _manager->get_socket_client_by_fd(fd);
//                if (!c)
//                    continue;
//                if (events & EPOLLIN)
//                {
//                    if (do_read(*c))
//                    {
//                        if (c->_state == PROCESS)
//                        {
//                            TRACE();
//                            HTTPRequest req = c->parser.getRequest();
//                            HTTPResponse resp = process_request(req);
//
//                            // session
//                            std::string session_id;
//                            // check if is session
//                            if (req.headers.find("Cookie") != req.headers.end())
//                            {
//                                session_id = req.headers["Cookie"];
//                                size_t pos = session_id.find("session_id=");
//                                if (pos != std::string::npos)
//                                {
//                                    size_t pos_end = session_id.find(";", pos);
//                                    session_id = session_id.substr(pos + 11, pos_end - (pos + 11));
//                                }
//                            }
//                            Session *session;
//                            bool is_new;
//                            session = _session_cookie->get_session(session_id, is_new);
//                            if (is_new == true)
//                                resp.headers["Set-Cookie"] = "session_id=" + session->_id + "; Path=/; HttpOnly";
//                            // cgi process request
//                            if (req.is_cgi_request())
//                            {
//                                TRACE();
//                                c->_cgi = new CGI_Process();
//                                c->_cgi->execute("./www" + req.path, const_cast<HTTPRequest &>(req));
//                                c->is_cgi = true;
//
//                                _epoller.add_event(c->_cgi->_read_fd, EPOLLIN | EPOLLET);
//                                _manager->bind_cgi_fd(c->_cgi->_read_fd, c->client_fd);
//                                continue;
//                            }
//                            // c->is_keep_alive = req.keep_alive;
//                            // keep-alive
//                            bool ka = computeKeepAlive(req, resp.statusCode);
//                            c->is_keep_alive = ka;
//                            applyConnectionHeader(resp, ka);
//
//                            c->write_buffer = ResponseBuilder::build(resp);
//                            c->write_pos = 0;
//                        }
//                        if (!c->is_cgi)
//                        {
//                            TRACE();
//                            c->_state = WRITING;
//                            _epoller.modif_event(fd, EPOLLOUT | EPOLLET);
//                        }
//                    }
//                }
//                if (events & EPOLLOUT)
//                {
//                    TRACE();
//                    if (do_write(*c))
//                    {
//                        if (c->is_keep_alive)
//                        {
//                            TRACE();
//                            c->reset();
//                            _epoller.modif_event(fd, EPOLLIN | EPOLLET);
//                        }
//                        else
//                        {
//                            _manager->remove_socket_client(fd);
//                            _epoller.del_event(fd);
//                            close(fd);
//                        }
//                    }
//                }
//                if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
//                {
//                    // std::cout << "errno: " << errno << std::endl;
//                    // // CGI pipe 的错误，直接走 pipe error（它会生成 500 并写回 client）
//                    // if (_manager->is_cgi_pipe(fd))
//                    // {
//                    //     TRACE();
//                    //     handle_pipe_error(fd);
//                    //     continue;
//                    // }
//                    // // client socket 的错误：如果已经有待发送响应，优先尝试发送，不要立刻 close 覆盖掉 504/500
//                    // Client *c = _manager->get_socket_client_by_fd(fd);
//                    if (c && c->_state == WRITING && !c->write_buffer.empty())
//                    {
//                        c->is_keep_alive = false;
//                        // 尝试直接写一次（即使没有 EPOLLOUT）
//                        if (do_write(*c))
//                        {
//                            TRACE();
//                            _manager->remove_socket_client(fd);
//                            _epoller.del_event(fd);
//                            close(fd);
//                            continue;
//                        }
//                        // 写到 EAGAIN -> 继续等 EPOLLOUT
//                        _epoller.modif_event(fd, EPOLLOUT | EPOLLET);
//                        continue;
//                    }
//                    // 没有待发送响应 -> 正常错误清理
//                    handle_socket_error(fd);
//                    continue;
//                }
//            }
//        }
//    }
//}

int main(void)
{
    int port = 8080;
   
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    try
    {
        Server server(port);
        server.init_sockets();
        std::cout << "Listening on http://localhost:" << port << std::endl;
        server.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
}

/**
 * test for timeout:
 *  curl -v http://localhost:8080/cgi-bin/filename
 *  in the cgi-bin, find all_sleep file, test timeout for long time no reply from the cgi timeout
 *  file partiel_pipe, test timeout with the pipe fonction
 *
 * need fuser -k 8080/tcp after every test
 *
 */