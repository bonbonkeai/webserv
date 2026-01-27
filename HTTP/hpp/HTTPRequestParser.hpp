#ifndef HTTPREQUESTPARSER_HPP
#define HTTPREQUESTPARSER_HPP

#include "HTTP/hpp/HTTPRequest.hpp"
#include "HTTP/hpp/HTTPUtils.hpp" 
#include <sstream>
#include <algorithm>

/*解析请求首行 METHOD URI VERSION
解析 headers
保存 header map
判断是否 chunked
解码 chunked
读 Content-Length body
读到完整请求后生成 HTTPRequest 对象

ex:
POST /upload?folder=images HTTP/1.1
Host: localhost:8080
Content-Type: multipart/form-data; boundary=XYZ
Content-Length: 345

<...binary data...>

HTTPParser 做的事：
拆成：
method = POST
path = "/upload"
query="folder=images"
version="HTTP/1.1"
headers["Content-Type"]="multipart/form-data; boundary=XYZ"
body = 345 字节
所有内容存在 HTTPRequest 对象里。

Handle 做的事：
由 PostRequest.handle() 决定：
这个 path 是否允许 POST
是否有 upload_path
是否需要 multipart parser
是否触发 CGI
是否返回错误
HTTPRequest 本身不会做决定,只存数据*/


enum ParseState
{
	WAIT_REQUEST_LINE,
	WAIT_HEADERS,
	WAIT_BODY,
	PARSE_DONE,
	WAIT_RESPONSE,
	CLOSE
};

class HTTPRequestParser
{
public:
		HTTPRequestParser();
		HTTPRequestParser(const HTTPRequestParser& copy);
		HTTPRequestParser& operator=(const HTTPRequestParser& copy);
		~HTTPRequestParser();

		const HTTPRequest&	getRequest() const;
		bool	dejaParse(const std::string &newData);
		void	reset();
		bool	is_empty();

private:
		HTTPRequest	_req;
		ParseState	_state;
		std::string	_buffer;

		//extraire path/query
		void	splitUri();
		bool	parseRequestLine();
		bool	parseHeaders();
		bool	parseBody();
		bool	isValidHeaderName(const std::string& key);
		// bool	parseContentLengthStrict(const std::string& v, std::size_t& out, std::size_t max_body);
		int		parseContentLengthStrict(const std::string& v, std::size_t& out, std::size_t max_body);
		bool	fail(int code);

		bool parseChunkedBody();
		bool parseFixedBody();
		// chunked decoding state
		bool        _chunk_waiting_size;   // true: 等 size 行；false: 等 data + CRLF
		std::size_t _chunk_expected_size;  // 当前 chunk 的大小

};

#endif