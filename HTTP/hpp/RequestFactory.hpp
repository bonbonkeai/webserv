#ifndef RESQUESTFACTORY_HPP
#define RESQUESTFACTORY_HPP

#include "HTTP/hpp/HTTPRequest.hpp"
#include "Method_Handle/hpp/IRequest.hpp"
#include "Method_Handle/hpp/GetRequest.hpp"
#include "Method_Handle/hpp/ErrorRequest.hpp"
#include "Method_Handle/hpp/PostRequest.hpp"
#include "Method_Handle/hpp/DeleteRequest.hpp"

class RequestFactory
{
public:
		RequestFactory();
		RequestFactory(const RequestFactory& copy);
		RequestFactory& operator=(const RequestFactory& copy);
		~RequestFactory();

		static IRequest* create(const HTTPRequest& req);
};


/*根据 HTTPRequest.method 返回正确的类型：
GET → GetRequest
POST → PostRequest
DELETE → DeleteRequest
否则 → ErrorRequest

用于把协议层(C)连接到业务层(Handle)。*/


#endif


