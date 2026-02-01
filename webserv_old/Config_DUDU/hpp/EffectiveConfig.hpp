#ifndef EFFECTIVECONFIG_HPP
#define EFFECTIVECONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"
#include "ServerConfig.hpp"
#include "ConfigUtils.hpp"

struct EffectiveConfig
{
    std::string root;
    std::vector<std::string> index;
    bool autoindex;
    std::vector<std::string> allowed_methods;
    std::map<int, std::string> error_pages;
    bool is_cgi;
    std::string cgi_path;
    size_t max_body_size;

    EffectiveConfig(): autoindex(false), is_cgi(false), max_body_size(0){}
};

#endif
