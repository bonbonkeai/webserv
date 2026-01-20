#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>
#include <sstream>

/*与Request对应的响应数据结构
statusCode
reasonPhrase
headers
body*/

class HTTPResponse
{
public:
		HTTPResponse();
		HTTPResponse(const HTTPResponse& copy);
		HTTPResponse& operator=(const HTTPResponse& copy);
		~HTTPResponse();

		/*response data*/
		int statusCode;
		std::string statusText;
		std::map<std::string, std::string> headers;
		std::string body;

		void clear();
		HTTPResponse buildResponseFromCGIOutput(const std::string& out, bool keep_alive);


};





#endif