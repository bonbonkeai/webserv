#include "Config/hpp/EffectiveConfig.hpp"
#include <stdexcept>

static bool isNumberString(const std::string& s)
{
    if (s.empty())
        return false;
    for (size_t i = 0; i < s.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
            return false;
    return true;
}


static bool hasDirectiveEither(const std::map<std::string, std::vector<std::string> >& d,
                               const std::string& a,
                               const std::string& b)
{
    return ConfigUtils::hasDirective(d, a) || ConfigUtils::hasDirective(d, b);
}

static std::vector<std::string> getDirectiveEither(const std::map<std::string, std::vector<std::string> >& d,
                                                   const std::string& a,
                                                   const std::string& b)
{
    if (ConfigUtils::hasDirective(d, a))
        return ConfigUtils::getV(d, a);
    return ConfigUtils::getV(d, b);
}

void parseListen(const std::string& value, std::string& host, int& port)
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

LocationRuntimeConfig buildLocation(const ServerRuntimeConfig& srv, const LocationConfig& raw)
{
    LocationRuntimeConfig loc;

    loc.path = raw.path;

    //
    loc.has_root = false;
    loc.has_autoindex = false;
    loc.has_methodes = false;
    loc.has_index = false;
    //

    // héritage
    loc.root = srv.root;
    loc.autoindex = srv.autoindex;
    loc.allow_methodes.clear();
    loc.client_max_body_size = srv.client__max_body_size;

    // override
    if (ConfigUtils::hasDirective(raw.directives, "root"))
    {
        loc.root =
            ConfigUtils::getSimpleV(raw.directives, "root");
        loc.has_root = true;
    }

    if (ConfigUtils::hasDirective(raw.directives, "autoindex"))
    {
        loc.autoindex =
            ConfigUtils::toBool(
                ConfigUtils::getSimpleV(raw.directives, "autoindex"));
        loc.has_autoindex = true;
    }

    if (hasDirectiveEither(raw.directives, "allowed_methods", "allow_methods"))
    {
        loc.allow_methodes =
            getDirectiveEither(raw.directives, "allowed_methods", "allow_methods");
        loc.allow_methodes = true;
    }
    else
    {
        loc.allow_methodes.push_back("GET");
        loc.allow_methodes.push_back("POST");
        loc.allow_methodes.push_back("DELETE");
    }

    if (ConfigUtils::hasDirective(raw.directives, "client_max_body_size"))
        loc.client_max_body_size =
            ConfigUtils::toSize(ConfigUtils::getSimpleV(raw.directives, "client_max_body_size"));

    loc.has_return = false;
    loc.return_code = 302;
    loc.return_url.clear();
    loc.has_cgi = false;
    loc.cgi_exec.clear();
    loc.has_error_pages = false;
    loc.error_pages.clear();

    return loc;
}


ServerRuntimeConfig buildServer(const ServerConfig& raw)
{
    ServerRuntimeConfig srv;

    // listen
    std::string listen =
        ConfigUtils::getSimpleV(raw.directives, "listen");
    parseListen(listen, srv.host, srv.port);
    srv.listen = srv.port;

    // server_name
    srv.server_name.clear();
    if (ConfigUtils::hasDirective(raw.directives, "server_name"))
    {
        std::vector<std::string> names = ConfigUtils::getV(raw.directives, "server_name");
        if (!names.empty())
            srv.server_name = names[0];
    }

    // root
    srv.root = "/var/www";
    if (ConfigUtils::hasDirective(raw.directives, "root"))
        srv.root = ConfigUtils::getSimpleV(raw.directives, "root");

    // autoindex
    srv.autoindex = false;
    if (ConfigUtils::hasDirective(raw.directives, "autoindex"))
        srv.autoindex =
            ConfigUtils::toBool(
                ConfigUtils::getSimpleV(raw.directives, "autoindex"));

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
            ConfigUtils::toSize(ConfigUtils::getSimpleV(raw.directives, "client_max_body_size"));

    // error_page
    srv.error_page.clear();
    if (ConfigUtils::hasDirective(raw.directives, "error_page"))
    {
        std::vector<std::string> values = ConfigUtils::getV(raw.directives, "error_page");
        if (values.size() >= 2)
        {
            bool override_set = false;
            int override_code = 0;
            size_t idx = 0;
            std::vector<int> codes;
            for (; idx < values.size(); ++idx)
            {
                if (!values[idx].empty() && values[idx][0] == '=')
                {
                    override_set = true;
                    if (values[idx].size() > 1)
                        override_code = ConfigUtils::toInt(values[idx].substr(1));
                    ++idx;
                    break;
                }
                if (!isNumberString(values[idx]))
                    break;
                codes.push_back(ConfigUtils::toInt(values[idx]));
            }
            if (!codes.empty() && idx < values.size())
            {
                std::string uri = values[idx];
                for (size_t i = 0; i < codes.size(); ++i)
                {
                    ErrorPageRule rule;
                    rule.uri = uri;
                    rule.override_set = override_set;
                    rule.override_code = override_code;
                    srv.error_page[codes[i]] = rule;
                }
            }
        }
    }

    return srv;
}

std::vector<ServerRuntimeConfig> buildRuntime(const std::vector<ServerConfig>& raw)
{
    std::vector<ServerRuntimeConfig> out;
    for (size_t i = 0; i < raw.size(); ++i)
    {
        ServerRuntimeConfig srv = buildServer(raw[i]);
        srv.locations.clear();
        for (size_t j = 0; j < raw[i].locations.size(); ++j)
            srv.locations.push_back(buildLocationRuntime(raw[i].locations[j], srv));
        out.push_back(srv);
    }
    return out;
}
