#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

/*纯数据结构：
method
path
query
version
headers
body

//不会包含业务逻辑，只负责存储解析后的结果。
//只是一个容器存HTTP Parser解析后的结果
*/


class HTTPRequest
{
public:
        HTTPRequest();
        HTTPRequest(const HTTPRequest& copy);
        HTTPRequest& operator=(const HTTPRequest& copy);
        ~HTTPRequest();

        std::string method;
        std::string uri;//Comprendre le nom de web
        std::string path;//seulement path 
        std::string query;
        std::string version;
        std::string body;
        std::map<std::string, std::string> headers;

        size_t  contentLength;
        bool    has_body;
        bool    complet;
        bool    bad_request;
        bool    keep_alive;
};

#endif