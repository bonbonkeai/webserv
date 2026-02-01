#ifndef CGIMANAGER_HPP
#define CGIMANAGER_HPP

#include "CGIProcess.hpp"
#include <iostream>
#include <map>

class   CGIManager
{
    private:
    
    public:
        CGIManager();
        ~CGIManager();

};

#endif

统一管理多个正在运行的 CGI
保存 pid / fd 对应关系
提供ex:
startCGI()
readCGIOutput()
finishCGI()