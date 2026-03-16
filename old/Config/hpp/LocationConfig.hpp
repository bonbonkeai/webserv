#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <cstddef>
#include <set>

struct ErrorPageRule
{
    std::string uri;
    bool override_set;
    int override_code;
    ErrorPageRule(): uri(""), override_set(false), override_code(0) {}
};

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
    std::string alias;
    bool autoindex;
    std::vector<std::string> allow_methodes;
    std::vector<std::string> index;
    size_t client_max_body_size;

    //FLags d override
    bool has_root;
    bool has_autoindex;
    bool has_methodes;
    bool has_index;
    bool has_client_max_body_size;
    bool has_alias;

    bool has_return;
    int return_code;
    std::string return_url;

    bool has_cgi;
    std::map<std::string, std::string> cgi_exec;
    std::set<std::string> cgi_extensions;
    //upload path
    std::string upload_path;
    bool    has_upload_path;

    bool has_error_pages;
    std::map<int, ErrorPageRule> error_pages;
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
