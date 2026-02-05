
/*统一错误处理入口：
调用 A 的 ErrorPageManager
输出 HTML 错误页*/

#ifndef ERRORREQUEST_HPP
#define ERRORREQUEST_HPP

#include "Method_Handle/hpp/IRequest.hpp"

class   ErrorRequest : public IRequest
{
private:
        HTTPRequest _req;
        int _code;

public:
        ErrorRequest(const HTTPRequest& req, int code);
        virtual ~ErrorRequest();
        virtual HTTPResponse handle();
};

#endif
