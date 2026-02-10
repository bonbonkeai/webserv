#ifndef CGIPROCESS_HPP
#define CGIPROCESS_HPP

#include "../../HTTP/hpp/HTTPRequest.hpp"
#include "../../HTTP/hpp/HTTPResponse.hpp"
#include "Config/hpp/EffectiveConfig.hpp"
#include "Event/hpp/Server.hpp"

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

class Server;

struct Client;

struct CGI_path
{
    std::string script_filename;
    std::string script_name;
    std::string path_info;
};

struct CGI_ENV
{
    std::vector<std::string> env_str;
    std::vector<char *> envp;
    CGI_path cgipaths;

    static bool check_extension(const std::string &str, const std::set<std::string> &cgi_exetension);

    static CGI_path final_script_paths(const HTTPRequest &req, const EffectiveConfig &config);

    void final_env()
    {
        for (size_t i = 0; i < env_str.size(); i++)
            envp.push_back((char *)env_str[i].c_str());
        envp.push_back(NULL);
    }
    ~CGI_ENV() {}
};

class CGI_Process
{
private:
    std::string _error_message;
    HTTPResponse _response;
    static std::string format_header_key(const std::string &key);
    static CGI_ENV get_env_from_request(HTTPRequest &req, const EffectiveConfig &config);

public:
    enum State
    {
        RUNNING,
        FINISHED,
        ERROR_CGI,
        TIMEOUT
    };
    pid_t _pid;
    int _read_fd;
    int _write_fd;
    State _state;
    std::string _output_buffer;
    std::string script_path;

    // timeout
    unsigned long long start_time_ms;
    unsigned long long last_output_ms;

    CGI_Process();
    ~CGI_Process();
    bool execute(const EffectiveConfig& config, HTTPRequest &req);
    void append_output(const char *buf, size_t n);
    const std::string &get_output() const
    {
        return _output_buffer;
    }
    void reset();
    bool write_body(const std::string &body);
    void terminate();
    bool check_timeout(unsigned long long now);
    bool handle_output();
    void get_exit_status();
    void handle_timeout();
    void handle_pipe_error();
    HTTPResponse build_response(bool keep_alive);
    void reset_no_kill();
    bool create_pipe(int pipe_in[2], int pipe_out[2]);
    void close_pipes(int pipe_in[2], int pipe_out[2]);
    bool setup_child_process(int pipe_in[2], int pipe_out[2], const EffectiveConfig& config,
                             HTTPRequest &req);
    bool setup_parent_process(int pipe_in[2], int pipe_out[2], HTTPRequest &req);
    bool set_non_block_fd(int fd);
};

#endif

// 完整 CGI 子进程管理：
//     建立 pipe
//     fork
//     子进程 dup2 → execve
//     父进程写 body 到 stdin
//     父进程将 stdout fd 加入 B 的 poll
//     解析完毕后返回到 Request handle。
