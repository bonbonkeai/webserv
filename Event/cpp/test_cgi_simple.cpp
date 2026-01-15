#include <iostream>
#include "Event/hpp/Client.hpp"
#include "Event/hpp/Server.hpp"
#include "HTTP/hpp/HTTPRequest.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>

class Server;
struct Client;
class HTTPRequest;

void spawn_cgi(Client &c, const std::string &script_path)
{
    int pipe_out[2];
    int pipe_in[2];

    pipe(pipe_out); // CGI stdout
    pipe(pipe_in);  // CGI stdin

    pid_t pid = fork();
    if (pid == 0)
    {
        // child
        close(pipe_out[0]);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_out[1]);

        close(pipe_in[1]);
        dup2(pipe_in[0], STDIN_FILENO);
        close(pipe_in[0]);

        // env minimal
        char *env[] = {(char *)"REQUEST_METHOD=GET", (char *)NULL};
        char *argv[] = {(char *)script_path.c_str(), NULL};

        execve(script_path.c_str(), argv, env);
        perror("execve");
        _exit(1);
    }
    else
    {
        // parent
        close(pipe_out[1]);
        close(pipe_in[0]);
        //for the test only
        int status;
        waitpid(pid, &status, 0); // 阻塞等 CGI 完
        char buf[4096];
        ssize_t n = read(pipe_out[0], buf, sizeof(buf));
        std::string output(buf, n);

        c.write_buffer = "HTTP/1.1 200 OK\r\nContent-Length: " + toString(output.size()) + "\r\n\r\n" + output;
        c.write_pos = 0;
        c._state = WRITING;
        //
        c._cgi = new CGI_Process();
        c._cgi->set_pid(pid);
        c._cgi->set_read_fd(pipe_out[0]);
        c._cgi->set_write_fd(pipe_in[1]);
        c.is_cgi = true;
    }
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
// prcess_request: version only for the test
void Server::process_request(Client &c)
{
    const HTTPRequest &req = c.parser.getRequest();

    if (req.is_cgi_request())
    {
        // CGI 处理
        spawn_cgi(c, "./www" + req.path); // 最小测试用
        c._state = WRITING;               // 输出通过 pipe 非阻塞写
        _epoller.modif_event(c.client_fd, EPOLLOUT | EPOLLET);
        _epoller.add_event(c._cgi->get_read_fd(), EPOLLIN | EPOLLET);
        _manager.bind_cgi_fd(c._cgi->get_read_fd(), c.client_fd);
    }
    else
    {
        // 静态文件 / 普通 response
        HTTPResponse resp;
        resp.statusCode = 200;
        resp.statusText = "OK";
        resp.body = "Hello static";
        resp.headers["content-length"] = toString(resp.body.size());
        resp.headers["content-type"] = "text/plain";
        resp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");

        c.write_buffer = ResponseBuilder::build(resp);
        c.write_pos = 0;
        c._state = WRITING;
        _epoller.modif_event(c.client_fd, EPOLLOUT | EPOLLET);
    }
}
#include <iostream>
#include "Event/hpp/Server.hpp"
#include <csignal>
#include <cstdlib>

static void signal_handler(int)
{
    std::cout << "\n[Signal] shutdown\n";
    exit(0);
}

int main(int ac, char **av)
{
    int port = 8080;
    if (ac == 2)
        port = std::atoi(av[1]);

    if (port <= 0 || port > 65535)
    {
        std::cerr << "Invalid port\n";
        return 1;
    }
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
 * lsof -i :端口号   检查端口是否被占用
 *   fuser -k 8080/tcp 清空端口号连接
 */