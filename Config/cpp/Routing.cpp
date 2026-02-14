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
            // prefix 的匹配检查
            if (uri.size() == path.size() || uri[path.size()] == '/')
            {
                if (path.size() > bestLen)
                {
                    best = &server.locations[i];
                    bestLen = path.size();
                }
            }
        }
    }
    return best;
}

/*EffectiveConfig Routing::resolve(const HTTPRequest &req, int listen_port) const
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
    cfg.location_path = (loc && loc->has_upload_path) ? loc->upload_path : "";
    return cfg;
}*/

std::string Routing::get_extension(const std::string &fs_path)
{
    size_t slash = fs_path.find_last_of('/');
    size_t point = fs_path.find_last_of('.');

    if (point == std::string::npos)
        return "";
    if (slash != std::string::npos && point < slash)
        return "";
    return fs_path.substr(point);
}

EffectiveConfig Routing::resolve(HTTPRequest &req, int listen_port, RouteResult &rout) const
{
    const ServerRuntimeConfig &server = selectS(req, listen_port);
    const LocationRuntimeConfig *loc = matchLocation(server, req.path);
    EffectiveConfig cfg;

    // herite from serverruntimeonfig
    cfg.server_port = server.port;
    cfg.server_name = server.server_name;

    // root or alias
    cfg.root = (loc && loc->has_root) ? loc->root : server.root;
    cfg.alias = (loc && loc->has_alias) ? loc->alias : "";

    cfg.index = (loc && loc->has_index) ? loc->index : server.index;
    cfg.autoindex = (loc && loc->has_autoindex) ? loc->autoindex : server.autoindex;

    cfg.allowed_methods = (loc && loc->has_methodes) ? loc->allow_methodes : server.allowed_methods;
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
    // cgi
    cfg.is_cgi = (loc && loc->has_cgi);
    cfg.cgi_exec = (loc && loc->has_cgi) ? loc->cgi_exec : std::map<std::string, std::string>();
    cfg.cgi_extensions = (loc && !loc->cgi_extensions.empty()) ? loc->cgi_extensions : std::set<std::string>();

    // upload_path
    cfg.upload_path.clear();
    if (loc && loc->has_upload_path)
        cfg.upload_path = loc->upload_path;
    else if (!server.upload_path.empty())
        cfg.upload_path = server.upload_path;
    cfg.has_upload_path = !cfg.upload_path.empty();

    cfg.has_return = (loc && loc->has_return);
    if (cfg.has_return)
    {
        cfg.return_code = loc->return_code;
        cfg.return_url = loc->return_url;
        rout.action = ACTION_REDIRECT;
        rout.redirect_code = cfg.return_code;
        rout.redirect_url = cfg.return_url;
        return cfg;
    }

    // fs_path
    if (loc && loc->has_alias)
    {
        std::string suffix = req.path.substr(loc->path.size());

        std::string base = loc->alias;
        if (!base.empty() && base[base.size() - 1] != '/')
            base += '/';

        if (!suffix.empty() && suffix[0] == '/')
            suffix.erase(0, 1);

        rout.fs_path = base + suffix;
    }
    else
    {
        std::string base = (loc && loc->has_root) ? loc->root : server.root;

        if (!base.empty() && base[base.size() - 1] == '/')
            base.erase(base.size() - 1);

        rout.fs_path = base + req.path;
    }
    // prepare script name, path info for cgi
    if (cfg.is_cgi)
    {
        std::string uri_after_loc;

        if (loc)
            uri_after_loc = req.path.substr(loc->path.size());
        else
            uri_after_loc = req.path;

        if (uri_after_loc.empty())
            uri_after_loc = "/";
        else if (uri_after_loc[0] != '/')
            uri_after_loc = "/" + uri_after_loc;

        size_t pos = 1;
        std::string current;

        while (true)
        {
            size_t slash = uri_after_loc.find('/', pos);

            std::string part;
            if (slash == std::string::npos)
                part = uri_after_loc.substr(pos);
            else
                part = uri_after_loc.substr(pos, slash - pos);

            current += "/" + part;

            std::string ext = get_extension(part);

            if (!ext.empty() && cfg.cgi_extensions.count(ext))
            {
                rout.script_name = loc ? loc->path + current : current;

                if (slash == std::string::npos)
                    rout.path_info = "";
                else
                    rout.path_info = uri_after_loc.substr(slash);

                // 重新计算 CGI 专用 fs_path
                if (loc && loc->has_alias)
                {
                    std::string base = loc->alias;
                    if (!base.empty() && base[base.size() - 1] != '/')
                        base += '/';

                    std::string script_suffix = current;
                    if (!script_suffix.empty() && script_suffix[0] == '/')
                        script_suffix.erase(0, 1);

                    rout.fs_path = base + script_suffix;
                }
                else
                {
                    std::string base = (loc && loc->has_root) ? loc->root : server.root;
                    if (!base.empty() && base[base.size() - 1] == '/')
                        base.erase(base.size() - 1);

                    rout.fs_path = base + rout.script_name;
                }

                rout.action = ACTION_CGI;
                return cfg;
            }

            if (slash == std::string::npos)
                break;

            pos = slash + 1;
        }
    }

    if (cfg.autoindex)
        rout.action = ACTION_AUTOINDEX;
    else if (req.method == "POST" && !cfg.upload_path.empty())
    {
        rout.action = ACTION_UPLOAD;
    }
    else
        rout.action = ACTION_STATIC;
    return cfg;
}
