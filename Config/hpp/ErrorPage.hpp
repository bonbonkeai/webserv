#ifndef ERRORPAGE_HPP
#define ERRORPAGE_HPP

#include <string>
#include "HTTP/hpp/HTTPResponse.hpp"
#include "Config/hpp/ConfigParser.hpp"
#include "Config/hpp/ConfigTokenizer.hpp"
#include "Config/hpp/LocationConfig.hpp"
#include "Config/hpp/ServerConfig.hpp"

class ErrorPage
{
    private:
        static std::string load_error_file(const std::string& path);
        static std::string get_error_page_path(int status, const ServerConfig& server,LocationConfig* location);
        static std::string default_error_page(int status);
    public:
        ErrorPage(){};
        ~ErrorPage(){};
        HTTPResponse generate(int status, const ServerConfig& s, LocationConfig* l);
};



#endif
/*
统一管理错误页：
找到对应的 error_page 文件 → 读入内容
如果没有定义，生成默认 HTML
返回一个 HTTPResponse 给 C/B 使用
避免所有 Request handler 都重复写错误页逻辑。*/
