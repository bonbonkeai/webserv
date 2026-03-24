
#include "Event/hpp/Client.hpp"
#include <stdio.h>
#include <limits.h> // 用于 PATH_MAX
#include <stdlib.h> // 用于 realpath
#include <unistd.h> // 用于 access, getcwd
#include <string.h> // 用于 strerror
#define TRACE() std::cout << "[] " << __FILE__ << ":" << __LINE__ << std::endl;

CGI_Process::CGI_Process()
    : _state(CREATE),
      _pid(-1),
      _read_fd(-1),
      _write_fd(-1),
      _error_code(500),
      has_output(false),
      write_pos(0),
      start_time_ms(0),
      last_output_ms(0),
      client(NULL),
      _has_wait_status(false),
      _wait_status(0)
{
}

CGI_Process::~CGI_Process()
{
    terminate();
}

bool CGI_Process::create_pipe(int pipe_in[2], int pipe_out[2])
{
    pipe_in[0]=pipe_in[1]=pipe_out[0]=pipe_out[1]=-1;
    if (pipe(pipe_in) < 0)
        return false;
    if (pipe(pipe_out) < 0)
    {
        close(pipe_in[0]);
        close(pipe_in[1]);
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
    if (access(script_path.c_str(), F_OK) != 0)
    {
        // 文件不存在
        _state = CGI_Process::ERROR;
        _error_code = 404;
        return false;
    }
    if (access(script_path.c_str(), X_OK) != 0)
    {
        // 文件存在但不可执行
        _state = CGI_Process::ERROR;
        _error_code = 403;
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

bool CGI_Process::setup_child_process(int pipe_in[2], int pipe_out[2],
                                      const EffectiveConfig &config,
                                      const HTTPRequest &req)
{
    close(pipe_in[1]);
    close(pipe_out[0]);

    if (dup2(pipe_in[0], STDIN_FILENO) < 0 || dup2(pipe_out[1], STDOUT_FILENO) < 0 || dup2(pipe_out[1], STDERR_FILENO) < 0)
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
    char **envp = env.envp.empty() ? NULL : &env.envp[0];

    // 1) 取扩展名
    std::string ext;
    std::size_t dot = req._rout.fs_path.rfind('.');
    if (dot != std::string::npos)
        ext = req._rout.fs_path.substr(dot);

    // 2) 若配置了 CGI 解释器，用解释器跑脚本
    std::map<std::string, std::string>::const_iterator it = config.cgi_exec.find(ext);
    if (it != config.cgi_exec.end() && !it->second.empty())
    {
        const std::string &interp = it->second;
        char *argv2[3];
        argv2[0] = const_cast<char*>(interp.c_str());
        argv2[1] = abs_path;
        argv2[2] = NULL;
        execve(argv2[0], argv2, envp);
        perror("execve interpreter failed");
        free(abs_path);
        _exit(1);
    }

    // 3) fallback：直接执行脚本（依赖 shebang + 无 CRLF）
    char *argv1[] = { abs_path, NULL };
    execve(abs_path, argv1, envp);

    perror("execve script failed");
    free(abs_path);
    _exit(1);
}


bool CGI_Process::setup_parent_process(int pipe_in[2], int pipe_out[2], const HTTPRequest &req)
{

    (void)req;
    close(pipe_in[0]);
    close(pipe_out[1]);

    _read_fd = pipe_out[0];
    _write_fd = pipe_in[1];
    set_non_block_fd(_read_fd);
    set_non_block_fd(_write_fd);

    _state = RUNNING;
    start_time_ms = Client::now_ms();
    write_pos = 0;// 每次新 CGI 从 0 开始写
    has_output = false;
    last_output_ms = 0;
    return true;
}

void CGI_Process::terminate()
{
    if (_pid > 0)
    {
        if (!_has_wait_status)
        {
            int status = 0;
            pid_t r = waitpid(_pid, &status, WNOHANG);
            if (r == _pid)
            {
                _has_wait_status = true;
                _wait_status = status;
            }
            else if (r == 0)
            {
                kill(_pid, SIGKILL);
                if (waitpid(_pid, &status, 0) == _pid)
                {
                    _has_wait_status = true;
                    _wait_status = status;
                }
            }
        }
        _pid = -1;
    }

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
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return (true); //继续等下次可读
        _state = CGI_Process::ERROR;
        close(_read_fd);
        _read_fd = -1;
        return (false);
    }
}
bool    CGI_Process::write_body(const std::string &body)
{
    if (_write_fd < 0 || _state != CGI_Process::RUNNING)
        return (false);
    if (write_pos >= body.size())
    {
        close(_write_fd);
        _write_fd = -1;
        return (true);
    }
    ssize_t n = write(_write_fd, body.data() + write_pos, body.size() - write_pos);
    if (n > 0)
    {
        write_pos += n;
        if (write_pos >= body.size())
        {
            close(_write_fd);
            _write_fd = -1;
            return (true);//写完
        }
        return (false);//还没写完
    }

    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return (false);//暂时写不了，等下一次 EPOLLOUT
    _state = CGI_Process::ERROR;
    close(_write_fd);
    _write_fd = -1;
    return (false);
}

void CGI_Process::set_non_block_fd(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl get flags failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl set flags failed");
}