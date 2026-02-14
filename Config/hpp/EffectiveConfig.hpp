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
    // for cgi env
    int server_port;
    std::string server_name;

    // static content
    std::string root;
    std::string alias;
    std::vector<std::string> index;
    bool autoindex;

    // method/body
    std::vector<std::string> allowed_methods;
    size_t max_body_size;

    // error/ redirection
    std::map<int, ErrorPageRule> error_pages;
    bool has_return;
    int return_code;
    std::string return_url;

    // cgi
    bool is_cgi;
    std::map<std::string, std::string> cgi_exec;
    std::set<std::string>   cgi_extensions;
    std::string upload_path;
    bool    has_upload_path;

    // routing
    std::string location_path;

    EffectiveConfig() : autoindex(false), max_body_size(0),
                        has_return(false), return_code(302), is_cgi(false), has_upload_path(false) {}
};

ServerRuntimeConfig buildServer(const ServerConfig &raw);
std::vector<ServerRuntimeConfig> buildRuntime(const std::vector<ServerConfig> &raw);
LocationRuntimeConfig buildLocation(const ServerRuntimeConfig &srv, const LocationConfig &raw);
void completeCGI_executors(std::map<std::string, std::string> &cgi_exec);

#endif
