#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <string>
#include <vector>
#include "Config/hpp/ConfigTokenizer.hpp"
#include "LocationConfig.hpp"
#include "ServerConfig.hpp"

class ConfigParser
{
private:
    const std::vector<Token>& _tokens;
    size_t _pos;
    const Token& current() const;
    const Token& next();
    void expect(Tokentype type, const std::string& err_msg);
    ServerConfig parse_server();
    LocationConfig parse_location();

public:
    ConfigParser(const std::vector<Token>& tokens);
    ~ConfigParser(){};
    void parse_directives(std::map<std::string, std::vector<std::string> >& directives);
    std::vector<ServerConfig> parse();
};

#endif


/*读取配置文件

词法分析 tokenize
语法分析（解析 server { ... } 结构）
构造：
ServerConfig
LocationConfig
检查语法错误、缺失字段

最后得到std::vector<ServerConfig> servers;*/
