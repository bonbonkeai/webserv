
#include "Event/hpp/Client.hpp"
#include <stdio.h>
#include <limits.h> // 用于 PATH_MAX
#include <stdlib.h> // 用于 realpath
#include <unistd.h> // 用于 access, getcwd
#include <string.h> // 用于 strerror
#define TRACE() std::cout << "[] " << __FILE__ << ":" << __LINE__ << std::endl;

CGI_Process::CGI_Process() : _state(CREATE), _pid(-1), _read_fd(-1),
                             _write_fd(-1), has_output(false), write_pos(0),
                             start_time_ms(0), last_output_ms(0), client(NULL)
{
}

CGI_Process::~CGI_Process()
{
    terminate();
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

bool CGI_Process::execute(const EffectiveConfig &config, const HTTPRequest &req, Client *c)
{
    int pipe_in[2];
    int pipe_out[2];

    std::string script_path = req._rout.fs_path;
    if (access(script_path.c_str(), X_OK) != 0)
    {
        _state = CGI_Process::ERROR;
        return false;
    }
    if (!create_pipe(pipe_in, pipe_out))
        return false;
    client = c;
    _pid = fork();
    if (_pid < 0)
    {
        close_pipes(pipe_in, pipe_out);
        return false;
    }
    if (_pid == 0)
        setup_child_process(pipe_in, pipe_out, config, req);
    return setup_parent_process(pipe_in, pipe_out, req);
}

bool CGI_Process::setup_child_process(int pipe_in[2], int pipe_out[2], const EffectiveConfig &config,
                                      const HTTPRequest &req)
{
    close(pipe_in[1]);
    close(pipe_out[0]);

    if (dup2(pipe_in[0], STDIN_FILENO) < 0 || dup2(pipe_out[1], STDOUT_FILENO) < 0 ||
        dup2(pipe_out[1], STDERR_FILENO) < 0)
        _exit(1);
    close(pipe_in[0]);
    close(pipe_out[1]);

    char *abs_path = realpath(req._rout.fs_path.c_str(), NULL);
    if (!abs_path)
    {
        perror("realpath failed");
        _exit(1);
    }

    CGI_ENV env = CGI_ENV::get_env_from_request(req, config);
    env.final_env();

    char *argv[] = {abs_path, NULL};
    execve(abs_path, argv, env.envp.data());

    perror("execve failed");
    free(abs_path);
    _exit(1);
    return false;
}

bool CGI_Process::setup_parent_process(int pipe_in[2], int pipe_out[2], const HTTPRequest &req)
{

    close(pipe_in[0]);
    close(pipe_out[1]);

    _read_fd = pipe_out[0];
    _write_fd = pipe_in[1];
    // if (!set_non_block_fd(_read_fd))
    //{
    //     terminate();
    //     _state = ERROR_CGI;
    //     _error_message = "Failed to se non block";
    //     return false;
    // }
    set_non_block_fd(_read_fd);
    set_non_block_fd(_write_fd);

    _state = RUNNING;
    start_time_ms = Client::now_ms();

    //
    if (req.method == "POST" && req.has_body)
    {
        if (!write_body(req.body))
        {
            terminate();
            _state = CGI_Process::ERROR;
            close_pipes(pipe_in, pipe_out);
            return false;
        }
    }

    // stdin：不要 non-block，确保 body 写满，否则 CGI 可能一直等
    close(_write_fd);
    _write_fd = -1;
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

bool CGI_Process::read_output(std::string &buffer)
{
    if (_read_fd < 0 || _state != CGI_Process::RUNNING)
        return false;

    char buf[4096];
    ssize_t n = read(_read_fd, buf, sizeof(buf));

    if (n > 0)
    {
        buffer.append(buf, n);
        _output_buffer.append(buf, n);
        has_output = true;
        last_output_ms = Client::now_ms();
        return true;
    }
    else if (n == 0)
    {
        _state = CGI_Process::FINISHED;
        close(_read_fd);
        _read_fd = -1;
        return false;
    }
    else
    {
        _state = CGI_Process::ERROR;
        close(_read_fd);
        _read_fd = -1;
        return false;
    }
}
bool    CGI_Process::write_body(const std::string &body)
{
    if (_write_fd < 0 || _state != CGI_Process::RUNNING)
        return false;
    
    if (write_pos > body.size()) //finish writing
    {
        close(_write_fd);
        _write_fd = -1;
        return true;
    }

    size_t  remaind = body.size() - write_pos;
    ssize_t n = write(_write_fd, body.c_str() + write_pos, remaind);

    if (n > 0)
    {
        write_pos += n;
        if (write_pos >= body.size())
        {
            close(_write_fd);
            _write_fd = -1;
            return true;
        }
        return false;
    }
    else
    {
        _state = CGI_Process::ERROR;
        close(_write_fd);
        _write_fd = -1;
        return false;
    }
}
/*bool CGI_Process::check_timeout(unsigned long long now)
{

    unsigned long long diff = now - start_time_ms;
    if (_output_buffer.empty())
    {
        if (diff > START_TIMEOUT)
            return true;
        return false;
    }

    // partiel pipe
    if (last_output_ms > 0)
    {
        unsigned long long diff_time = now - last_output_ms;
        if (diff_time > EXECUTION_TIMEOUT)
            return true;
    }

    // total execution timeout
    if (diff > EXECUTION_TIMEOUT * 2)
        return true;
    return false;
}*/
void CGI_Process::set_non_block_fd(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl get flags failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl set flags failed");
}