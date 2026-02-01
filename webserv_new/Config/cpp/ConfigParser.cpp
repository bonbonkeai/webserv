#include "Config/hpp/ConfigParser.hpp"
#include "Config/hpp/ConfigUtils.hpp"
#include "HTTP/hpp/HTTPUtils.hpp" 
#include <stdexcept>

ConfigParser::ConfigParser(const std::vector<Token>& tokens)
    : _tokens(tokens), _pos(0) {}

// -------------------
// Helpers
// -------------------
const Token& ConfigParser::current() const
{
    if (_pos >= _tokens.size())
        return _tokens.back();
    return _tokens[_pos];
}

const Token& ConfigParser::next()
{
    if (_pos < _tokens.size())
        _pos++;
    return current();
}

void ConfigParser::expect(Tokentype type, const std::string& err_msg)
{
    if (current().type != type)
    {
        throw std::runtime_error(
            err_msg + " at line " + toString(current().line) +
            " col " + toString(current().col)
        );

    }
    next();
}

// -------------------
// Parse server
// -------------------
ServerConfig ConfigParser::parse_server()
{
    ServerConfig server;

    if (current().type != TYPE_WORD || current().value != "server")
        throw std::runtime_error("'server' keyword expected at line " + toString(current().line));
    next();
    expect(TYPE_L_CURLY, "Expected '{' after server");

    while (current().type != TYPE_R_CURLY && current().type != TYPE_EOF)
    {
        if (current().type == TYPE_WORD && current().value == "location")
        {
            LocationConfig loc = parse_location();
            server.locations.push_back(loc);
        }
        else
        {
            parse_directives(server.directives);
        }
    }

    expect(TYPE_R_CURLY, "Expected '}' at the end of server");
    return server;
}

// -------------------
// Parse location
// -------------------
LocationConfig ConfigParser::parse_location()
{
    LocationConfig loc;

    expect(TYPE_WORD, "Expected 'location'");
    next();

    // Path
    if (current().type != TYPE_WORD)
        throw std::runtime_error("Expected path after location at line " + toString(current().line));
    loc.path = current().value;
    next();

    expect(TYPE_L_CURLY, "Expected '{' after location path");

    // Directives inside location
    while (current().type != TYPE_R_CURLY && current().type != TYPE_EOF)
    {
        parse_directives(loc.directives);
    }

    expect(TYPE_R_CURLY, "Expected '}' at the end of location");
    return loc;
}

// -------------------
// Parse generic directives
// -------------------
void ConfigParser::parse_directives(std::map<std::string, std::vector<std::string> >& directives)
{
    if (current().type != TYPE_WORD)
        throw std::runtime_error("Expected directive name at line " + toString(current().line));

    std::string key = current().value;
    next();

    std::vector<std::string> values;

    while (current().type == TYPE_WORD || current().type == TYPE_NUM)
    {
        values.push_back(current().value);
        next();
    }

    expect(TYPE_SEMICOLON, "Expected ';' after directive " + key);
    directives[key] = values;
}

std::vector<ServerConfig> ConfigParser::parse()
{
    std::vector<ServerConfig> servers;

    while (current().type != TYPE_EOF)
    {
        if (current().type == TYPE_WORD && current().value == "server")
        {
            servers.push_back(parse_server());
        }
        else
        {
            throw std::runtime_error("Unexpected token '" + current().value + "' at line " + toString(current().line));
        }
    }

    if (servers.empty())
        throw std::runtime_error("No server defined in configuration");

    return servers;
}
