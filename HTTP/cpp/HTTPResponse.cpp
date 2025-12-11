#include "HTTP/hpp/HTTPResponse.hpp"

HTTPResponse::HTTPResponse() : statusCode(200), statusText("OK") {}
		
HTTPResponse::HTTPResponse(const HTTPResponse& copy) : 
				statusCode(copy.statusCode),
				statusText(copy.statusText),
				headers(copy.headers),
				body(copy.body)
				{}

		
HTTPResponse& HTTPResponse::operator=(const HTTPResponse& copy)
{
	if (this != &copy)
	{
		statusCode = copy.statusCode;
		statusText = copy.statusText;
		headers = copy.headers;
		body = copy.body;
	}
	return (*this);
}
		
HTTPResponse::~HTTPResponse() {}

void HTTPResponse::clear()
{
	statusCode = 200;
	statusText = "OK";
	headers.clear();
	body.clear();
}