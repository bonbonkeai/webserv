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

void start_cgi(Client &c, std::string script_path)
{
    std::cout << "script_path in cgi: " << script_path << std::endl;
    int pipe_in[2];
    int pipe_out[2];

    pipe(pipe_in);
    pipe(pipe_out);

    pid_t pid = fork();
    if (pid == 0) // child
    {
        dup2(pipe_out[1], STDOUT_FILENO);
        dup2(pipe_in[0], STDIN_FILENO);

        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);

        char *argv[] = {(char *)script_path.c_str(), NULL};
        char *env[] = {(char *)"REQUEST_METHOD=GET", NULL};

        execve(script_path.c_str(), argv, env);
        TRACE();
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        _exit(1);
    }

    // parent
    close(pipe_in[0]);
    close(pipe_out[1]);

    TRACE();
    c._cgi = new CGI_Process();
    c._cgi->_pid = pid;
    c._cgi->_read_fd = pipe_out[0];
    c._cgi->_write_fd = pipe_in[1];
    c._cgi->start_time_ms = std::time(0); // ⭐ CGI timeout 起点
    c.is_cgi = true;
    c.last_active = std::time(0);

    TRACE(); // ⭐ client activity
}
//void Server::process_request(Client &c)
//{
//    const HTTPRequest &req = c.parser.getRequest();
//
//    if (req.is_cgi_request())
//    {
//        start_cgi(c, "./www" + req.path);
//        c._state = PROCESS;
//        set_non_block_fd(c._cgi->_read_fd);
//        set_non_block_fd(c._cgi->_write_fd);
//
//        //_epoller.modif_event(c.client_fd, EPOLLOUT | EPOLLET); 太早改编为out，但是却没有准备好
//        _epoller.add_event(c._cgi->_read_fd, EPOLLIN | EPOLLET);
//        _manager.bind_cgi_fd(c._cgi->_read_fd, c.client_fd);
//        TRACE();
//
//        // handle_cgi_read(c, c._cgi->_read_fd);
//    }
//    else
//    {
//        TRACE();
//        HTTPResponse resp;
//        resp.statusCode = 200;
//        resp.statusText = "OK";
//        resp.body = "Hello static";
//        resp.headers["content-length"] = toString(resp.body.size());
//        resp.headers["content-type"] = "text/plain";
//        resp.headers["connection"] = (req.keep_alive ? "keep-alive" : "close");
//
//        c.write_buffer = ResponseBuilder::build(resp);
//        c.write_pos = 0;
//        c._state = WRITING;
//        _epoller.modif_event(c.client_fd, EPOLLOUT | EPOLLET);
//    }
//}

int main(void)
{
    int port = 8090;
   
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