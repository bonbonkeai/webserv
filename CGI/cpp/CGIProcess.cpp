#include "../hpp/CGIProcess.hpp"
#include "Event/hpp/Client.hpp"
#include <stdio.h>

CGI_Process::CGI_Process() : _pid(-1), _read_fd(-1), _write_fd(-1), _output_buffer(), start_time_ms(0),
                             last_output_ms(0)
{
}

CGI_Process::~CGI_Process()
{
    reset();
}

std::string CGI_Process::format_header_key(const std::string &key)
{
    std::string resultat = "HTTP_";
    for (size_t i = 0; i < key.size(); i++)
    {
        if (key[i] == '-')
            resultat += '_';
        else
            resultat += (char)std::toupper(key[i]);
    }
    return resultat;
}

CGI_ENV CGI_Process::get_env_from_request(HTTPRequest &req)
{
    CGI_ENV env;

    env.env_str.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.env_str.push_back("REQUEST_METHOD=" + req.method);
    env.env_str.push_back("SERVER_PROTOCOL=HTTP/1.1");

    env.env_str.push_back("QUERY_STRING=" + req.query);

    env.env_str.push_back("SCRIPT_NAME=" + req.path);
    env.env_str.push_back("SERVER_NAME=localhost");
    env.env_str.push_back("SERVER_PORT=8080");
    env.env_str.push_back("SERVER_SOFTWARE=webserv/1.0");
    env.env_str.push_back("REMOTE_ADDR=127.0.0.1");

    if (req.method == "POST")
    {
        env.env_str.push_back("CONTENT_LENGTH=" + toString(req.contentLength));
        std::map<std::string, std::string>::const_iterator it = req.headers.find("content-type");
        if (it != req.headers.end())
            env.env_str.push_back("CONTENT_TYPE=" + req.headers["content-type"]);
    }

    for (std::map<std::string, std::string>::const_iterator it = req.headers.begin(); it != req.headers.end(); it++)
    {
        if (it->first == "content-type" || it->first == "content-length")
            continue;
        env.env_str.push_back(format_header_key(it->first) + "=" + it->second);
    }
    env.final_env();
    return env;
}

void CGI_Process::reset()
{
    if (_read_fd >= 0)
    {
        close(_read_fd);
        _read_fd = -1;
    }
    if (_write_fd >= 0)
    {
        close(_write_fd);
        _write_fd = -1;
    }
    if (_pid > 0)
    {
        int status;
        kill(_pid, SIGKILL);
        waitpid(_pid, &status, 0);
        _pid = -1;
    }
    _output_buffer.clear();
    start_time_ms = 0;
    last_output_ms = 0;
}

void CGI_Process::append_output(const char *buf, size_t n)
{
    _output_buffer.append(buf, n);
}

//
// static bool write_all_blocking(int fd, const char* data, size_t len)
// {
//     size_t off = 0;
//     while (off < len)
//     {
//         ssize_t n = ::write(fd, data + off, len - off);
//         if (n > 0)
//         {
//             off += (size_t)n;
//             continue;
//         }
//         if (n < 0 && errno == EINTR)
//             continue;
//         // 这里如果坚持 non-block 写，需要处理 EAGAIN 并用 epoll 等待可写
//         // 但当前架构没管理 stdin 的 epoll，所以最好让它阻塞写完。
//         return false;
//     }
//     return true;
// }
//

// bool CGI_Process::execute(const std::string &script_path, HTTPRequest &req)
// {
//     int pipe_in[2];
//     int pipe_out[2];

//     if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
//         return false;
//     _pid = fork();
//     if (_pid < 0)
//     {
//         close(pipe_in[0]);
//         close(pipe_in[1]);
//         close(pipe_out[0]);
//         close(pipe_out[1]);
//         return false;
//     }
//     if (_pid == 0)
//     {
//         dup2(pipe_in[0], STDIN_FILENO);
//         dup2(pipe_out[1], STDOUT_FILENO);
//         // close(pipe_in[1]);
//         // close(pipe_out[0]);

//         //// close all pipe fds in child after dup2？
//         close(pipe_in[0]);
//         close(pipe_in[1]);
//         close(pipe_out[0]);
//         close(pipe_out[1]);
//         // add env

//         CGI_ENV envp = get_env_from_request(req);

//         char *argv[] = {const_cast<char *>(script_path.c_str()), NULL};
//         execve(script_path.c_str(), argv, envp.envp.data());
//         exit(1);
//     }
//     close(pipe_in[0]);
//     close(pipe_out[1]);
//     _read_fd = pipe_out[0];
//     _write_fd = pipe_in[1];
//     set_non_block_fd(_read_fd);
//     // set_non_block_fd(_write_fd);

//     //
//     start_time_ms = Client::now_ms();
//     //
//     // if (req.method == "POST" && req.has_body)
//     // //write the content in the body
//     //     write(pipe_in[1], req.body.c_str(), req.body.size());
//     // stdin：不要 non-block，确保 body 写满，否则 CGI 可能一直等
//     if (req.method == "POST" && req.has_body && !req.body.empty())
//     {
//         if (!write_all_blocking(_write_fd, req.body.c_str(), req.body.size()))
//         {
//             // 写失败：把写端关掉，留给上层走 500/504
//             close(_write_fd);
//             _write_fd = -1;
//             // 这里用 SIGKILL 还是 SIGTERM?
//             kill(_pid, SIGKILL);
//             waitpid(_pid, NULL, 0);
//             _pid = -1;
//             // 同时把 read 端也关掉，避免 fd 泄漏
//             if (_read_fd >= 0)
//             {
//                 close(_read_fd);
//                 _read_fd = -1;
//             }
//             return false;
//         }
//     }
//     close(_write_fd);
//     _write_fd = -1;
//     return true;
// }

bool CGI_Process::execute(const std::string &script_path, HTTPRequest &req)
{
    int pipe_in[2];
    int pipe_out[2];

    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
        return false;
    _pid = fork();
    if (_pid < 0)
    {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        return false;
    }
    if (_pid == 0)
    {
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_in[1]);
        close(pipe_out[0]);
        // add env

        CGI_ENV envp = get_env_from_request(req);

        char *argv[] = {const_cast<char *>(script_path.c_str()), NULL};
        execve(script_path.c_str(), argv, envp.envp.data());
        exit(1);
    }
    close(pipe_in[0]);
    close(pipe_out[1]);
    _read_fd = pipe_out[0];
    _write_fd = pipe_in[1];
    set_non_block_fd(_read_fd);
    //set_non_block_fd(_write_fd);

    //
    start_time_ms = Client::now_ms();
    last_output_ms = 0;
    
    //
    if (req.method == "POST" && req.has_body)
        // write the content in the body
        write(pipe_in[1], req.body.c_str(), req.body.size());
    // stdin：不要 non-block，确保 body 写满，否则 CGI 可能一直等

    close(_write_fd);
    _write_fd = -1;
    return true;
}

void CGI_Process::reset_no_kill()
{
    // 清空 CGI 输出缓冲、状态机字段
    _output_buffer.clear();
    // 任何与 pid/fd 有关的都不要动
    start_time_ms = 0;
    last_output_ms = 0;
}

void CGI_Process::set_non_block_fd(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl get flags failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl set flags failed");
}