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
//Tokenizer 的 read_number() 允许 k / m，但你的toSize() 现在只认纯数字
//如果配置写 client_max_body_size 10m;，会直接throw
std::size_t ConfigUtils::toSize(const std::string& s)
{
    if (s.empty())
        throw std::runtime_error("empty size");
    char suffix = s[s.size()-1];
    std::string num = s;
    std::size_t mult = 1;
    if (suffix == 'k' || suffix == 'K')
    {
        mult = 1024ULL; num = s.substr(0, s.size()-1);
    }
    else if (suffix == 'm' || suffix == 'M')
    {
        mult = 1024ULL*1024ULL; num = s.substr(0, s.size()-1);
    }
    for (size_t i=0;i<num.size();++i)
        if (!std::isdigit((unsigned char)num[i])) throw std::runtime_error("invalid size: " + s);
    unsigned long long v = std::strtoull(num.c_str(), 0, 10);
    return (std::size_t)(v * mult);
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
 * get value
 */
std::string ConfigUtils::getValue(const std::map<std::string, std::vector<std::string> > &d, const std::string &cle)
{
    std::map<std::string, std::vector<std::string> >::const_iterator it = d.find(cle);
    if (it == d.end() || it->second.empty())
        return "";
    if (it->second.size() == 1)
        return it->second[0];

    std::string resultat;
    for(size_t i = 0; i < it->second.size(); ++i)
    {
        if (i > 0)
            resultat += "";
        resultat += it->second[i];
    }
    return resultat;
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
    
    std::set<std::string> seen_paths;
    for (size_t i = 0; i < serveurs.locations.size(); ++i) {
        if (!seen_paths.insert(serveurs.locations[i].path).second) {
            throw std::runtime_error("Duplicate location path: " + serveurs.locations[i].path);
        }
    }
    
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
    //检查 listen + server_name 冲突 如果重复 只能是erreur 
    for (size_t i = 0; i < serveurs.size(); i++)
    {
        std::string listen_i = ConfigUtils::getSimpleV(serveurs[i].directives, "listen");
        std::string name_i = "";

        if(hasDirective(serveurs[i].directives, "server_name"))
            name_i = getSimpleV(serveurs[i].directives, "server_name");
        for (size_t j = i + 1; j < serveurs.size(); ++j)
        {
            std::string listen_j = ConfigUtils::getSimpleV(serveurs[j].directives, "listen");
            std::string name_j = "";

            if (hasDirective(serveurs[j].directives, "server_name"))
                name_j = getSimpleV(serveurs[j].directives, "server_name");
            if (listen_i == listen_j && name_i == name_j)
            {
                throw std::runtime_error(
                    "Duplicate server_name '" + name_i + "' on same listen '" + listen_i + "'");
            }
        }
    }
}
