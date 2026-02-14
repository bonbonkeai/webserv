#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include <cstddef>
#include "Config/hpp/EffectiveConfig.hpp"

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

enum Routeaction
{
        ACTION_REDIRECT,
        ACTION_CGI,
        ACTION_AUTOINDEX,
        ACTION_UPLOAD,
        ACTION_STATIC,
};
struct RouteResult
{
        Routeaction action;
        std::string fs_path; 

        int redirect_code;
        std::string redirect_url;
        //cgi
        std::string script_name;
        std::string path_info;
};

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

        bool    has_content_length;

        int     error_code;

        bool    has_transfer_encoding;
        bool    chunked;
        std::size_t max_body_size; //先写死，后面接入config
        bool    is_cgi_request() const;

        std::string authority;

        //Config
	EffectiveConfig effective;
	bool	has_effective;

        // route result
        RouteResult _rout;
};

#endif