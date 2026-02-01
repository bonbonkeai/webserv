#ifndef IREQUEST_HPP
#define IREQUEST_HPP

#include "HTTP/hpp/HTTPRequest.hpp"
#include "HTTP/hpp/HTTPResponse.hpp"
#include "Config/hpp/EffectiveConfig.hpp"

/*业务处理的统一接口：
virtual void handle(const EffectiveConfig&, Client&) = 0;
所有请求类都会实现它->get/post/delete。*/

class	IRequest
{
public:
		virtual ~IRequest() {}
		virtual HTTPResponse handle(const EffectiveConfig& cfg) = 0;
};



#endif
