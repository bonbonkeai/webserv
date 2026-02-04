#include "Config/hpp/ConfigUtils.hpp"

ConfigUtils::ConfigUtils() {}
ConfigUtils::~ConfigUtils() {}

/**
 * Conversion string en int
 */
int ConfigUtils::toInt(const std::string& str)
{
    if(str.empty())
        throw std::runtime_error("Empty number");
    for(size_t i = 0; i < str.size(); i++)
        if (!std::isdigit(static_cast<unsigned char>(str[i])))
            throw std::runtime_error(std::string("Invalid number: ") + str);
    return std::atoi(str.c_str());
}

/**
 * Pour verifier le maximum size du client
 */
size_t ConfigUtils::toSize(const std::string& str)
{
    if(str.empty())
        throw std::runtime_error("Empty size");
    std::string digits = str;
    size_t multiplier = 1;
    char last = str[str.size() - 1];
    if (last == 'k' || last == 'K' || last == 'm' || last == 'M')
    {
        digits = str.substr(0, str.size() - 1);
        if (last == 'k' || last == 'K')
            multiplier = 1024;
        else
            multiplier = 1024 * 1024;
    }
    if (digits.empty())
        throw std::runtime_error("Invalid size");
    for(size_t i=0; i<digits.size(); i++)
        if (!std::isdigit(static_cast<unsigned char>(digits[i])))
            throw std::runtime_error("Invalid size");
    return static_cast<size_t>(std::atoi(digits.c_str())) * multiplier;
}

/**
 * Verifier si c est on ou off
 */
bool ConfigUtils::toBool(const std::string& str)
{
    if (str == "on") return true;
    else if (str == "off") return false;
    else throw std::runtime_error("Expect on/off");
}


bool ConfigUtils::hasDirective(const std::map<std::string, std::vector<std::string> >& d, const std::string& cle)
{
    return d.find(cle) != d.end();
}

/**
 * une directive avec une seule valeur
 */
std::string ConfigUtils::getSimpleV(const std::map<std::string, std::vector<std::string> >& d, const std::string& cle)
{
    std::map<std::string, std::vector<std::string> >::const_iterator it = d.find(cle);

    if (it == d.end())
        throw std::runtime_error("Directive not found " + cle);
    if (it->second.size() != 1)
        throw std::runtime_error("Directive must have a seul valeur " + cle);
    return it->second[0];
}

/**
 * Directive avec plusieurs valeurs
 */
std::vector<std::string> ConfigUtils::getV(const std::map<std::string, std::vector<std::string> >& d, const std::string& cle)
{
    std::map<std::string, std::vector<std::string> >::const_iterator it=d.find(cle);

    if (it == d.end())
        throw std::runtime_error("Directive not found " + cle);
    return it->second;
}

/**
 * Validation du location
 */
void ConfigUtils::validateL(ServerConfig& serveurs, LocationConfig& l)
{
    (void)serveurs;
    if (hasDirective(l.directives, "allowed_methods") || hasDirective(l.directives, "allow_methods"))
    {
        std::vector<std::string> method = hasDirective(l.directives, "allowed_methods")
            ? getV(l.directives, "allowed_methods")
            : getV(l.directives, "allow_methods");

        for(size_t i=0; i<method.size();i++)
        {
            if(method[i] != "GET" && method[i] != "POST" && method[i] != "DELETE")
                throw std::runtime_error("INVALID HTTP method");
        }
    }

}

/**
 * Validation du serveur
 */
void ConfigUtils::validateS(ServerConfig& serveurs)
{
    if (!hasDirective(serveurs.directives, "listen"))
        throw std::runtime_error("Serveur miss listen directive");
    
    for (size_t i=0; i<serveurs.locations.size(); i++)
        validateL(serveurs,serveurs.locations[i]);
}


/**
 * Valider global du serveurs
 */
void ConfigUtils::validate(std::vector<ServerConfig>& serveurs)
{
    if(serveurs.empty())
        throw std::runtime_error("On ne trouve pas de serveur");
    for(size_t i = 0; i<serveurs.size(); i++)
        validateS(serveurs[i]);
}
