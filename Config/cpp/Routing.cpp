#include "Routing.hpp"
#include <stdexcept>

// -------------------
// Constructeur / Destructeur
// -------------------
Routing::Routing(const std::vector<ServerRuntimeConfig> &serveurs)
    : _serveurs(serveurs)
{
}

Routing::~Routing() {}

// -------------------
// Sélection du serveur correspondant au Host
// -------------------
const ServerRuntimeConfig &Routing::selectS(const HTTPRequest &req) const
{
    if (_serveurs.empty())
        throw std::runtime_error("No servers available in Routing::selectS");

    std::string host = req.headers.count("Host") ? req.headers.at("Host") : "";

    for (size_t i = 0; i < _serveurs.size(); i++)
    {
        if (_serveurs[i].matchesHost(host))
            return _serveurs[i];
    }
    // Si aucun host ne match, retourne le premier serveur (default)
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
    else
        rt.has_methodes = false;

    if (ConfigUtils::hasDirective(loc.directives, "index"))
    {
        rt.index = ConfigUtils::getV(loc.directives, "index");
        rt.has_index = true;
    }
    else
        rt.has_index = false;

    return rt;
}

EffectiveConfig Routing::resolve(const HTTPRequest& req) const
{
    const ServerRuntimeConfig& server = selectS(req);
    const LocationRuntimeConfig* loc = matchLocation(server, req.uri);

    EffectiveConfig cfg;

    cfg.root = (loc && loc->has_root) ? loc->root : server.root;
    cfg.index = (loc && loc->has_index) ? loc->index : server.index;
    cfg.autoindex = (loc && loc->has_autoindex) ? loc->autoindex : server.autoindex;
    cfg.allowed_methods = (loc && loc->has_methodes)
        ? loc->allow_methodes
        : server.allowed_methods;

    cfg.error_pages = server.error_page;

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
