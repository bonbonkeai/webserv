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
const ServerRuntimeConfig &Routing::selectS(const HTTPRequest &req, int listen_port) const
{
    if (_serveurs.empty())
        throw std::runtime_error("No servers available in Routing::selectS");

    std::string host = req.headers.count("host") ? req.headers.at("host") : "";
    const ServerRuntimeConfig* first_port_match = NULL;
    for (size_t i = 0; i < _serveurs.size(); i++)
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
    const LocationRuntimeConfig *best = nullptr;
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

// -------------------
// Fusion ServerConfig + LocationConfig
// -------------------

LocationRuntimeConfig buildLocationRuntime(const LocationConfig &loc, const ServerRuntimeConfig &server)
{
    LocationRuntimeConfig rt;
    (void)server;

    rt.path = loc.path;

    if (ConfigUtils::hasDirective(loc.directives, "root"))
    {
        rt.root = ConfigUtils::getSimpleV(loc.directives, "root");
        rt.has_root = true;
    }
    else
        rt.has_root = false;

    if (ConfigUtils::hasDirective(loc.directives, "autoindex"))
    {
        rt.autoindex = ConfigUtils::toBool(
            ConfigUtils::getSimpleV(loc.directives, "autoindex"));
        rt.has_autoindex = true;
    }
    else
        rt.has_autoindex = false;

    if (ConfigUtils::hasDirective(loc.directives, "allowed_methods"))
    {
        rt.allow_methodes = ConfigUtils::getV(loc.directives, "allowed_methods");
        rt.has_methodes = true;
    }
    else if (ConfigUtils::hasDirective(loc.directives, "allow_methods"))
    {
        rt.allow_methodes = ConfigUtils::getV(loc.directives, "allow_methods");
        rt.has_methodes = true;
    }
    else
        rt.has_methodes = false;

    if (ConfigUtils::hasDirective(loc.directives, "index"))
    {
        rt.index = ConfigUtils::getV(loc.directives, "index");
        rt.has_index = true;
    }
    else
        rt.has_index = false;

    if (ConfigUtils::hasDirective(loc.directives, "client_max_body_size"))
    {
        rt.client_max_body_size =
            ConfigUtils::toSize(
                ConfigUtils::getSimpleV(loc.directives, "client_max_body_size"));
        rt.has_client_max_body_size = true;
    }
    else
        rt.has_client_max_body_size = false;

    rt.has_return = false;
    rt.return_code = 302;
    rt.return_url.clear();
    if (ConfigUtils::hasDirective(loc.directives, "return"))
    {
        std::vector<std::string> values = ConfigUtils::getV(loc.directives, "return");
        if (!values.empty())
        {
            bool is_num = true;
            for (size_t i = 0; i < values[0].size(); ++i)
            {
                if (!std::isdigit(static_cast<unsigned char>(values[0][i])))
                {
                    is_num = false;
                    break;
                }
            }
            if (is_num)
            {
                rt.return_code = ConfigUtils::toInt(values[0]);
                if (values.size() >= 2)
                    rt.return_url = values[1];
            }
            else
            {
                rt.return_code = 302;
                rt.return_url = values[0];
            }
            if (!rt.return_url.empty())
                rt.has_return = true;
        }
    }

    rt.has_cgi = false;
    rt.cgi_exec.clear();
    if (ConfigUtils::hasDirective(loc.directives, "cgi"))
    {
        std::vector<std::string> values = ConfigUtils::getV(loc.directives, "cgi");
        if (values.size() == 1)
        {
            rt.cgi_exec[values[0]] = "";
        }
        else
        {
            for (size_t i = 0; i + 1 < values.size(); i += 2)
            {
                std::string ext = values[i];
                std::string exec = values[i + 1];
                if (!ext.empty())
                    rt.cgi_exec[ext] = exec;
            }
        }
        if (!rt.cgi_exec.empty())
            rt.has_cgi = true;
    }

    rt.has_error_pages = false;
    rt.error_pages.clear();
    if (ConfigUtils::hasDirective(loc.directives, "error_page"))
    {
        std::vector<std::string> values = ConfigUtils::getV(loc.directives, "error_page");
        if (values.size() >= 2)
        {
            std::string uri = values[values.size() - 1];
            bool override_set = false;
            int override_code = 0;
            size_t codes_end = values.size() - 1;
            if (codes_end >= 1 && values[codes_end - 1].size() >= 1 && values[codes_end - 1][0] == '=')
            {
                override_set = true;
                std::string v = values[codes_end - 1];
                if (v.size() > 1)
                    override_code = ConfigUtils::toInt(v.substr(1));
                codes_end -= 1;
            }
            for (size_t i = 0; i < codes_end; ++i)
            {
                int code = ConfigUtils::toInt(values[i]);
                ErrorPageRule rule;
                rule.uri = uri;
                rule.override_set = override_set;
                rule.override_code = override_code;
                rt.error_pages[code] = rule;
            }
            rt.has_error_pages = !rt.error_pages.empty();
        }
    }

    return rt;
}

EffectiveConfig Routing::resolve(const HTTPRequest& req, int listen_port) const
{
    const ServerRuntimeConfig& server = selectS(req, listen_port);
    const LocationRuntimeConfig* loc = matchLocation(server, req.uri);

    EffectiveConfig cfg;

    cfg.root = (loc && loc->has_root) ? loc->root : server.root;
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

    cfg.has_return = (loc && loc->has_return);
    if (cfg.has_return)
    {
        cfg.return_code = loc->return_code;
        cfg.return_url = loc->return_url;
    }

    return cfg;
}


// -------------------
// Pour RuntimeConfig (version runtime des locations)
// -------------------
LocationRuntimeConfig *matchLocation(ServerRuntimeConfig &srv, const std::string &path)
{
    LocationRuntimeConfig *best = nullptr;
    size_t bestLen = 0;

    for (size_t i = 0; i < srv.locations.size(); i++)
    {
        const std::string &locPath = srv.locations[i].path;

        if (path.compare(0, locPath.size(), locPath) == 0 && locPath.size() > bestLen)
        {
            best = &srv.locations[i];
            bestLen = locPath.size();
        }
    }
    return best;
}
