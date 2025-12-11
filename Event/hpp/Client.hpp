#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include "HTTP/hpp/HTTPRequest.hpp"
#include "HTTP/hpp/HTTPRequestParser.hpp"
#include "HTTP/hpp/HEETResponse.hpp"

enum	ClienState
{
	CLIENT_READING,
	CLIENT_READY,
	CLIENT_WRITING,
	CLIENT_CLOSED
};

class	Client
{
	
}

/*表示一个客户端连接：
成员包括：
readBuffer / writeBuffer
HTTPRequestParser 实例
是否 headerComplete / bodyComplete
keepAlive 状态
lastActive 时间戳
与 CGI 交互时的 cgiFd*/


#endif