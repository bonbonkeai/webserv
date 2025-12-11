#ifndef IREQUEST_HPP
#define IREQUEST_HPP

#include "HTTP/hpp/HTTPRequest.hpp"
#include "HTTP/hpp/HEETResponse.hpp"

/*业务处理的统一接口：
virtual void handle(const EffectiveConfig&, Client&) = 0;
所有请求类都会实现它->get/post/delete。*/

class	Irequest
{
public:
		virtual ~Irequest() {}
		virtual HTTPResponse handle() = 0;
};



#endif
