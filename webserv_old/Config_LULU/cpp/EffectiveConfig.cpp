#include "EffectiveConfig.hpp"

void parseListen( const std::string& value, std::string& host, int& port)
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

    // héritage
    loc.root = srv.root;
    loc.autoindex = srv.autoindex;
    loc.allow_methodes.clear();

    // override
    if (ConfigUtils::hasDirective(raw.directives, "root"))
        loc.root =
            ConfigUtils::getSimpleV(raw.directives, "root");

    if (ConfigUtils::hasDirective(raw.directives, "autoindex"))
        loc.autoindex =
            ConfigUtils::toBool(
                ConfigUtils::getSimpleV(raw.directives, "autoindex"));

    if (ConfigUtils::hasDirective(raw.directives, "allowed_methods"))
        loc.allow_methodes =
            ConfigUtils::getV(raw.directives, "allowed_methods");
    else
    {
        loc.allow_methodes.push_back("GET");
        loc.allow_methodes.push_back("POST");
    }

    return loc;
}


ServerRuntimeConfig buildServer(const ServerConfig& raw)
{
    ServerRuntimeConfig srv;

    // listen
    std::string listen =
        ConfigUtils::getSimpleV(raw.directives, "listen");
    parseListen(listen, srv.host, srv.port);

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

    return srv;
}

