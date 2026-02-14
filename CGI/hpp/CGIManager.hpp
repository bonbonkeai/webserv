#ifndef CGIMANAGER_HPP
#define CGIMANAGER_HPP

#include "CGIProcess.hpp"
#include <iostream>
#include <map>
#include <vector>

class CGI_Process;

class CGIManager
{
private:
    std::map<int, CGI_Process*> _fd_to_process;
    std::vector<CGI_Process*>   _all_process;
    std::vector<CGI_Process*>   _timeout_cgi;

public:
    CGIManager();
    ~CGIManager();

    void    add_process(CGI_Process* proc);
    void    remove_process(CGI_Process* proc);

    CGI_Process*    get_process_by_fd(int fd);
    bool    is_cgi_fd(int fd);

    void    registe_fd(int fd, CGI_Process *proc);
    void    unregiste_fd(int fd);

    //void    find_all_cgi_timeout(unsigned long long now);

    void    cleanup_process();
    std::vector<CGI_Process*> get_timeout_cgi(){
        return _timeout_cgi;
    }
};

#endif

//统一管理多个正在运行的 CGI
//        保存 pid /
//    fd 对应关系
//        提供ex : startCGI()
//                     readCGIOutput()
//                         finishCGI()