#ifndef GETREQUEST_HPP
#define GETREQUEST_HPP

#include "Method_Handle/hpp/IRequest.hpp"

class   GetRequest : public IRequest
{
private:
        HTTPRequest _req;
public:
        GetRequest(const HTTPRequest& req);
        virtual ~GetRequest();
        virtual HTTPResponse handle();
};

#endif


/*处理 GET 请求：
是否为重定向(return)
是否为CGI(由扩展名决定)
是否为目录(调用 DirectoryHandler)
是否是普通文件(调用 StaticHandle)
不存在 → ErrorRequest*/