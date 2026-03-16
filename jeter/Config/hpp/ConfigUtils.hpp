#ifndef CONFIGUTILS_HPP
#define CONFIGUTILS_HPP

#include <string>
#include <cstdlib> // for atoi
#include <cctype>  // for std::isdigit
#include "HTTP/hpp/HTTPResponse.hpp"
#include "Config/hpp/ConfigParser.hpp"
#include "Config/hpp/ConfigTokenizer.hpp"
#include "Config/hpp/ServerConfig.hpp"

class ConfigUtils
{

private:
public:
    ConfigUtils();
    ~ConfigUtils();

    static int toInt(const std::string &str);
    static size_t toSize(const std::string &str);
    static bool toBool(const std::string &str);

    static bool hasDirective(const std::map<std::string, std::vector<std::string> > &d, const std::string &cle);
    static std::string getSimpleV(const std::map<std::string, std::vector<std::string> > &d, const std::string &cle);
    static std::vector<std::string> getV(const std::map<std::string, std::vector<std::string> > &d, const std::string &cle);
    static std::string getValue(const std::map<std::string, std::vector<std::string> > &d, const std::string &cle);

    void validate(std::vector<ServerConfig> &serveurs);
    void validateS(ServerConfig &serveurs);
    void validateL(ServerConfig &serveurs, LocationConfig &location);
};
#endif
/*
存放辅助函数：
字符串 trim、split
路径拼接
解析端口 / IP
转换数字单位 如 1M → 1,048,576 bytes
检查配置信息合法性*/
