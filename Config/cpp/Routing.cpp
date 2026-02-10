#include "Config/hpp/Routing.hpp"
#include <stdexcept>
#include <cctype>

// -------------------
// Constructeur / Destructeur
// -------------------
Routing::Routing(const std::vector<ServerRuntimeConfig> &serveurs)
    : _serveurs(serveurs)
{
}

// -------------------
// Sélection du serveur correspondant au Host
// -------------------
// const ServerRuntimeConfig &Routing::selectS(const HTTPRequest &req, int listen_port) const
// {
//     if (_serveurs.empty())
//         throw std::runtime_error("No servers available in Routing::selectS");

//     std::string host = req.headers.count("host") ? req.headers.at("host") : "";
//     const ServerRuntimeConfig* first_port_match = NULL;
//     for (size_t i = 0; i < _serveurs.size(); i++)
//     {
//         if (_serveurs[i].port != listen_port)
//             continue;
//         if (!first_port_match)
//             first_port_match = &_serveurs[i];
//         if (_serveurs[i].matchesHost(host))
//             return _serveurs[i];
//     }
//     if (first_port_match)
//         return *first_port_match;
//     return _serveurs[0];
// }
const ServerRuntimeConfig &Routing::selectS(const HTTPRequest &req, int listen_port) const
{
    if (_serveurs.empty())
        throw std::runtime_error("No servers available in Routing::selectS");

    // 1) header key 大小写不敏感，我都转小写了：host
    std::string host = "";
    std::map<std::string, std::string>::const_iterator it = req.headers.find("host");
    if (it != req.headers.end())
        host = it->second;
    else
        host = "";
    // 2) 去掉前后空格
    // 这里给一个最小实现：只处理首尾空格/Tab
    ltrimSpaces(host);
    rtrimSpaces(host);
    // 3) lower（避免 server_name 比较大小写不一致）
    toLowerInPlace(host);
    // 4) 去掉端口：处理 "example.local:8080" -> "example.local"
    //    额外处理 IPv6: "[::1]:8080" -> "::1"
    if (!host.empty())
    {
        if (host[0] == '[')
        {
            // IPv6 bracket form: [::1]:8080
            std::size_t rb = host.find(']');
            if (rb != std::string::npos)
            {
                host = host.substr(1, rb - 1); // 取出 ::1
            }
        }
        else
        {
            std::size_t colon = host.find(':');
            if (colon != std::string::npos)
                host = host.substr(0, colon);
        }
    }
    // 5) 逐 server 匹配
    const ServerRuntimeConfig *first_port_match = NULL;
    for (size_t i = 0; i < _serveurs.size(); ++i)
    {
        if (_serveurs[i].port != listen_port)
            continue;
        if (!first_port_match)
            first_port_match = &_serveurs[i];
        if (_serveurs[i].matchesHost(host))
            return _serveurs[i];
    }
    if (first_port_match)
        return *first_port_match;
    return _serveurs[0];
}

// -------------------
// Retourne la location la plus spécifique (longest prefix match)
// -------------------
const LocationRuntimeConfig *Routing::matchLocation(const ServerRuntimeConfig &server, const std::string &uri) const
{
    const LocationRuntimeConfig *best = NULL;
    size_t bestLen = 0;

    for (size_t i = 0; i < server.locations.size(); i++)
    {
        const std::string &path = server.locations[i].path;

        // Vérifie si l'URI commence par le path
        if (uri.compare(0, path.size(), path) == 0)
        {
            if (path.size() > bestLen)
            {
                best = &server.locations[i];
                bestLen = path.size();
            }
        }
    }
    return best;
}

EffectiveConfig Routing::resolve(const HTTPRequest &req, int listen_port) const
{
    const ServerRuntimeConfig &server = selectS(req, listen_port);
    // const LocationRuntimeConfig* loc = matchLocation(server, req.uri);
    const LocationRuntimeConfig *loc = matchLocation(server, req.path);

    EffectiveConfig cfg;

    // server info
    cfg.server_port = server.port;
    cfg.server_name = server.server_name;

    cfg.root = (loc && loc->has_root) ? loc->root : server.root;
    // alias traitement
    cfg.alias = (loc && loc->has_alias) ? loc->alias : "";

    cfg.index = (loc && loc->has_index) ? loc->index : server.index;
    cfg.autoindex = (loc && loc->has_autoindex) ? loc->autoindex : server.autoindex;
    cfg.allowed_methods = (loc && loc->has_methodes)
                              ? loc->allow_methodes
                              : server.allowed_methods;

    cfg.error_pages = server.error_page;
    if (loc && loc->has_error_pages)
    {
        for (std::map<int, ErrorPageRule>::const_iterator it = loc->error_pages.begin();
             it != loc->error_pages.end(); ++it)
            cfg.error_pages[it->first] = it->second;
    }
    cfg.max_body_size = (loc && loc->has_client_max_body_size)
                            ? loc->client_max_body_size
                            : server.client__max_body_size;

    cfg.is_cgi = (loc && loc->has_cgi);
    cfg.cgi_exec = (loc && loc->has_cgi) ? loc->cgi_exec : std::map<std::string, std::string>();
    cfg.cgi_extensions = (loc && loc->cgi_extensions.empty()) ? loc->cgi_extensions : std::set<std::string>();

    cfg.has_return = (loc && loc->has_return);
    if (cfg.has_return)
    {
        cfg.return_code = loc->return_code;
        cfg.return_url = loc->return_url;
    }

    // location path
    cfg.location_path = loc ? loc->path : "";
    return cfg;
}
