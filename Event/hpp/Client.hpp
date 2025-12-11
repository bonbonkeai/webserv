#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include "HTTP/hpp/HTTPRequest.hpp"
#include "HTTP/hpp/HTTPRequestParser.hpp"
#include "HTTP/hpp/HEETResponse.hpp"

/*表示一个客户端连接：
成员包括：
readBuffer / writeBuffer
HTTPRequestParser 实例
是否 headerComplete / bodyComplete
keepAlive 状态
lastActive 时间戳
与 CGI 交互时的 cgiFd*/


enum	ClientState
{
	CLIENT_READING,
	CLIENT_READY,
	CLIENT_WRITING,
	CLIENT_CLOSED
};

class	Client
{
public:
		Client();
		Client(int fd);
		Client(const Client& copy);
		Client& operator=(const Client& copy);
		~Client();

		int getFd() const;
		bool isClosed() const;
		bool isReady() const;

		/*I/O*/
		void readBuffer();
		void writeBuffer();

		/*state control*/
		void markClosed();
		void restForNextReq() const;

		/*access parsed data*/
		const HTTPRequest& getRequest() const;

		/*set response*/
		void setResponse(const HTTPResponse& resq);
private:
		int _clientFd;
		ClientState _state;
		/*parse http*/
		HTTPRequestParser _httpParser;
		bool	_reqComplet;
		/*response buffer*/
		std::string	_respBuffer;
		size_t	_respSent;
};



#endif