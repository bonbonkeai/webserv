#ifndef RESPONSEBUILDER_HPP
#define RESPONSEBUILDER_HPP

#include <string>
#include "HTTP/hpp/HTTPResponse.hpp"
#include "HTTP/hpp/HTTPUtils.hpp" 

/*把 HTTPResponse 对象转成原始字节串
ex:

HTTP/1.1 200 OK\r\n
Content-Length: 123\r\n
...\r\n
\r\n
<body>

//这就是写入 Client.writeBuffer 的内容。*/
class	ResponseBuilder
{
public:
		// ResponseBuilder();
		// ResponseBuilder(const ResponseBuilder& copy);
		// ResponseBuilder& operator=(const ResponseBuilder& copy);
		// ~ResponseBuilder();
		static std::string build(const HTTPResponse& resp);
private:
		static std::string buildStatusLine(const HTTPResponse &resp);
		static std::string buildHeaders(const HTTPResponse &resp);
		static std::string buildDataHeader();
};

#endif

