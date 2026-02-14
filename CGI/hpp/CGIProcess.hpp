#ifndef CGIPROCESS_HPP
#define CGIPROCESS_HPP

#include "../../HTTP/hpp/HTTPRequest.hpp"
#include "../../HTTP/hpp/HTTPResponse.hpp"
#include "Config/hpp/EffectiveConfig.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "Method_Handle/hpp/CGIRequestHandle.hpp"

#include <iostream>
#include <vector>
#include <map>
#include <iterator>
#include <unistd.h>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#define EXECUTION_TIMEOUT 10000ULL
#define START_TIMEOUT 5000ULL

class Server;
struct Client;
struct CGI_ENV;

class CGI_Process
{
private:

    bool create_pipe(int pipe_in[2], int pipe_out[2]);
    void close_pipes(int pipe_in[2], int pipe_out[2]);
    bool setup_child_process(int pipe_in[2], int pipe_out[2], const EffectiveConfig &config,
                             const HTTPRequest &req);
    bool setup_parent_process(int pipe_in[2], int pipe_out[2],const HTTPRequest &req);

public:
    enum State
    {
        CREATE,
        RUNNING,
        FINISHED,
        ERROR,
        TIMEOUT
    } _state;

    pid_t _pid;
    int _read_fd;
    int _write_fd;
    // output
    std::string _output_buffer;
    bool has_output;
    size_t write_pos;

    // timeout
    unsigned long long start_time_ms;
    unsigned long long last_output_ms;

    Client *client;

    CGI_Process();
    ~CGI_Process();
    bool execute(const EffectiveConfig &config, const HTTPRequest &req, Client* c);
    void terminate();

    // IO gestion
    bool write_body(const std::string &body);
    bool read_output(std::string &buffer);

    // timeout
   // bool check_timeout(unsigned long long now);

    bool  is_running()
    {
        return _state == CGI_Process::RUNNING;
    }
    bool    is_finished()
    {
        return _state == CGI_Process::FINISHED;
    }
    bool    is_timeout()
    {
        return _state == CGI_Process::TIMEOUT;
    }
    void set_non_block_fd(int fd);

};

#endif

// 完整 CGI 子进程管理：
//     建立 pipe
//     fork
//     子进程 dup2 → execve
//     父进程写 body 到 stdin
//     父进程将 stdout fd 加入 B 的 poll
//     解析完毕后返回到 Request handle。
