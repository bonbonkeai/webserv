#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <map>
#include <vector>
#include <cctype>
#include "LocationConfig.hpp"

/* Serveur Brut(parser)*/
struct ServerConfig
{
    //server block
    std::map<std::string, std::vector<std::string> > directives;
    //Toutes les locations
    std::vector<LocationConfig> locations;
};

/* Serveur Final (runtime)*/
struct ServerRuntimeConfig
{
    std::string host;
    std::string server_name;
    int listen;
    int port;
    std::string root;
    bool autoindex;
    size_t client__max_body_size;
    std::vector<std::string> index;
    std::vector<std::string> allowed_methods;
    std::map<int, ErrorPageRule> error_page;
    std::vector<LocationRuntimeConfig> locations;

    bool matchesHost(const std::string& reqHost) const 
    {
        if(reqHost.empty())
            return true;
        std::string cleanH = reqHost;
        size_t pos = cleanH.find(":");
        if (pos != std::string::npos)
            cleanH = cleanH.substr(0,pos);
        if (!server_name.empty())
        {
            std::string lhs = cleanH;
            std::string rhs = server_name;
            for (size_t i = 0; i < lhs.size(); ++i)
                lhs[i] = static_cast<char>(std::tolower(lhs[i]));
            for (size_t i = 0; i < rhs.size(); ++i)
                rhs[i] = static_cast<char>(std::tolower(rhs[i]));
            return lhs == rhs;
        }
        return true;
    }
};

#endif
 
/*存储单个 server {} block 中的配置：
listen ip:port
root
index 文件数组
autoindex on/off
client_max_body_size
error_page CODE path
一个 server 下所有 location 的列表
//不包含运行时逻辑，纯数据结构。*/
