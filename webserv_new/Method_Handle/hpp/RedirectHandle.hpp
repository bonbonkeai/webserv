
/*处理：
return 301 URL
return 302 URL
设置：
Location header
body


->
当配置文件的某个location指定了return指令时
服务器必须返回一个 HTTP 重定向响应。
因为：网站迁移时通常需要把旧路径 /old 指向新路径 /new->告诉浏览器 “以后访问新地址”
ex:
location /old 
{
    return 301 http://example.com/new;
}
当客户端访问/old 时，wevserv必须返回一个标准的301HTTP重定向响应:
HTTP/1.1 301 Moved Permanently
Location: http://example.com/new
Content-Length: ...
Content-Type: text/html

<html>....</html>


return->这里意味着：
不处理文件
不进入 CGI
不进入静态文件流程
直接返回->重定向响应*/

#ifndef REDIRECTHANDLE_HPP
#define REDIRECTHANDLE_HPP

#include <string>
#include "HTTP/hpp/HTTPResponse.hpp"
#include "HTTP/hpp/HTTPRequest.hpp"

class RedirectHandle
{
public:
        static HTTPResponse buildRedirect(const HTTPRequest& req, int code, const std::string& location);
};

#endif
