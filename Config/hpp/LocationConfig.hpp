#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>
#include <map>

/* Location Brut(parser)*/
struct LocationConfig
{
    std::string path;
    std::map<std::string, std::vector<std::string> > directives;
};

/* Location Final (runtime)*/
struct LocationRuntimeConfig
{
    std::string path;
    std::string root;
    bool autoindex;
    std::vector<std::string> allow_methodes;
    std::vector<std::string> index;

    //FLags d override
    bool has_root;
    bool has_autoindex;
    bool has_methodes;
    bool has_index;
};
#endif

/*
对应 location /path { ... } 内容：
path 匹配前缀
root 覆盖 server
allowed_methods GET/POST/DELETE
upload_path
cgi_extension & cgi_executor
return 重定向
autoindex / index
//纯数据结构。*/