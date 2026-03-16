#ifndef DELETEREQUEST_HPP
#define DELETEREQUEST_HPP

#include "Method_Handle/hpp/IRequest.hpp"

class DeleteRequest : public IRequest
{
private:
        HTTPRequest _req;

public:
        DeleteRequest(const HTTPRequest& req);
        virtual ~DeleteRequest();

        virtual HTTPResponse handle();
};

#endif


/*处理 DELETE
删除文件
不允许 → error*/