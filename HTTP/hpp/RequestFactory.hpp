
#ifndef REQUESTFACTORY_HPP
#define REQUESTFACTORY_HPP

#include "HTTP/hpp/HTTPRequest.hpp"
#include "Method_Handle/hpp/IRequest.hpp"

class RequestFactory
{
public:
    static IRequest* create(const HTTPRequest& req);
};

#endif

/*根据 HTTPRequest.method 返回正确的类型：
GET → GetRequest
POST → PostRequest
DELETE → DeleteRequest
否则 → ErrorRequest

用于把协议层(C)连接到业务层(Handle)。*/



