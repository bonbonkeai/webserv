#ifndef EFFECTIVECONFIG_HPP
#define EFFECTIVECONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include "Config/hpp/LocationConfig.hpp"
#include "Config/hpp/ServerConfig.hpp"
#include "Config/hpp/ConfigUtils.hpp"

struct EffectiveConfig
{
    std::string root;
    std::vector<std::string> index;
    bool autoindex;
    std::vector<std::string> allowed_methods;
    std::map<int, ErrorPageRule> error_pages;
    bool is_cgi;
    std::map<std::string, std::string> cgi_exec;
    bool has_return;
    int return_code;
    std::string return_url;
    size_t max_body_size;

    EffectiveConfig(): autoindex(false), is_cgi(false), has_return(false), return_code(302), max_body_size(0){}
};

ServerRuntimeConfig buildServer(const ServerConfig& raw);
std::vector<ServerRuntimeConfig> buildRuntime(const std::vector<ServerConfig>& raw);
LocationRuntimeConfig buildLocation(const ServerRuntimeConfig& srv, const LocationConfig& raw);

#endif
