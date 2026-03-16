#ifndef CGIREQUESTHandle_HPP
#define CGIREQUESTHandle_HPP

#include "HTTP/hpp/HTTPResponse.hpp"
#include "HTTP/hpp/ErrorResponse.hpp"
#include "CGI/hpp/CGIProcess.hpp"
#include "HTTP/hpp/HTTPRequest.hpp"
#include "Config/hpp/EffectiveConfig.hpp"
#include "CGI/hpp/CGIManager.hpp"
#include "Event/hpp/Client.hpp"

#include <iostream>
#include <map>
#include <stdint.h>

class CGI_Process;
class CGIManager;
class HTTPResponse;
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
    static std::string format_header_key(const std::string &key);
    static CGI_ENV get_env_from_request(const HTTPRequest &req, const EffectiveConfig &config);
};

class CGIRequestHandle
{
private:

    CGI_Process* _process;
    CGIManager* _manager;
    HTTPRequest _req;
    EffectiveConfig _config;

    // output
    std::string _output_buffer;
    HTTPResponse _resp;
    bool    completed;
    bool    succes;

public:
    CGIRequestHandle(const HTTPRequest &req, EffectiveConfig& cfg);
    ~CGIRequestHandle();

    bool start(CGIManager& manager, const EffectiveConfig &config, const HTTPRequest &req, Client* c);
 
    //uint32_t    get_events() const;
    // handle_data
    bool handle_read();
    bool handle_write();
    HTTPResponse get_response() const;

    int     get_read_fd() const;
    int get_write_fd() const;
    bool    is_completed() const
    {
        return completed;
    }
    CGI_Process * get_process()
    {
        return _process;
    }
};

#endif

// 统一管理多个正在运行的 CGI
// 保存 pid / fd 对应关系
// 提供ex:
// startCGI()
// readCGIOutput()
// finishCGI()