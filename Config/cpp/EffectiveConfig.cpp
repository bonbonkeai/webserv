
#include "Config/hpp/EffectiveConfig.hpp"
#include <stdexcept>
#include <cctype>
#include <iostream>

static bool isNumberString(const std::string &s)
{
    if (s.empty())
        return false;
    for (size_t i = 0; i < s.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
            return false;
    return true;
}

static bool hasDirectiveEither(const std::map<std::string, std::vector<std::string> > &d,
                               const std::string &a,
                               const std::string &b)
{
    return ConfigUtils::hasDirective(d, a) || ConfigUtils::hasDirective(d, b);
}

static std::vector<std::string> getDirectiveEither(const std::map<std::string, std::vector<std::string> > &d,
                                                   const std::string &a,
                                                   const std::string &b)
{
    if (ConfigUtils::hasDirective(d, a))
        return ConfigUtils::getV(d, a);
    return ConfigUtils::getV(d, b);
}

void parseListen(const std::string &value, std::string &host, int &port)
{
    size_t pos = value.find(':');

    if (pos == std::string::npos)
    {
        host = "0.0.0.0";
        port = ConfigUtils::toInt(value);
    }
    else
    {
        host = value.substr(0, pos);
        port = ConfigUtils::toInt(value.substr(pos + 1));
    }

    if (port < 1 || port > 65535)
        throw std::runtime_error("Invalid listen port");
}

void completeCGI_executors(std::map<std::string, std::string> &cgi_exec)
{
    static std::map<std::string, std::string> DEFAULT_INTER;
    if (DEFAULT_INTER.empty())
    {
        DEFAULT_INTER.insert(std::make_pair(".php", "/usr/bin/php-cgi"));
        DEFAULT_INTER.insert(std::make_pair(".py", "/usr/bin/python3"));
        DEFAULT_INTER.insert(std::make_pair(".sh", "/bin/bash"));
        DEFAULT_INTER.insert(std::make_pair(".cgi", "/usr/bin/env"));
    }
    for (std::map<std::string, std::string>::iterator it = cgi_exec.begin(); it != cgi_exec.end(); ++it)
    {
        const std::string &ext = it->first;
        std::string &inter = it->second;

        if (inter.empty())
        {
            std::map<std::string, std::string>::const_iterator default_it = DEFAULT_INTER.find(ext);
            if (default_it != DEFAULT_INTER.end())
                inter = default_it->second;
            else
                throw std::runtime_error("cgi extension " + ext + " need an interpreter");
        }
    }
}

/*
 * 解析 error_page 指令
 *
 * 支持类似：
 * error_page 404 /html_error/404.html;
 * error_page 403 404 /html_error/common.html;
 * error_page 502 =200 /some/path.html;
 *
 * 如果 ConfigUtils::getV(raw.directives, "error_page") 返回的是扁平化后的 vector，
 * 这个函数会把里面的多组规则全部读出来。
 */
static void parseErrorPageRules(const std::vector<std::string> &values,
                                std::map<int, ErrorPageRule> &out)
{
    size_t i = 0;

    while (i < values.size())
    {
        std::vector<int> codes;
        bool override_set = false;
        int override_code = 0;

        // 1) 读取一个或多个状态码
        while (i < values.size() && isNumberString(values[i]))
        {
            codes.push_back(ConfigUtils::toInt(values[i]));
            ++i;
        }

        // 2) 可选的 =xxx
        if (i < values.size() && !values[i].empty() && values[i][0] == '=')
        {
            override_set = true;
            if (values[i].size() > 1)
                override_code = ConfigUtils::toInt(values[i].substr(1));
            ++i;
        }

        // 3) 接下来必须有 uri
        if (codes.empty() || i >= values.size())
            break;

        std::string uri = values[i];
        ++i;

        for (size_t k = 0; k < codes.size(); ++k)
        {
            ErrorPageRule rule;
            rule.uri = uri;
            rule.override_set = override_set;
            rule.override_code = override_code;
            out[codes[k]] = rule;
        }
    }
}

LocationRuntimeConfig buildLocation(const ServerRuntimeConfig &srv, const LocationConfig &raw)
{
    LocationRuntimeConfig loc;

    loc.path = raw.path;

    // héritage
    loc.root = srv.root;
    loc.has_root = false;
    loc.autoindex = srv.autoindex;
    loc.has_autoindex = false;
    loc.allow_methodes = srv.allowed_methods;
    loc.has_methodes = false;
    loc.client_max_body_size = srv.client__max_body_size;
    loc.has_client_max_body_size = false;
    loc.index = srv.index;
    loc.has_index = false;
    loc.has_alias = false;
    loc.alias.clear();

    // 关键：先继承 server 级 error_page
    loc.error_pages = srv.error_page;
    loc.has_error_pages = !loc.error_pages.empty();

    // override
    if (ConfigUtils::hasDirective(raw.directives, "index"))
    {
        loc.index = ConfigUtils::getV(raw.directives, "index");
        loc.has_index = true;
    }

    // traitement alias or root
    if (ConfigUtils::hasDirective(raw.directives, "alias"))
    {
        loc.alias = ConfigUtils::getSimpleV(raw.directives, "alias");
        loc.has_alias = true;

        // alias 和 root 互斥
        loc.root.clear();
        loc.has_root = false;
    }
    else if (ConfigUtils::hasDirective(raw.directives, "root"))
    {
        loc.root = ConfigUtils::getSimpleV(raw.directives, "root");
        loc.has_root = true;
    }

    if (loc.has_alias && loc.has_root)
        throw std::runtime_error("alias and root cannot be use together");

    if (ConfigUtils::hasDirective(raw.directives, "autoindex"))
    {
        loc.autoindex = ConfigUtils::toBool(ConfigUtils::getSimpleV(raw.directives, "autoindex"));
        loc.has_autoindex = true;
    }

    if (hasDirectiveEither(raw.directives, "allowed_methods", "allow_methods"))
    {
        loc.allow_methodes = getDirectiveEither(raw.directives, "allowed_methods", "allow_methods");
        loc.has_methodes = true;
    }
    else
    {
        loc.allow_methodes.clear();
        loc.allow_methodes.push_back("GET");
        loc.allow_methodes.push_back("POST");
        loc.allow_methodes.push_back("DELETE");
        loc.has_methodes = false;
    }

    if (ConfigUtils::hasDirective(raw.directives, "client_max_body_size"))
    {
        loc.client_max_body_size = ConfigUtils::toSize(ConfigUtils::getValue(raw.directives, "client_max_body_size"));
        loc.has_client_max_body_size = true;
    }

    loc.has_return = false;
    loc.return_code = 302;
    loc.return_url.clear();
    if (ConfigUtils::hasDirective(raw.directives, "return"))
    {
        std::vector<std::string> values = ConfigUtils::getV(raw.directives, "return");
        if (!values.empty())
        {
            if (isNumberString(values[0]))
            {
                loc.return_code = ConfigUtils::toInt(values[0]);
                if (values.size() >= 2)
                    loc.return_url = values[1];
            }
            else
            {
                loc.return_code = 302;
                loc.return_url = values[0];
            }
            if (!loc.return_url.empty())
                loc.has_return = true;
        }
    }

    // --- cgi ---
    loc.has_cgi = false;
    loc.cgi_exec.clear();
    if (ConfigUtils::hasDirective(raw.directives, "cgi"))
    {
        std::vector<std::string> values = ConfigUtils::getV(raw.directives, "cgi");
        size_t i = 0;
        while (i < values.size())
        {
            std::string ext = values[i];
            std::string exec = "";
            if (i + 1 < values.size() && !values[i + 1].empty() && values[i + 1][0] != '.')
            {
                exec = values[i + 1];
                i += 2;
            }
            else
            {
                exec = "";
                i += 1;
            }
            if (!ext.empty())
                loc.cgi_exec[ext] = exec;
        }
    }
    if (!loc.cgi_exec.empty())
    {
        loc.has_cgi = true;
        std::string effective_root = loc.alias.empty() ? loc.root : loc.alias;
        (void)effective_root;
        completeCGI_executors(loc.cgi_exec);
    }

    // make sure .cgi for upload
    if (loc.has_cgi && loc.cgi_exec.find(".cgi") == loc.cgi_exec.end())
    {
        loc.cgi_exec[".cgi"] = "/usr/bin/env";
    }

    // --- error_page ---
    // location 自己写了 error_page 时，覆盖/追加 server 继承来的映射
    if (ConfigUtils::hasDirective(raw.directives, "error_page"))
    {
        std::vector<std::string> values = ConfigUtils::getV(raw.directives, "error_page");
        parseErrorPageRules(values, loc.error_pages);
        loc.has_error_pages = !loc.error_pages.empty();
    }

    // upload_path
    if (ConfigUtils::hasDirective(raw.directives, "upload_path"))
    {
        loc.upload_path = ConfigUtils::getSimpleV(raw.directives, "upload_path");
        loc.has_upload_path = true;
    }
    else if (!srv.upload_path.empty())
    {
        loc.upload_path = srv.upload_path;
        loc.has_upload_path = true;
    }
    else
    {
        loc.upload_path.clear();
        loc.has_upload_path = false;
    }

    return (loc);
}

ServerRuntimeConfig buildServer(const ServerConfig &raw)
{
    ServerRuntimeConfig srv;

    // listen
    std::string listen = ConfigUtils::getSimpleV(raw.directives, "listen");
    parseListen(listen, srv.host, srv.port);
    srv.listen = srv.port;

    // server_name
    srv.server_name.clear();
    if (ConfigUtils::hasDirective(raw.directives, "server_name"))
    {
        std::vector<std::string> names = ConfigUtils::getV(raw.directives, "server_name");
        if (names.empty())
            throw std::runtime_error("servername is empty");
        if (names.size() > 1)
            throw std::runtime_error("multipe servername find");
        srv.server_name = names[0];
    }
    else
        srv.server_name = "";

    // root
    srv.root = "/var/www";
    if (ConfigUtils::hasDirective(raw.directives, "root"))
        srv.root = ConfigUtils::getSimpleV(raw.directives, "root");

    // autoindex
    srv.autoindex = false;
    if (ConfigUtils::hasDirective(raw.directives, "autoindex"))
        srv.autoindex = ConfigUtils::toBool(ConfigUtils::getSimpleV(raw.directives, "autoindex"));

    // index
    if (ConfigUtils::hasDirective(raw.directives, "index"))
        srv.index = ConfigUtils::getV(raw.directives, "index");
    else
        srv.index.push_back("index.html");

    // allowed methods
    srv.allowed_methods.clear();
    if (hasDirectiveEither(raw.directives, "allowed_methods", "allow_methods"))
        srv.allowed_methods = getDirectiveEither(raw.directives, "allowed_methods", "allow_methods");
    else
    {
        srv.allowed_methods.push_back("GET");
        srv.allowed_methods.push_back("POST");
        srv.allowed_methods.push_back("DELETE");
    }

    // client max body size
    srv.client__max_body_size = 10 * 1024 * 1024;
    if (ConfigUtils::hasDirective(raw.directives, "client_max_body_size"))
        srv.client__max_body_size =
            ConfigUtils::toSize(ConfigUtils::getValue(raw.directives, "client_max_body_size"));

    // error_page
    srv.error_page.clear();
    if (ConfigUtils::hasDirective(raw.directives, "error_page"))
    {
        std::vector<std::string> values = ConfigUtils::getV(raw.directives, "error_page");
        parseErrorPageRules(values, srv.error_page);
    }

    // upload path
    srv.upload_path = "www/upload";
    if (ConfigUtils::hasDirective(raw.directives, "upload_path"))
        srv.upload_path = ConfigUtils::getSimpleV(raw.directives, "upload_path");

    return srv;
}

std::vector<ServerRuntimeConfig> buildRuntime(const std::vector<ServerConfig> &raw)
{
    std::vector<ServerRuntimeConfig> out;
    for (size_t i = 0; i < raw.size(); ++i)
    {
        ServerRuntimeConfig srv = buildServer(raw[i]);
        srv.locations.clear();
        for (size_t j = 0; j < raw[i].locations.size(); ++j)
            srv.locations.push_back(buildLocation(srv, raw[i].locations[j]));
        out.push_back(srv);
    }
    return out;
}