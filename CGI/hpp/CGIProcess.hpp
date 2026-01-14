#endif
#ifndef CGIPROCESS_HPP
#define CGIPROCESS_HPP

#include "../../HTTP/hpp/HTTPRequest.hpp"
#include "../../HTTP/hpp/HTTPResponse.hpp"
#include "Event/hpp/Client.hpp"

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

struct Client;
struct CGI_ENV
{
    std::vector<std::string> env_str;
    std::vector<char *> envp;

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
    pid_t _pid;
    int _read_fd;
    int _write_fd;
    std::string _output_buffer;

public:
    CGI_Process();
    ~CGI_Process();

    std::string format_header_key(const std::string &key);
    CGI_ENV get_env_from_request(HTTPRequest &req); // 可能还需要别的内容
    bool execute(const std::string &script_path, HTTPRequest &req);
    void reset();
    void append_output(const char *buf, size_t n);
    void set_non_block_fd(int fd);

    pid_t get_pid() const
    {
        return _pid;
    }
    int get_read_fd() const
    {
        return _read_fd;
    }
    void    set_pid(pid_t pid)
    {
        _pid = pid;
    }
    void    set_read_fd(pid_t read_pid)
    {
        _read_fd = read_pid;
    }
    void    set_write_fd(pid_t pid)
    {
        _write_fd = pid;
    }
    std::string get_output()
    {
        return _output_buffer;
    }
};

#endif

// 完整 CGI 子进程管理：
//     建立 pipe
//     fork
//     子进程 dup2 → execve
//     父进程写 body 到 stdin
//     父进程将 stdout fd 加入 B 的 poll
//     解析完毕后返回到 Request handle。