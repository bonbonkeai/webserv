#ifndef ROUTING_HPP
#define ROUTING_HPP

#include <string>
#include <vector>
#include <cstddef>   // for NULL
#include "HTTP/hpp/HTTPResponse.hpp"
#include "HTTP/hpp/HTTPRequest.hpp"
#include "HTTP/hpp/HTTPUtils.hpp" 
#include "ConfigParser.hpp"
#include "Config/hpp/ConfigTokenizer.hpp"
#include "ServerConfig.hpp"
#include "EffectiveConfig.hpp"
#include "ConfigUtils.hpp"

class Routing
{
    private:
        const std::vector<ServerRuntimeConfig>& _serveurs;

    public:
        Routing(const std::vector<ServerRuntimeConfig>& serveurs);
        ~Routing(){};
        const ServerRuntimeConfig& selectS(const HTTPRequest& request, int listen_port) const;
        EffectiveConfig resolve(const HTTPRequest& request, int listen_port) const;
        //const LocationConfig* matchLocation(const ServerConfig& server, const std::string& uri);
        const LocationRuntimeConfig* matchLocation(const ServerRuntimeConfig &server, const std::string &uri) const;

};
#endif

/*配置层的核心逻辑：
根据 client 请求的 Host header 选择正确的 ServerConfig 
根据 URI longest-prefix-match 匹配 Routing 
合并 server + location 得到最终有效配置对象 EffectiveConfig 
//拼接
服务器所有行为 (静态文件根目录、cgi、index…) 都依赖这个组件*/
