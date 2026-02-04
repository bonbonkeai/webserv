#include "../hpp/CGIProcess.hpp"
#include "Event/hpp/Client.hpp"
#include <stdio.h>
#include <limits.h> // 用于 PATH_MAX
#include <stdlib.h> // 用于 realpath
#include <unistd.h> // 用于 access, getcwd
#include <string.h> // 用于 strerror
#define TRACE() std::cout << "[] " << __FILE__ << ":" << __LINE__ << std::endl;

CGI_Process::CGI_Process() : _error_message(),
                             _pid(-1), _read_fd(-1), _write_fd(-1), _state(RUNNING),
                             _output_buffer(), start_time_ms(0), last_output_ms(0)
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
    terminate();
    if (_write_fd >= 0)
    {
        close(_write_fd);
        _write_fd = -1;
    }

    _output_buffer.clear();
    _error_message.clear();
    start_time_ms = 0;
    last_output_ms = 0;
    _state = RUNNING;
}

void CGI_Process::append_output(const char *buf, size_t n)
{
    _output_buffer.append(buf, n);
}

bool CGI_Process::create_pipe(int pipe_in[2], int pipe_out[2])
{
    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
    {
        if (pipe_in[0] >= 0)
        {
            close(pipe_in[0]);
            close(pipe_in[1]);
        }
        return false;
    }
    return true;
}
void CGI_Process::close_pipes(int pipe_in[2], int pipe_out[2])
{
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);
}

bool CGI_Process::execute(const std::string &script_path, const std::string &exec_path, HTTPRequest &req)
{
    int pipe_in[2];
    int pipe_out[2];

    if (!create_pipe(pipe_in, pipe_out))
        return false;
    _pid = fork();
    if (_pid < 0)
    {
        close_pipes(pipe_in, pipe_out);
        return false;
    }
    if (_pid == 0)
        setup_child_process(pipe_in, pipe_out, script_path, exec_path, req);
    return setup_parent_process(pipe_in, pipe_out, req);
}

bool CGI_Process::setup_child_process(int pipe_in[2], int pipe_out[2], const std::string &script_path,
                                      const std::string &exec_path, HTTPRequest &req)
{
    close(pipe_in[1]);
    close(pipe_out[0]);

    if (dup2(pipe_in[0], STDIN_FILENO) < 0 || dup2(pipe_out[1], STDOUT_FILENO) < 0 ||
        dup2(pipe_out[1], STDERR_FILENO) < 0)
        _exit(1);
    close(pipe_in[0]);
    close(pipe_out[1]);

    char *abs_path = realpath(script_path.c_str(), NULL);
    if (!abs_path)
    {
        perror("realpath failed");
        _exit(1);
    }

    CGI_ENV env = get_env_from_request(req);
    env.env_str.push_back("PATH=/usr/bin:/bin");
    env.env_str.push_back("SCRIPT_FILENAME=" + std::string(abs_path));
    env.final_env();

    if (exec_path.empty())
    {
        char *argv[] = {abs_path, NULL};
        execve(abs_path, argv, env.envp.data());
    }
    else
    {
        char *argv[] = {
            const_cast<char *>(exec_path.c_str()), abs_path, NULL};
        execve(exec_path.c_str(), argv, env.envp.data());
    }
    perror("execve failed");
    free(abs_path);
    _exit(1);
    return false;
}

bool CGI_Process::setup_parent_process(int pipe_in[2], int pipe_out[2], HTTPRequest &req)
{

    close(pipe_in[0]);
    close(pipe_out[1]);

    _read_fd = pipe_out[0];
    _write_fd = pipe_in[1];
    if (!set_non_block_fd(_read_fd))
    {
        terminate();
        _state = ERROR_CGI;
        _error_message = "Failed to se non block";
        return false;
    }
    _state = RUNNING;
    start_time_ms = Client::now_ms();

    //
    if (req.method == "POST" && req.has_body)
    {
        if (!write_body(req.body))
        {
            terminate();
            _state = ERROR_CGI;
            _error_message = "failed to write POST body";
            close_pipes(pipe_in, pipe_out);
            return false;
        }
    }

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

bool CGI_Process::set_non_block_fd(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return false;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return false;
    return true;
}

bool CGI_Process::write_body(const std::string &body)
{
    if (body.empty())
        return true;

    size_t total = body.size();
    size_t written = 0;

    while (written < total)
    {
        ssize_t n = write(_write_fd, body.c_str() + written, total - written);
        if (n > 0)
            written += n;
        else
            return false;
    }
    return true;
}

void CGI_Process::terminate()
{
    if (_pid > 0)
    {
        kill(_pid, SIGKILL);
        waitpid(_pid, NULL, 0);
        _pid = -1;
    }
    if (_read_fd >= 0)
    {
        close(_read_fd);
        _read_fd = -1;
    }
}

bool CGI_Process::check_timeout(unsigned long long now, unsigned long long timeout)
{
    if (!_output_buffer.empty())
    {
        return (now - last_output_ms) > timeout;
    }
    else
        return (now - start_time_ms) > timeout;
}

bool CGI_Process::handle_output()
{
    char buf[4096];

    while (true)
    {
        ssize_t n = read(_read_fd, buf, sizeof(buf));
        if (n > 0)
        {
            append_output(buf, n);
            last_output_ms = Client::now_ms();
            continue;
            TRACE();
        }
        else if (n == 0)
        {
            TRACE();
            get_exit_status();
            _state = FINISHED;
            return true;
        }
        return false;
    }
}

void CGI_Process::get_exit_status()
{
    if (_pid > 0)
    {
        int status;
        if (waitpid(_pid, &status, WNOHANG) > 0)
        {
            _pid = -1;
            _state = (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? FINISHED : ERROR_CGI;
        }
    }
}

void CGI_Process::handle_timeout()
{
    terminate();
    _state = TIMEOUT;
    _error_message = "CGI execution timeout";
}

void CGI_Process::handle_pipe_error()
{
    terminate();
    _state = ERROR_CGI;
    _error_message = "CGI pipe error";
}

HTTPResponse CGI_Process::build_response(bool keep_alive)
{
    HTTPResponse resp;

    switch (_state)
    {
    case FINISHED:
        if (_output_buffer.empty())
        {
            resp = buildErrorResponse(500);
        }
        else
        {
            resp = resp.buildResponseFromCGIOutput(_output_buffer, keep_alive);
        }
        break;

    case ERROR_CGI:
        resp = buildErrorResponse(500);
        resp.headers["content-type"] = "text/plain";
        resp.body = "CGI Error: " + _error_message;
        break;

    case TIMEOUT:
        resp = buildErrorResponse(504);
        break;

    default:
        // 如果有输出，尝试使用它
        if (!_output_buffer.empty())
        {
            resp = resp.buildResponseFromCGIOutput(_output_buffer, keep_alive);
        }
        else
        {
            resp = buildErrorResponse(500);
        }
        break;
    }

    return resp;
}